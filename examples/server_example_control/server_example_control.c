/*
 *  server_example_control.c
 *
 *  How to use the different control handlers (TBD)
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "static_model.h"

#ifdef _WIN32
#include <windows.h>
#endif

/*
 * 中文乱码修复说明：
 *   源文件是 UTF-8，而 Windows 控制台默认代码页是 936(GBK)，
 *   MSVC 又默认把窄字符串字面量按系统 ANSI 代码页编码 —— 两头不一致就会乱码。
 * 做法：
 *   1) 源文件保存为 UTF-8 with BOM，并用下面的 pragma 强制字面量按 UTF-8 编码；
 *   2) 程序启动时把控制台输入/输出代码页切到 65001(UTF-8)，退出时还原。
 */
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

static unsigned int originalConsoleOutputCP = 0;
static unsigned int originalConsoleInputCP = 0;

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

static int running = 0;
static IedServer iedServer = NULL;

/* ==================== 新增：DI/AI 变化监视 ==================== */

/*
 * 这里只负责检测 DI/AI 变化并在控制台打印，不再由服务器模拟。
 *
 * DI/AI 本身是只读的（ST/MX），客户端不能直接写：
 *   GGIO1.Ind1.stVal   FC=ST  BOOLEAN
 *   GGIO1.AnIn1.mag.f  FC=MX  FLOAT32
 *
 * 模型中为每路 DI/AI 增加了可写的 SP 设定值：
 *   GGIO1.Ind1.setVal   FC=SP  BOOLEAN
 *   GGIO1.AnIn1.setVal  FC=SP  FLOAT32
 *
 * 客户端写 setVal，服务器在写回调中把它同步到 stVal / mag.f，
 * 下面的轮询逻辑随后检测到变化并打印。
 */

/* 变化检测周期（毫秒） */
#define DI_AI_POLL_INTERVAL_MS 100

/* AI: GGIO1.AnIn1~4 (MX) */
static DataAttribute* anInValueAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_AnIn1_mag_f,
    IEDMODEL_GenericIO_GGIO1_AnIn2_mag_f,
    IEDMODEL_GenericIO_GGIO1_AnIn3_mag_f,
    IEDMODEL_GenericIO_GGIO1_AnIn4_mag_f
};

static DataAttribute* anInTimestampAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_AnIn1_t,
    IEDMODEL_GenericIO_GGIO1_AnIn2_t,
    IEDMODEL_GenericIO_GGIO1_AnIn3_t,
    IEDMODEL_GenericIO_GGIO1_AnIn4_t
};

/* AI setpoints: GGIO1.AnIn1~4.setVal (SP) */
static DataAttribute* anInSetValAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_AnIn1_setVal,
    IEDMODEL_GenericIO_GGIO1_AnIn2_setVal,
    IEDMODEL_GenericIO_GGIO1_AnIn3_setVal,
    IEDMODEL_GenericIO_GGIO1_AnIn4_setVal
};

/* DI: GGIO1.Ind1~4 (ST) */
static DataAttribute* indValueAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_Ind1_stVal,
    IEDMODEL_GenericIO_GGIO1_Ind2_stVal,
    IEDMODEL_GenericIO_GGIO1_Ind3_stVal,
    IEDMODEL_GenericIO_GGIO1_Ind4_stVal
};

static DataAttribute* indTimestampAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_Ind1_t,
    IEDMODEL_GenericIO_GGIO1_Ind2_t,
    IEDMODEL_GenericIO_GGIO1_Ind3_t,
    IEDMODEL_GenericIO_GGIO1_Ind4_t
};

/* DI setpoints: GGIO1.Ind1~4.setVal (SP) */
static DataAttribute* indSetValAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_Ind1_setVal,
    IEDMODEL_GenericIO_GGIO1_Ind2_setVal,
    IEDMODEL_GenericIO_GGIO1_Ind3_setVal,
    IEDMODEL_GenericIO_GGIO1_Ind4_setVal
};

typedef struct {
    int index;
    bool isAnalog;
} SetpointParameter;

static SetpointParameter anInSetpointParams[4];
static SetpointParameter indSetpointParams[4];

