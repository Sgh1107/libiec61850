/*
 *  server_example_goose.c
 *
 *  功能说明：
 *  这个示例程序演示了如何创建一个同时支持 GOOSE 发布、报告（Reporting）和控制模型（Control Model）的 IEC 61850 服务器。
 *
 *  主要演示内容：
 *  - 如何创建和配置 GOOSE 发布者（Publisher）
 *  - 如何处理客户端发起的控制操作（如遥控）
 *  - 如何更新数据模型并触发 GOOSE 或报告
 *  - 如何为不同的 GOOSE 控制块指定不同的网络接口
 */

#include "iec61850_server.h"
#include "hal_thread.h" /* 用于 Thread_sleep() */
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "static_model.h" /* 由配置工具生成的静态数据模型 */

static int running = 0;        /* 服务器运行标志，0=停止，1=运行 */
static IedServer iedServer = NULL; /* IEC 61850 服务器实例 */

/**
 * 信号处理函数
 * 当用户按下 Ctrl+C 时，设置 running = 0，让主循环退出
 */
void sigint_handler(int signalId)
{
    running = 0;
}

/**
 * 控制操作回调函数
 * 
 * 当客户端发起控制操作（如遥控合闸/分闸）时，此函数被调用。
 * 它负责更新对应的数据属性（如 stVal 和 t 时间戳），模拟物理输出。
 *
 * @param action    控制动作类型（操作、取消等）
 * @param parameter 用户参数，用于标识是哪个控制对象
 * @param value     控制值（如 true=合闸，false=分闸）
 */
void
controlHandlerForBinaryOutput(ControlAction action, void* parameter, MmsValue* value)
{
    /* 获取当前时间戳，用于记录操作时间 */
    uint64_t timestamp = Hal_getTimeInMs();

    /* 根据传入的参数判断是哪个控制对象，并更新对应的模型节点 */
    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal, value);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, timestamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, value);
    }
}

/**
 * GOOSE 控制块（GoCB）事件回调函数
 * 
 * 当客户端通过 MMS 修改 GoCB 配置时（如改变 GoEna 使能状态），此函数被调用。
 * 可用于记录审计日志或监控 GOOSE 配置的变化。
 *
 * @param goCb      GOOSE 控制块对象
 * @param event     事件类型
 * @param parameter 用户参数
 */
static void
goCbEventHandler(MmsGooseControlBlock goCb, int event, void* parameter)
{
    printf("Access to GoCB: %s\n", MmsGooseControlBlock_getName(goCb));
    printf("         GoEna: %i\n", MmsGooseControlBlock_getGoEna(goCb));
}

/**
 * 程序主函数
 */
int
main(int argc, char** argv)
{
    /* 创建服务器配置对象 */
    IedServerConfig config = IedServerConfig_create();

    /* 使用静态模型和配置创建服务器实例 */
    iedServer = IedServer_createWithConfig(&iedModel, NULL, config);

    /* 配置对象使用后即可销毁 */
    IedServerConfig_destroy(config);

    /* ========== 网络接口配置 ========== */
    /* 第一个参数：设置所有 GOOSE 发布者使用的默认网络接口 */
    if (argc > 1) {
        char* ethernetIfcID = argv[1];

        printf("Using GOOSE interface: %s\n", ethernetIfcID);

        IedServer_setGooseInterfaceId(iedServer, ethernetIfcID);
    }

    /* 第二个参数：为特定的 GOOSE 控制块指定独立的网络接口 */
    if (argc > 2) {
        char* ethernetIfcID = argv[2];

        printf("Using GOOSE interface for GenericIO/LLN0.gcbAnalogValues: %s\n", ethernetIfcID);

        IedServer_setGooseInterfaceIdEx(iedServer, IEDMODEL_GenericIO_LLN0, "gcbAnalogValues", ethernetIfcID);
    }

    /* 安装 GoCB 事件回调，用于监控 GOOSE 配置变化 */
    IedServer_setGoCBHandler(iedServer, goCbEventHandler, NULL);

    /* ========== 启动服务器 ========== */
    /* 开始监听 MMS 连接（端口 102） */
    IedServer_start(iedServer, 102);

    /* ========== 注册控制操作处理器 ========== */
    /* 为 4 个可控开关（SPCSO1 ~ SPCSO4）注册控制回调 */
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

    /* 检查服务器是否成功启动 */
    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    /* ========== 启动 GOOSE 发布 ========== */
    /* 启用所有配置好的 GOOSE 发布者，开始发送 GOOSE 报文 */
    IedServer_enableGoosePublishing(iedServer);

    running = 1;

    /* 注册 Ctrl+C 信号处理 */
    signal(SIGINT, sigint_handler);

    /* ========== 主循环：模拟数据变化 ========== */
    float anIn1 = 0.f;    /* 模拟量输入值，从 0 开始递增 */
    int eventCount = 10;  /* 只触发前 10 次事件 */

    while (running) {
        /* 锁定数据模型，确保线程安全 */
        IedServer_lockDataModel(iedServer);

        /* 更新模拟量输入 AnIn1 的值和时间戳 */
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_t, Hal_getTimeInMs());
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_mag_f, anIn1);

        /* 前 10 次循环时，交替改变 SPCSO4 的状态，触发 GOOSE 事件 */
        if (eventCount) {
            IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, Hal_getTimeInMs());

            if (eventCount % 2) {
                /* 偶数次：置为 GOOD 品质 + true（合闸） */
                IedServer_updateQuality(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_q, QUALITY_VALIDITY_GOOD);
                IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, true);
            }
            else {
                /* 奇数次：置为 INVALID 品质 + false（分闸） */
                IedServer_updateQuality(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_q, QUALITY_VALIDITY_INVALID);
                IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, false);
            }

            eventCount--;
        }

        /* 解锁数据模型 */
        IedServer_unlockDataModel(iedServer);

        /* 模拟量递增 0.1，模拟连续变化的采样值 */
        anIn1 += 0.1;

        /* 每秒更新一次 */
        Thread_sleep(1000);
    }

    /* ========== 停止并清理资源 ========== */
    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);

    return 0;
} /* main() */
