/*
 *  server_example_basic_io.c
 *
 *  功能说明：
 *  这个示例程序演示了如何创建一个功能完整的 IEC 61850 服务器，
 *  涵盖数据服务、控制服务和协议栈配置等核心功能。
 *
 *  主要演示内容：
 *  - 如何使用简单的控制模型（Control Model）
 *  - 如何提供模拟量测量数据服务
 *  - 如何使用 IedServerConfig 对象配置协议栈特性
 *
 *  使用说明：
 *  1. 编译后运行服务器
 *  2. 可配合各种客户端示例进行测试
 *  3. 本示例禁用了文件服务，因此 file-tool 无法使用
 *
 *  启动命令示例：
 *    ./server_example_basic_io           # 使用默认端口102
 *    ./server_example_basic_io 8102      # 使用自定义端口8102
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "static_model.h"      /* 由配置工具生成的静态数据模型 */

static int running = 0;                 /* 服务器运行标志 */
static int activeConnections = 0;       /* 当前活跃连接数 */
static IedServer iedServer = NULL;      /* 服务器实例 */

/**
 * 信号处理函数（Ctrl+C）
 */
void sigint_handler(int signalId)
{
    running = 0;
}

/**
 * 控制操作回调函数
 * 当客户端发起控制操作时被调用
 *
 * @param action  控制动作类型（操作/取消等）
 * @param parameter 用户参数，标识是哪个控制对象
 * @param value   控制值（true=合闸/启动，false=分闸/停止）
 * @param test    是否为测试模式
 * @return 控制结果（成功/失败）
 */
static ControlHandlerResult
controlHandlerForBinaryOutput(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    /* 如果是测试模式，直接返回失败（实际应用中可允许测试） */
    if (test)
        return CONTROL_RESULT_FAILED;

    /* 检查值类型是否为布尔值 */
    if (MmsValue_getType(value) == MMS_BOOLEAN) {
        printf("received binary control command: ");

        if (MmsValue_getBoolean(value))
            printf("on\n");
        else
            printf("off\n");
    }
    else
        return CONTROL_RESULT_FAILED;

    /* 获取当前时间戳，用于记录操作时间 */
    uint64_t timeStamp = Hal_getTimeInMs();

    /* 根据参数更新对应的控制对象 */
    /// @todo all these if conditions are in static_model
    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, value);
    }

    return CONTROL_RESULT_OK;
}

/**
 * 连接状态回调函数
 * 当客户端连接建立或断开时被调用
 *
 * @param self       服务器实例
 * @param connection 客户端连接对象
 * @param connected  true=连接建立，false=连接断开
 * @param parameter  用户参数
 */
static void
connectionHandler(IedServer self, ClientConnection connection, bool connected, void* parameter)
{
    const char* peerAddress = ClientConnection_getPeerAddress(connection);

    if (connected) {
        activeConnections++;
        printf("Connection opened: client=%p peer=%s\n",
            (void*) connection,
            peerAddress ? peerAddress : "unknown");
    }
    else {
        if (activeConnections > 0)
            activeConnections--;

        printf("Connection closed: client=%p peer=%s\n",
            (void*) connection,
            peerAddress ? peerAddress : "unknown");
    }

    printf("Current active connections: %d\n", activeConnections);
}

/**
 * 报告控制块（RCB）事件回调函数
 * 当客户端访问或修改 RCB 时被调用
 * 注释掉的代码可开启用于调试
 *
 * @param parameter     用户参数
 * @param rcb           报告控制块对象
 * @param connection    客户端连接
 * @param event         事件类型（启用/禁用/设置参数/获取参数）
 * @param parameterName 参数名
 * @param serviceError  服务错误码
 */
static void
rcbEventHandler(void* parameter, ReportControlBlock* rcb, ClientConnection connection,
                IedServer_RCBEventType event, const char* parameterName,
                MmsDataAccessError serviceError)
{
    /* 默认实现为空，可取消注释进行调试 */
    // printf("RCB: %s event: %i\n", ReportControlBlock_getName(rcb), event);

    if(connection == NULL)
    {
        // printf("[RCB] Connection is NULL, function exit ... ");
        return;
    }

    const char* rcbName = ReportControlBlock_getName(rcb);
    const char* peerAddress = ClientConnection_getPeerAddress(connection);;
    switch (event)
    {
    case RCB_EVENT_ENABLE:
        printf("[RCB] %s is enabled by client %s", rcbName, peerAddress);
        break;
    case RCB_EVENT_DISABLE:
        printf("[RCB] %s is disabled by client %s", rcbName, peerAddress);
        break;
    case RCB_EVENT_SET_PARAMETER:
        printf("[RCB] %s param changed by client %s", rcbName, peerAddress);
        break;
    case RCB_EVENT_GET_PARAMETER:
        printf("[RCB] %s param read by client %s", rcbName, peerAddress);
        break;
    case RCB_EVENT_GI:
        printf("[RCB] %s recieve（GI）call\n", rcbName);
        break;  
    case RCB_EVENT_REPORT_CREATED:
        printf("[RCB] %s generated a new rpt\n", rcbName);
        break;
    case RCB_EVENT_OVERFLOW:
        printf("[RCB] %s buffer out of mem！\n", rcbName);
        break;   
    case RCB_EVENT_PURGEBUF:
        printf("[RCB] %s buffer cleared\n", rcbName);
        break;  
    case RCB_EVENT_RESERVED:
        printf("[RCB] %s is saved by client %s\n", rcbName, peerAddress);
        break; 
    case RCB_EVENT_UNRESERVED:
        printf("[RCB] %s is canceled by client %s\n", rcbName, peerAddress);
        break;
    default:
        printf("[RCB] %s occurs unknown event: %i\n", rcbName, event);
        break;
    }
}