static float lastAnInValues[4];
static bool lastIndValues[4];
static bool firstDiAiPrint = true;
static uint64_t diAiStartTime = 0;

/* 读取当前 AI 值（模型里尚未赋值时返回 0） */
static float
getAnInValue(DataAttribute* dataAttribute)
{
    MmsValue* value = IedServer_getAttributeValue(iedServer, dataAttribute);

    if ((value != NULL) && (MmsValue_getType(value) == MMS_FLOAT))
        return MmsValue_toFloat(value);

    return 0.f;
}

/* 读取当前 DI 值（模型里尚未赋值时返回 false） */
static bool
getIndValue(DataAttribute* dataAttribute)
{
    MmsValue* value = IedServer_getAttributeValue(iedServer, dataAttribute);

    if ((value != NULL) && (MmsValue_getType(value) == MMS_BOOLEAN))
        return MmsValue_getBoolean(value);

    return false;
}

/* 相对启动时刻的秒数（服务器刚起来时 diAiStartTime 还为 0，防负值/巨大值） */
static double
elapsedSeconds(uint64_t timestamp)
{
    if (diAiStartTime == 0)
        return 0.0;

    return (timestamp - diAiStartTime) / 1000.0;
}

/*
 * 比较当前值与上一次的值，只在发生变化时打印。
 * 首次调用会打印全部 DI/AI 的初始值。
 */
static void
logDiAiChanges(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        float value = getAnInValue(anInValueAttrs[i]);

        if (firstDiAiPrint || (fabsf(value - lastAnInValues[i]) > 0.0005f)) {
            printf("[%7.2fs] [AI] GGIO1.AnIn%d.mag.f = %.4f\n",
                    (Hal_getTimeInMs() - diAiStartTime) / 1000.0, i + 1, value);

            lastAnInValues[i] = value;
        }
    }

    for (i = 0; i < 4; i++) {
        bool value = getIndValue(indValueAttrs[i]);

        if (firstDiAiPrint || (value != lastIndValues[i])) {
            printf("[%7.2fs] [DI] GGIO1.Ind%d.stVal = %s\n",
                    (Hal_getTimeInMs() - diAiStartTime) / 1000.0, i + 1,
                    value ? "true" : "false");

            lastIndValues[i] = value;
        }
    }

    firstDiAiPrint = false;

    /* 保证重定向到文件时也能实时看到输出 */
    fflush(stdout);
}

void
sigint_handler(int signalId)
{
    running = 0;
}

static CheckHandlerResult
checkHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test, bool interlockCheck)
{
    ClientConnection clientCon = ControlAction_getClientConnection(action);

    if (clientCon) {
        printf("Control from client %s\n", ClientConnection_getPeerAddress(clientCon));
    }
    else {
        printf("clientCon == NULL\n");
    }

    if (ControlAction_isSelect(action))
        printf("check handler called by select command!\n");
    else
        printf("check handler called by operate command!\n");

    if (interlockCheck)
        printf("  with interlock check bit set!\n");

    printf("  ctlNum: %i\n", ControlAction_getCtlNum(action));

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1)
        return CONTROL_ACCEPTED;

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2)
        return CONTROL_ACCEPTED;

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3)
        return CONTROL_ACCEPTED;

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4)
        return CONTROL_ACCEPTED;

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO9)
        return CONTROL_ACCEPTED;

    return CONTROL_OBJECT_UNDEFINED;
}

static ControlHandlerResult
controlHandlerForBinaryOutput(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    uint64_t timestamp = Hal_getTimeInMs();

    printf("control handler called\n");
    printf("  ctlNum: %i\n", ControlAction_getCtlNum(action));

    ClientConnection clientCon = ControlAction_getClientConnection(action);

    if (clientCon) {
        printf("Control from client %s\n", ClientConnection_getPeerAddress(clientCon));
    }
    else {
        printf("clientCon == NULL!\n");
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
    }

    else if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal, value);
    }

    else if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal, value);
    }

    else if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, value);
    }
    else
        return CONTROL_RESULT_FAILED;

    return CONTROL_RESULT_OK;
}

