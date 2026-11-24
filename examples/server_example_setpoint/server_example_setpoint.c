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
 *  ------------------------------------------------------------------
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
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

/* 实际输出跟随速率（每周期逼近剩余偏差的比例） */
#define OUTPUT_FOLLOW_RATE 0.2f
#define OUTPUT_FOLLOW_EPS  0.01f

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

    IedServer_updateFloatAttributeValue(iedServer, channel->setpoint, newValue);
    IedServer_updateUTCTimeAttributeValue(iedServer, channel->setpointTime, timestamp);

    channel->target = newValue;
    channel->followed = false;

    fflush(stdout);

    return CONTROL_RESULT_OK;
}

/* 每周期让实际输出向设定值逼近一次，模拟 AO 通道的响应过程 */
static void
updateOutputs(void)
{
    int i;

    IedServer_lockDataModel(iedServer);

    for (i = 0; i < 2; i++) {
        SetpointChannel* channel = &channels[i];
        float delta = channel->target - channel->output;

        if (fabsf(delta) > OUTPUT_FOLLOW_EPS) {
            channel->output += delta * OUTPUT_FOLLOW_RATE;

            if (fabsf(channel->target - channel->output) <= OUTPUT_FOLLOW_EPS)
                channel->output = channel->target;

            IedServer_updateFloatAttributeValue(iedServer, channel->feedback, channel->output);
            IedServer_updateUTCTimeAttributeValue(iedServer, channel->feedbackTime, Hal_getTimeInMs());
        }
        else if (channel->output != channel->target) {
            channel->output = channel->target;

            IedServer_updateFloatAttributeValue(iedServer, channel->feedback, channel->output);
            IedServer_updateUTCTimeAttributeValue(iedServer, channel->feedbackTime, Hal_getTimeInMs());
        }
    }

    IedServer_unlockDataModel(iedServer);

    for (i = 0; i < 2; i++) {
        SetpointChannel* channel = &channels[i];

        if (!channel->followed && (channel->output == channel->target)) {
            printf("[%7.2fs] [FOLLOW] %s 实际输出已跟随到 %.2f\n",
                    elapsedSeconds(), channel->name, channel->output);

            channel->followed = true;
        }
    }

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
    printf(" 数据集 SetpointEvents 已包含上述 4 个点，可订阅报告观察变化\n");
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
        channels[i].followed = true;
    }

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_AnOut1,
            controlHandlerForSetpoint, &channels[0]);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_AnOut2,
            controlHandlerForSetpoint, &channels[1]);

    IedServer_start(iedServer, tcpPort);

    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        restoreConsoleEncoding();
        exit(-1);
    }

    running = 1;
    startTime = Hal_getTimeInMs();

    /* 输出重定向到文件时也要能实时看到日志 */
    setvbuf(stdout, NULL, _IONBF, 0);

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