int main(int argc, char** argv)
{
    /* 解析端口参数：./server_example_basic_io [port] */
    int tcpPort = 102;
    if (argc > 1) {
        tcpPort = atoi(argv[1]);
    }
    printf("Using libIEC61850 version %s\n", LibIEC61850_getVersionString());

    /* ========== 创建服务器配置对象 ========== */
    IedServerConfig config = IedServerConfig_create();

    /* 1. 设置报告缓冲区大小（用于缓冲报告控制块） */
    IedServerConfig_setReportBufferSize(config, 200000);

    /* 2. 设置标准版本（警告：数据模型也需要符合相应版本） */
    IedServerConfig_setEdition(config, IEC_61850_EDITION_2);

    /* 3. 设置文件服务基路径（虽然本示例禁用了文件服务） */
    IedServerConfig_setFileServiceBasePath(config, "./vmd-filestore/");

    /* 4. 禁用 MMS 文件服务（演示如何关闭不需要的服务） */
    IedServerConfig_enableFileService(config, true);       // sungh change to true

    /* 5. 启用动态数据集服务
     *    允许客户端在运行时创建、修改或删除数据集，
     *    而不是只能使用 CID 中预定义的静态数据集
     */
    IedServerConfig_enableDynamicDataSetService(config, true);

    /* 6. 禁用日志服务 */
    IedServerConfig_enableLogService(config, false);

    /* 7. 设置最大客户端连接数 */
    IedServerConfig_setMaxMmsConnections(config, 2);

    /* ========== 创建服务器实例 ========== */
    iedServer = IedServer_createWithConfig(&iedModel, NULL, config);

    /* 配置对象不再需要，释放它 */
    IedServerConfig_destroy(config);

    /* ========== 设置 MMS 身份标识信息 ========== */
    /* 当客户端调用 Identify 服务时返回这些信息 */
    IedServer_setServerIdentity(iedServer, "MZ", "basic io", "1.6.0");

    /* ========== 安装控制操作处理器 ========== */
    /* 为 4 个可控开关注册控制回调 */
    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO1);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO2);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO3);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO4);

    /* ========== 安装回调处理器 ========== */
    IedServer_setConnectionIndicationHandler(iedServer, (IedConnectionIndicationHandler) connectionHandler, NULL);
    IedServer_setRCBEventHandler(iedServer, rcbEventHandler, NULL);

    /* ========== 设置写访问策略 ========== */
    /* 默认情况下，FC=DC（描述信息）和 FC=CF（配置）不可写
     * 这里允许客户端写入 GGIO1.NamPlt.vendor（用于某些客户端示例）
     */
    IedServer_setWriteAccessPolicy(iedServer, IEC61850_FC_DC, ACCESS_POLICY_ALLOW);

    /* ========== 启动服务器 ========== */
    IedServer_start(iedServer, tcpPort);

    if (!IedServer_isRunning(iedServer))
    {
        printf("Starting server failed (maybe need root permissions or another server is already using the port)! Exit.\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    running = 1;

    signal(SIGINT, sigint_handler);

    /* ========== 主循环：生成模拟数据 ========== */
    float t = 0.f;   /* 时间变量，用于生成正弦波 */
    /* sungh: 遥信变位模拟 —— 每个点周期不同，便于对号 */
    int  tickCount = 0;
    int  ind1 = 0, ind2 = 0, ind3 = 0, ind4 = 0, proxy = 0;

    while (running)
    {
        uint64_t timestamp = Hal_getTimeInMs();

        /* 生成 4 路相位不同的正弦波模拟量 */
        t += 0.1f;

        float an1 = sinf(t);
        float an2 = sinf(t + 1.f);
        float an3 = sinf(t + 2.f);
        float an4 = sinf(t + 3.f);

        /* 构造 IEC 61850 时间戳 */
        Timestamp iecTimestamp;

        Timestamp_clearFlags(&iecTimestamp);
        Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
        Timestamp_setLeapSecondKnown(&iecTimestamp, true);

        /* 每 2 秒切换一次时钟同步状态标志 */
        if (((int) t % 2) == 0)
            Timestamp_setClockNotSynchronized(&iecTimestamp, true);

        /* ========== 更新数据模型 ========== */
        IedServer_lockDataModel(iedServer);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_mag_f, an1);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn2_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn2_mag_f, an2);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn3_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn3_mag_f, an3);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn4_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn4_mag_f, an4);


        /* ---- sungh 遥信变位模拟（方案B映射验证用） ---- */
        tickCount++;
        if ((tickCount % 20)  == 0) ind1  = !ind1;   /* 每 2  秒翻 */
        if ((tickCount % 30)  == 0) ind2  = !ind2;   /* 每 3  秒翻 */
        if ((tickCount % 50)  == 0) ind3  = !ind3;   /* 每 5  秒翻 */
        if ((tickCount % 70)  == 0) ind4  = !ind4;   /* 每 7  秒翻 */
        if ((tickCount % 100) == 0) proxy = !proxy;  /* 每 10 秒翻 */

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_t, &iecTimestamp);
        IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_stVal, ind1);
        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_t, &iecTimestamp);
        IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_stVal, ind2);
        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind3_t, &iecTimestamp);
        IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind3_stVal, ind3);
        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind4_t, &iecTimestamp);
        IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind4_stVal, ind4);
        IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_LPHD1_Proxy_stVal, proxy);

        IedServer_unlockDataModel(iedServer);

        /* 每 100ms 更新一次（10Hz 刷新率） */
        Thread_sleep(100);
    }

    /* ========== 停止服务器并清理资源 ========== */
    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);

    return 0;
} /* main() */