static MmsDataAccessError
writeAccessHandler (DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    ControlModel ctlModelVal = (ControlModel) MmsValue_toInt32(value);

    /* we only allow status-only and direct-operate */
    if ((ctlModelVal == CONTROL_MODEL_STATUS_ONLY) || (ctlModelVal == CONTROL_MODEL_DIRECT_NORMAL))
    {
        IedServer_updateCtlModel(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1, ctlModelVal);

        printf("Changed GGIO1.SPCSI.ctlModel to %i\n", ctlModelVal);

        return DATA_ACCESS_ERROR_SUCCESS;
    }
    else {
        return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
    }
}

/*
 * 客户端写入 SP 设定值（setVal）时调用。
 * 把设定值同步到只读的 stVal / mag.f，并更新时间戳。
 */
/*
 * 客户端写入 SP 设定值（setVal）时调用。
 * 把设定值同步到只读的 stVal / mag.f，更新时间戳，并立即打印一条带客户端地址的记录，
 * 这样"客户端主动更新模拟量/开入"在服务端终端上能第一时间看到。
 */
static MmsDataAccessError
setpointWriteAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    SetpointParameter* setpoint = (SetpointParameter*) parameter;
    uint64_t timestamp = Hal_getTimeInMs();
    const char* peer = (connection != NULL) ? ClientConnection_getPeerAddress(connection) : "(unknown)";

    (void) dataAttribute;

    if (setpoint->isAnalog) {
        if (MmsValue_getType(value) != MMS_FLOAT)
            return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;

        float oldValue = getAnInValue(anInValueAttrs[setpoint->index]);
        float newValue = MmsValue_toFloat(value);

        IedServer_updateFloatAttributeValue(iedServer, anInValueAttrs[setpoint->index], newValue);
        IedServer_updateUTCTimeAttributeValue(iedServer, anInTimestampAttrs[setpoint->index], timestamp);

        /* 这里已经打印过，同步缓存值，避免主循环轮询时重复输出同一次变化 */
        lastAnInValues[setpoint->index] = newValue;

        printf("[%7.2fs] [CLIENT-WRITE] %s 写入 GGIO1.AnIn%d.mag.f (setVal): %.4f -> %.4f\n",
                elapsedSeconds(timestamp), peer, setpoint->index + 1, oldValue, newValue);
    }
    else {
        if (MmsValue_getType(value) != MMS_BOOLEAN)
            return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;

        bool oldValue = getIndValue(indValueAttrs[setpoint->index]);
        bool newValue = MmsValue_getBoolean(value);

        IedServer_updateBooleanAttributeValue(iedServer, indValueAttrs[setpoint->index], newValue);
        IedServer_updateUTCTimeAttributeValue(iedServer, indTimestampAttrs[setpoint->index], timestamp);

        lastIndValues[setpoint->index] = newValue;

        printf("[%7.2fs] [CLIENT-WRITE] %s 写入 GGIO1.Ind%d.stVal (setVal): %s -> %s\n",
                elapsedSeconds(timestamp), peer, setpoint->index + 1,
                oldValue ? "true" : "false", newValue ? "true" : "false");
    }

    fflush(stdout);

    return DATA_ACCESS_ERROR_SUCCESS;
}

/* ==================== 新增：终端本地遥控（SPCSO1~4 变位） ==================== */

/*
 * 程序运行时在终端输入 1~4 并回车，即对 GGIO1.SPCSO1~4 执行一次遥控变位：
 *   - 更新 SPCSO_n.t     （UTC 时间戳）
 *   - 更新 SPCSO_n.stVal （新位置：取反，或由 "n on" / "n off" 显式指定）
 *
 * 走的是 IedServer_update* 接口，和客户端下发 Oper 命令的最终效果一致，
 * 报告（RCB）/ GOOSE / 订阅的客户端都能看到这次变位。
 */

static DataAttribute* spcsoStValAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal,
    IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal,
    IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal,
    IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal
};

