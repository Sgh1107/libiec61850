/*
 *  server_example_setpoint.c
 *
 *  ------------------------------------------------------------------
 *  专用于"遥调（模拟量设定值调节）"测试的服务端示例。
 *
 *  数据模型见同目录 setpoint_tests.cid（改完模型用 model_generator 重新生成
 *  static_model.c/h 即可，不要手改生成文件）。
 *
 *  模型要点：
 *    GenericIO/GGIO1.AnOut1    APC  遥调设定值（direct-with-enhanced-security）
 *    GenericIO/GGIO1.AnOut1Fb  MV   模拟的实际输出，跟随设定值
 *    GenericIO/GGIO1.AnOut2    APC  第二路遥调通道
 *    GenericIO/GGIO1.AnOut2Fb  MV   第二路实际输出
 *
 *  四个点都在数据集 SetpointEvents 里，客户端订阅报告即可看到变化；
 *  增强型安全模型（ctlModel=3）还会带回 CommandTermination 回执。
 *
 *  报告相关（对照 setpoint 报告侧改动要求）：
 *    RCB 引用     setpointIOGenericIO/LLN0.RP.SetpointRCB
 *                 （FC 段 .RP. 不能省，库按分隔符后首字符判缓冲属性）
 *    数据集       GenericIO/LLN0.SetpointEvents，4 个 FLOAT32 叶子，顺序即报告元素顺序
 *    TrgOps       0x1F（dchg/qchg/dupd/integ/gi 全开，缺 gi 则全询被静默吞掉）
 *    OptFields    0xEF = seqNum + timeStamp + reasonCode + dataSet + entryID + configRef
 *                 （bufOvfl 由生成器默认置位；dataRef 是可选项，默认不加，
 *                  需要自描述报告时在 CID 的 OptFields 里打开）
 *    周期          IntgPd=1000，需 TrgOps 的 integ 位才排程
 *    触发方式      所有值变化必须走 IedServer_update* 接口，直接改 static_model 内存不发报告
 * ------------------------------------------------------------------
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "static_model.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* 中文输出：源文件为 UTF-8 with BOM，控制台代码页切到 UTF-8 */
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

/* 遥调量程，超限拒绝 */
#define SETPOINT_MIN 0.f
#define SETPOINT_MAX 1000.f

/*
 * 一次遥调把反馈量分几步走到位。
 *
 * 每一步都是一次数据变化 -> 一条变位报告，所以这个值直接决定"一次操作产生几条报告"。
 * 6 对应"一次遥调 6~8 条报告"的正常现象，顺序号连续递增，不是重复上报。
 * 设成 1 就是一步到位：一次操作只对应一条变位报告。
 *
 * 注意：早期版本用的是"每周期逼近剩余偏差的一个比例"（OUTPUT_FOLLOW_RATE=0.2），
 * 那个算法在 0.01 的收敛门限下实际要约 48 个周期才停，会产生几十条报告。
 * 改成固定步数后报告条数是确定的，便于对端按顺序号核对。
 */
#define OUTPUT_FOLLOW_STEPS 6

/* 控制周期 */
#define CONTROL_CYCLE_MS 100

typedef struct {
    const char* name;               /* 显示名，如 "GGIO1.AnOut1" */
    DataAttribute* setpoint;        /* mxVal.f  (MX) */
    DataAttribute* setpointTime;    /* t        (MX) */
    DataAttribute* feedback;        /* Fb.mag.f (MX) */
    DataAttribute* feedbackTime;    /* Fb.t     (MX) */
    float target;                   /* 最近一次遥调目标 */
    float output;                   /* 模拟实际输出 */
    float stepSize;                 /* 本次遥调每步的增量 */
    int stepsRemaining;             /* 还剩几步到位（0 表示已到位） */
    bool followed;                  /* 是否已跟到目标 */
} SetpointChannel;

static int running = 0;
static IedServer iedServer = NULL;
static uint64_t startTime = 0;

static unsigned int originalConsoleOutputCP = 0;
static unsigned int originalConsoleInputCP = 0;

static SetpointChannel channels[2];