static DataAttribute* spcsoTimestampAttrs[4] = {
    IEDMODEL_GenericIO_GGIO1_SPCSO1_t,
    IEDMODEL_GenericIO_GGIO1_SPCSO2_t,
    IEDMODEL_GenericIO_GGIO1_SPCSO3_t,
    IEDMODEL_GenericIO_GGIO1_SPCSO4_t
};

static const char* spcsoNames[4] = {
    "GGIO1.SPCSO1",
    "GGIO1.SPCSO2",
    "GGIO1.SPCSO3",
    "GGIO1.SPCSO4"
};

/* 服务器就绪标志：仅在服务器运行期间允许本地遥控，避免停止后误用句柄 */
static bool localControlReady = false;

static bool
getSpcsoValue(int index)
{
    MmsValue* value = IedServer_getAttributeValue(iedServer, spcsoStValAttrs[index]);

    if ((value != NULL) && (MmsValue_getType(value) == MMS_BOOLEAN))
        return MmsValue_getBoolean(value);

    return false;
}

/* 打印 4 路 SPCSO 当前状态 */
static void
printSpcsoStatus(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        printf("    %s.stVal = %s\n", spcsoNames[i], getSpcsoValue(i) ? "true" : "false");
    }

    fflush(stdout);
}

/*
 * 执行一次本地遥控。
 * isToggle = true  时取反当前位置；否则按 newValue 置位。
 */
static void
executeLocalControl(int index, bool newValue, bool isToggle)
{
    bool oldValue;
    uint64_t timestamp;

    if (!localControlReady)
        return;

    oldValue = getSpcsoValue(index);

    if (isToggle)
        newValue = !oldValue;

    timestamp = Hal_getTimeInMs();

    IedServer_updateUTCTimeAttributeValue(iedServer, spcsoTimestampAttrs[index], timestamp);
    IedServer_updateBooleanAttributeValue(iedServer, spcsoStValAttrs[index], newValue);

    printf("[%7.2fs] [LOCAL-CTRL] %s %s: %s -> %s\n",
            (Hal_getTimeInMs() - diAiStartTime) / 1000.0,
            spcsoNames[index],
            isToggle ? "变位" : "置位",
            oldValue ? "true" : "false",
            newValue ? "true" : "false");

    fflush(stdout);
}

static void
printLocalControlMenu(void)
{
    printf("\n");
    printf("=========================================================\n");
    printf(" 本地遥控（在终端输入后回车）\n");
    printf("   1 / 2 / 3 / 4 : GGIO1.SPCSO1~4 遥控变位（当前位置取反）\n");
    printf("   1 on / 1 off  : 指定 SPCSO1 置为 true / false\n");
    printf("   0             : 打印 4 路 SPCSO 当前状态\n");
    printf("   q             : 退出程序（等同 Ctrl+C）\n");
    printf("=========================================================\n");
    fflush(stdout);
}

/* 解析一行终端输入并执行 */
static void
handleConsoleCommand(char* line)
{
    char* p = line;
    int index = -1;
    bool hasExplicitValue = false;
    bool explicitValue = false;

    while ((*p != '\0') && isspace((unsigned char) *p))
        p++;

    if ((*p >= '1') && (*p <= '4')) {
        index = *p - '1';
        p++;
    }
    else if (*p == '0') {
        printf("\n当前 SPCSO 状态：\n");
        printSpcsoStatus();
        return;
    }
    else if ((*p == 'q') || (*p == 'Q')) {
        printf("收到退出指令，正在停止服务器 ...\n");
        fflush(stdout);
        running = 0;
        return;
    }
    else if ((*p == '\0') || (*p == '\n') || (*p == '\r')) {
        return; /* 空行忽略 */
    }
    else {
        printf("无效输入：请输入 1~4 变位，0 查看状态，q 退出。\n");
        fflush(stdout);
        return;
    }

    /* 后面可跟 on / off / 1 / 0 表示显式置位 */
    while ((*p != '\0') && isspace((unsigned char) *p))
        p++;

    if (*p == '1') {
        hasExplicitValue = true;
        explicitValue = true;
    }
    else if (*p == '0') {
        hasExplicitValue = true;
        explicitValue = false;
    }
    else if ((*p == 'o') || (*p == 'O')) {
        if ((p[1] == 'n') || (p[1] == 'N')) {
            hasExplicitValue = true;
            explicitValue = true;
        }
        else if ((p[1] == 'f') || (p[1] == 'F')) {
            hasExplicitValue = true;
            explicitValue = false;
        }
    }

    if (!hasExplicitValue && (*p != '\0') && (*p != '\n') && (*p != '\r')) {
        printf("无效输入：SPCSO%d 后面只能跟 on / off。\n", index + 1);
        fflush(stdout);
        return;
    }

    executeLocalControl(index, explicitValue, !hasExplicitValue);
}