static void
initConsoleEncoding(void)
{
#ifdef _WIN32
    originalConsoleOutputCP = GetConsoleOutputCP();
    originalConsoleInputCP = GetConsoleCP();

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static void
restoreConsoleEncoding(void)
{
#ifdef _WIN32
    if (originalConsoleOutputCP != 0)
        SetConsoleOutputCP(originalConsoleOutputCP);

    if (originalConsoleInputCP != 0)
        SetConsoleCP(originalConsoleInputCP);
#endif
}

static double
elapsedSeconds(void)
{
    if (startTime == 0)
        return 0.0;

    return (Hal_getTimeInMs() - startTime) / 1000.0;
}

static float
getAttributeFloat(DataAttribute* attr)
{
    MmsValue* value = IedServer_getAttributeValue(iedServer, attr);

    if ((value != NULL) && (MmsValue_getType(value) == MMS_FLOAT))
        return MmsValue_toFloat(value);

    return 0.f;
}

void
sigint_handler(int signalId)
{
    running = 0;
}

/*
 * APC 的控制处理函数。
 *
 * 注意：APC 的 ctlVal 是 AnalogueValue 结构体（含 f / i），服务器传给 handler
 * 的是整个结构而不是裸 FLOAT32，必须先取第 0 个元素。
 */
static ControlHandlerResult
controlHandlerForSetpoint(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    SetpointChannel* channel = (SetpointChannel*) parameter;
    uint64_t timestamp = Hal_getTimeInMs();
    ClientConnection clientCon = ControlAction_getClientConnection(action);
    const char* peer = (clientCon != NULL) ? ClientConnection_getPeerAddress(clientCon) : "(local)";

    MmsValue* ctlVal = value;

    if (MmsValue_getType(value) == MMS_STRUCTURE)
        ctlVal = MmsValue_getElement(value, 0);

    if ((ctlVal == NULL) || (MmsValue_getType(ctlVal) != MMS_FLOAT)) {
        printf("[%7.2fs] [CTRL] %s ctlVal 类型错误，拒绝\n", elapsedSeconds(), channel->name);
        fflush(stdout);
        return CONTROL_RESULT_FAILED;
    }

    float newValue = MmsValue_toFloat(ctlVal);
    float oldValue = getAttributeFloat(channel->setpoint);

    printf("[%7.2fs] [CTRL] 来自 %s 遥调 %s: %.2f -> %.2f  (ctlNum=%i%s)\n",
            elapsedSeconds(), peer, channel->name, oldValue, newValue,
            ControlAction_getCtlNum(action),
            test ? ", Test" : "");

    if ((newValue < SETPOINT_MIN) || (newValue > SETPOINT_MAX)) {
        printf("           ↳ 超出量程 [%.0f, %.0f]，拒绝\n", SETPOINT_MIN, SETPOINT_MAX);

        /* 把拒绝原因随 CommandTermination- / LastApplError 一起带回客户端 */
        ControlAction_setAddCause(action, ADD_CAUSE_INVALID_POSITION);

        fflush(stdout);
        return CONTROL_RESULT_FAILED;
    }

    /* 两个属性必须在同一把模型锁里更新，否则报告线程可能在两次更新之间取样，
       发出"新值 + 旧时标"的组合；与 updateOutputs() 的做法保持一致。
       跟随状态（target/stepSize/stepsRemaining）也用同一把锁保护，
       避免控制线程与主循环同时改这组字段。 */
    IedServer_lockDataModel(iedServer);

    IedServer_updateFloatAttributeValue(iedServer, channel->setpoint, newValue);
    IedServer_updateUTCTimeAttributeValue(iedServer, channel->setpointTime, timestamp);

    channel->target = newValue;

    if (newValue != channel->output) {
        channel->stepSize = (newValue - channel->output) / (float) OUTPUT_FOLLOW_STEPS;
        channel->stepsRemaining = OUTPUT_FOLLOW_STEPS;
        channel->followed = false;
    }
    else {
        channel->stepSize = 0.f;
        channel->stepsRemaining = 0;
        channel->followed = true;
    }

    IedServer_unlockDataModel(iedServer);

    fflush(stdout);

    return CONTROL_RESULT_OK;
}

/*
 * 每周期把实际输出往设定值推进一步，模拟 AO 通道的响应过程。
 *
 * 所有 update 都走 IedServer_update* 接口：1.6 的变位检测只在这些接口内部登记，
 * 直接改 static_model 里的值内存不会产生报告。
 * 一整组更新包在 lockDataModel/unlock 之间，报告线程不会取到半截状态。
 */
static void
updateOutputs(void)
{
    int i;

    IedServer_lockDataModel(iedServer);

    for (i = 0; i < 2; i++) {
        SetpointChannel* channel = &channels[i];

        if (channel->stepsRemaining > 0) {
            channel->output += channel->stepSize;
            channel->stepsRemaining--;

            /* 最后一步直接对齐目标，避免浮点累积误差 */
            if (channel->stepsRemaining == 0)
                channel->output = channel->target;

            IedServer_updateFloatAttributeValue(iedServer, channel->feedback, channel->output);
            IedServer_updateUTCTimeAttributeValue(iedServer, channel->feedbackTime, Hal_getTimeInMs());
        }
    }

    IedServer_unlockDataModel(iedServer);

    for (i = 0; i < 2; i++) {
        SetpointChannel* channel = &channels[i];

        if (!channel->followed && (channel->stepsRemaining == 0)) {
            printf("[%7.2fs] [FOLLOW] %s 实际输出已跟随到 %.2f\n",
                    elapsedSeconds(), channel->name, channel->output);

            channel->followed = true;
        }
    }

    fflush(stdout);
}

/*
 * 报告控制块事件回调。
 *
 * 没有这层输出时，"启用失败"和"启用成功但没发报告"在服务端看起来完全一样。
 * 判读要点：
 *   RESERVED -> ENABLE -> GI -> REPORT_CREATED   全询路径完全正常
 *   RESERVED -> ENABLE -> GI，之后没有 REPORT_CREATED
 *                                               模型的 TrgOps 没有 gi 位
 *   只有 SET_PARAMETER 且 err != 0               属性被拒（可写位/位值超模型声明）
 *   有 RESERVED 但没有 ENABLE                    上一个客户端没释放预留
 *   值变了但没有 REPORT_CREATED                  值不是通过 IedServer_update* 改的
 */
static const char*
rcbEventName(IedServer_RCBEventType event)
{
    switch (event) {
    case RCB_EVENT_GET_PARAMETER:   return "GET_PARAM";
    case RCB_EVENT_SET_PARAMETER:   return "SET_PARAM";
    case RCB_EVENT_UNRESERVED:      return "UNRESERVED";
    case RCB_EVENT_RESERVED:        return "RESERVED";
    case RCB_EVENT_ENABLE:          return "ENABLE";
    case RCB_EVENT_DISABLE:         return "DISABLE";
    case RCB_EVENT_GI:              return "GI";
    case RCB_EVENT_PURGEBUF:        return "PURGEBUF";
    case RCB_EVENT_OVERFLOW:        return "OVERFLOW";
    case RCB_EVENT_REPORT_CREATED:  return "REPORT_CREATED";
    default:                        return "UNKNOWN";
    }
}

static void
rcbEventHandler(void* parameter, ReportControlBlock* rcb, ClientConnection con,
        IedServer_RCBEventType event, const char* parameterName, MmsDataAccessError serviceError)
{
    char errorText[16];

    /* 注意 MmsDataAccessError 是负数枚举：-1 = 成功、-3 = 成功但值未变化，
       只有 >= 0 才是真正的错误码。这里直接翻成人话，免得把成功误读成"err!=0"。 */
    if ((int) serviceError < 0)
        snprintf(errorText, sizeof(errorText), "OK");
    else
        snprintf(errorText, sizeof(errorText), "%d", (int) serviceError);

    printf("[%7.2fs] [RCB] event=%-14s(%2d) rcb=%s param=%s client=%s err=%s\n",
            elapsedSeconds(), rcbEventName(event), (int) event,
            ReportControlBlock_getName(rcb),
            (parameterName != NULL) ? parameterName : "-",
            (con != NULL) ? ClientConnection_getPeerAddress(con) : "(null)",
            errorText);

    fflush(stdout);
}

/*
 * 连接断开时库会自动把该连接持有的非缓冲 RCB 置 RptEna=false 并释放预留
 * （mms_mapping.c 的 Reporting_deactivateReportsForConnection）。
 * 这里只把事件打出来，便于确认"杀掉客户端 -> 立刻重连 -> 能重新启用"这条路径。
 */
static void
connectionIndicationHandler(IedServer server, ClientConnection connection, bool connected, void* parameter)
{
    printf("[%7.2fs] [CONN] %s %s\n",
            elapsedSeconds(),
            connected ? "connected   " : "disconnected",
            ClientConnection_getPeerAddress(connection));

    fflush(stdout);
}

static void
printMenu(int tcpPort)
{
    printf("\n");
    printf("=============================================================\n");
    printf(" 遥调测试服务端已启动，端口 %d\n", tcpPort);
    printf("-------------------------------------------------------------\n");
    printf(" 遥调目标（客户端 operate 下发）:\n");
    printf("   GenericIO/GGIO1.AnOut1    APC  range %.0f ~ %.0f\n", SETPOINT_MIN, SETPOINT_MAX);
    printf("   GenericIO/GGIO1.AnOut2    APC  range %.0f ~ %.0f\n", SETPOINT_MIN, SETPOINT_MAX);
    printf(" 实际输出（服务端自动跟随）:\n");
    printf("   GenericIO/GGIO1.AnOut1Fb  MV\n");
    printf("   GenericIO/GGIO1.AnOut2Fb  MV\n");
    printf("-------------------------------------------------------------\n");
    printf(" ctlModel = direct-with-enhanced-security（无需 select，直接 operate）\n");
    printf(" 一次遥调分 %d 步到位 -> 约 %d 条变位报告（顺序号连续，不是重复上报）\n",
            OUTPUT_FOLLOW_STEPS, OUTPUT_FOLLOW_STEPS);
    printf(" 数据集 SetpointEvents 已包含上述 4 个点，可订阅报告观察变化\n");
    printf("-------------------------------------------------------------\n");
    printf(" 报告控制块（非缓冲 URCB）:\n");
    printf("   setpointIOGenericIO/LLN0.RP.SetpointRCB\n");
    printf("   数据集 LLN0$SetpointEvents（4 个 FLOAT32 叶子）\n");
    printf("   TrgOps 0x1F(dchg/qchg/dupd/integ/gi)  OptFields 0xFF  IntgPd 1000ms\n");
    printf("   启用规范的顺序：同一次 Set 里先 RptEna=true，再 GI=true\n");
    printf("   服务端会把 RCB 事件（RESERVED/ENABLE/GI/REPORT_CREATED...）打到下面\n");
    printf("-------------------------------------------------------------\n");
    printf(" Ctrl+C 退出\n");
    printf("=============================================================\n\n");
    fflush(stdout);
}

int
main(int argc, char** argv)
{
    int tcpPort = 102;
    int i;

    initConsoleEncoding();

    /* 输出重定向到文件时也要能实时看到日志（必须在任何输出之前设置） */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 时间戳基准提到 start 之前，连接/RCB 事件从第一刻就带正确时间 */
    startTime = Hal_getTimeInMs();

    if (argc > 1)
        tcpPort = atoi(argv[1]);

    iedServer = IedServer_create(&iedModel);

    /* 通道 1 */
    channels[0].name = "GGIO1.AnOut1";
    channels[0].setpoint = IEDMODEL_GenericIO_GGIO1_AnOut1_mxVal_f;
    channels[0].setpointTime = IEDMODEL_GenericIO_GGIO1_AnOut1_t;
    channels[0].feedback = IEDMODEL_GenericIO_GGIO1_AnOut1Fb_mag_f;
    channels[0].feedbackTime = IEDMODEL_GenericIO_GGIO1_AnOut1Fb_t;

    /* 通道 2 */
    channels[1].name = "GGIO1.AnOut2";
    channels[1].setpoint = IEDMODEL_GenericIO_GGIO1_AnOut2_mxVal_f;
    channels[1].setpointTime = IEDMODEL_GenericIO_GGIO1_AnOut2_t;
    channels[1].feedback = IEDMODEL_GenericIO_GGIO1_AnOut2Fb_mag_f;
    channels[1].feedbackTime = IEDMODEL_GenericIO_GGIO1_AnOut2Fb_t;

    for (i = 0; i < 2; i++) {
        channels[i].target = 0.f;
        channels[i].output = 0.f;
        channels[i].stepSize = 0.f;
        channels[i].stepsRemaining = 0;
        channels[i].followed = true;
    }

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_AnOut1,
            controlHandlerForSetpoint, &channels[0]);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_AnOut2,
            controlHandlerForSetpoint, &channels[1]);

    /*
     * IedServer_create(&iedModel) 不传配置文件时，RCB 属性默认可写六项全开
     * （RPT_ID | BUF_TIME | DATSET | TRG_OPS | OPT_FIELDS | INTG_PD），
     * 客户端因此能自行改 TrgOps / OptFields / IntgPd 并写 RptEna、GI、Resv。
     * 若将来改成 createWithConfig，必须显式把这六项设回可写，否则"改触发条件"永远 ACCESS_DENIED。
     */

    /* 报告/连接事件回调必须在 start 之前注册，否则会漏掉最早的几条事件 */
    IedServer_setRCBEventHandler(iedServer, rcbEventHandler, NULL);
    IedServer_setConnectionIndicationHandler(iedServer, connectionIndicationHandler, NULL);

    IedServer_start(iedServer, tcpPort);

    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        restoreConsoleEncoding();
        exit(-1);
    }

    running = 1;

    printMenu(tcpPort);

    signal(SIGINT, sigint_handler);

    while (running) {
        updateOutputs();

        Thread_sleep(CONTROL_CYCLE_MS);
    }

    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);

    restoreConsoleEncoding();

    return 0;
} /* main() */