/* 终端输入线程：阻塞读 stdin，不影响主循环的 DI/AI 监视 */
static void*
consoleInputThread(void* parameter)
{
    char line[64];

    (void) parameter;

    while (running && (fgets(line, sizeof(line), stdin) != NULL)) {
        handleConsoleCommand(line);
    }

    return NULL;
}

int
main(int argc, char** argv)
{
    initConsoleEncoding();

    iedServer = IedServer_create(&iedModel);
    int tcpPort = 102;
    int i;

    if (argc > 1) {
        tcpPort = atoi(argv[1]);
    }

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO1);

    /*
     * For SPCSO1 we want the user be able to change the control model by online service -
     * so we install a write access handler to change the control model when the client
     * writes to the "ctlModel" attribute.
     */
    IedServer_handleWriteAccess(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_ctlModel, writeAccessHandler, NULL);

    /*
     * 为 DI/AI 的 SP 设定值安装写回调。
     * 客户端写 GGIO1.Ind1.setVal 或 GGIO1.AnIn1.setVal 时，
     * 回调会把值同步到对应的 stVal / mag.f。
     */
    for (i = 0; i < 4; i++) {
        anInSetpointParams[i].index = i;
        anInSetpointParams[i].isAnalog = true;

        indSetpointParams[i].index = i;
        indSetpointParams[i].isAnalog = false;

        IedServer_handleWriteAccess(iedServer, anInSetValAttrs[i],
                setpointWriteAccessHandler, &anInSetpointParams[i]);

        IedServer_handleWriteAccess(iedServer, indSetValAttrs[i],
                setpointWriteAccessHandler, &indSetpointParams[i]);
    }

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO2);

    /* this is optional - performs operative checks */
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2, checkHandler,
            IEDMODEL_GenericIO_GGIO1_SPCSO2);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO3);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO4);

    /* this is optional - performs operative checks */
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4, checkHandler,
            IEDMODEL_GenericIO_GGIO1_SPCSO4);


    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO9,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO9);

    /* this is optional - performs operative checks */
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO9, checkHandler,
            IEDMODEL_GenericIO_GGIO1_SPCSO9);

    /* MMS server will be instructed to start listening to client connections. */
    IedServer_start(iedServer, tcpPort);

    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    running = 1;

    signal(SIGINT, sigint_handler);

    /* ========== 新增：启动终端本地遥控 ========== */
    diAiStartTime = Hal_getTimeInMs();
    localControlReady = true;

    printLocalControlMenu();

    /* 独立线程读终端输入，主循环继续做 DI/AI 变化监视 */
    Thread consoleThread = Thread_create(consoleInputThread, NULL, true);

    if (consoleThread != NULL) {
        Thread_start(consoleThread);
    }
    else {
        printf("警告：终端输入线程创建失败，本地遥控不可用。\n");
        fflush(stdout);
    }

    /* ========== 主循环：监控 DI/AI 变化并打印 ========== */
    while (running) {
        /* 每 DI_AI_POLL_INTERVAL_MS 检查一次，有变化就打印 */
        logDiAiChanges();

        Thread_sleep(DI_AI_POLL_INTERVAL_MS);
    }

    /* 先关闭本地遥控入口，避免服务器销毁后仍被写入 */
    localControlReady = false;

    /* stop MMS server - close TCP server socket and all client sockets */
    IedServer_stop(iedServer);

    /* Cleanup - free all resources */
    IedServer_destroy(iedServer);

    restoreConsoleEncoding();

    return 0;
} /* main() */
