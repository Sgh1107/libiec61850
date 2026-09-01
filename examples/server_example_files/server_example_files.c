/*
 *  server_example_files.c
 *  
 *  功能说明：
 *  这个示例程序演示了如何创建一个支持MMS文件服务的IEC 61850服务器
 *  
 *  主要演示内容：
 *  - 如何使用一些特殊的MMS文件服务特性
 *  - 如何精细控制客户端对文件服务的使用权限
 *  
 *  使用说明：
 *  1. 编译后运行服务器
 *  2. 确保当前目录下存在 "./vmd-filestore/" 目录（文件存储根目录）
 *  3. 在该目录下放置一些文件，供客户端访问
 *  4. 配合 file-tool 客户端工具进行测试
 *  
 *  启动命令示例：
 *    ./server_example_files           # 使用默认端口102
 *    ./server_example_files 8102      # 使用自定义端口8102
 */

#include "iec61850_server.h"   /* IEC 61850 服务器端 API 头文件 */
#include "hal_thread.h"        /* 线程休眠等系统抽象层函数 */
#include <signal.h>            /* 信号处理（用于优雅退出） */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "static_model.h"      /* 由配置工具生成的静态模型定义文件 */

static int running = 0;        /* 服务器运行状态标志，0停止，1运行中 */
static IedServer iedServer = NULL;  /* IEC 61850 服务器实例句柄 */

/**
 * 信号处理函数（SIGINT信号，即Ctrl+C）
 * 当用户按下Ctrl+C时，设置 running = 0，让主循环优雅退出
 */
void
sigint_handler(int signalId)
{
    running = 0;
}

/**
 * 客户端连接状态变化回调函数
 * 当有客户端连接或断开时，该函数会被自动调用
 * 
 * @param self        IED服务器实例
 * @param connection  客户端连接对象
 * @param connected   true表示连接建立，false表示连接断开
 * @param parameter   用户自定义参数（未使用）
 */
static void
connectionHandler(IedServer self, ClientConnection connection, bool connected, void* parameter)
{
    if (connected)
        printf("Connection opened\n");   /* 客户端连接成功 */
    else
        printf("Connection closed\n");   /* 客户端断开连接 */
}

/**
 * 文件访问控制回调函数（核心功能）
 * 在客户端执行文件操作（列出目录、读取、写入、删除、重命名等）时，
 * 该函数会被调用，用于决定是否允许该操作。
 * 
 * 功能演示：
 *   - 禁止客户端重命名文件
 *   - 禁止客户端删除名为 "IEDSERVER.BIN" 的文件
 *   - 其他操作允许
 * 
 * @param parameter     用户自定义参数
 * @param connection    MMS连接对象
 * @param service       请求的文件服务类型（枚举见下方说明）
 * @param localFilename 本地文件名（服务器端文件名）
 * @param otherFilename 另一个文件名（用于重命名等操作）
 * @return MMS_ERROR_NONE 表示允许操作，其他值表示拒绝
 */
static MmsError
fileAccessHandler(void* parameter, MmsServerConnection connection, MmsFileServiceType service,
                                          const char* localFilename, const char* otherFilename)
{
    /* 打印文件访问日志，便于调试和审计 */
    printf("fileAccessHandler: service = %s, local-file: %s other-file: %s\n", 
           ser2str(service), localFilename, otherFilename);

    /*
     * MMS_FILE_ACCESS_TYPE_RENAME = 重命名操作
     * 规则：禁止客户端重命名任何文件（返回错误码拒绝操作）
     */
    if (service == MMS_FILE_ACCESS_TYPE_RENAME)
        return MMS_ERROR_FILE_FILE_ACCESS_DENIED;

    /*
     * MMS_FILE_ACCESS_TYPE_DELETE = 删除操作
     * 规则：禁止客户端删除名为 "IEDSERVER.BIN" 的特定文件
     * 这个文件可能是服务器的关键配置文件，需要特殊保护
     */
    if (service == MMS_FILE_ACCESS_TYPE_DELETE) {
        if (strcmp(localFilename, "IEDSERVER.BIN") == 0)
            return MMS_ERROR_FILE_FILE_ACCESS_DENIED;
    }

    /*
     * 对于其他文件访问操作（读取、写入、创建目录等），
     * 返回 MMS_ERROR_NONE 表示允许操作
     */
    return MMS_ERROR_NONE;
}

/**
 * 程序主函数
 */
int
main(int argc, char** argv)
{
    int tcpPort = 102;   /* 默认端口：IEC 61850/MMS 标准端口号 */

    /* 如果命令行提供了端口参数，则使用自定义端口 */
    if (argc > 1) {
        tcpPort = atoi(argv[1]);
    }

    /* 打印库版本信息 */
    printf("Using libIEC61850 version %s\n", LibIEC61850_getVersionString());

    /* 
     * 创建IED服务器实例
     * iedModel 在 static_model.h 中定义，包含了该服务器支持的数据模型
     */
    iedServer = IedServer_create(&iedModel);

    /* @todo 修改这里改变文件操作的basedir
     * ========== 关键配置：设置文件存储根目录 ==========
     * 所有客户端的文件访问都将基于此路径进行
     * 例如：客户端请求读取 "config.cfg"，实际访问的是 "./vmd-filestore/config.cfg
     * 重要：运行此程序前，需要手动创建该目录！
     */
    IedServer_setFilestoreBasepath(iedServer, "./vmd-filestore/");

    /*
     * ========== 关键配置：安装文件访问控制回调 ==========
     * 通过此回调函数，可以精确控制每个客户端文件操作的权限
     * 非常适合实现基于角色（RBAC）或文件类型（如保护关键系统文件）的访问控制
     */
    MmsServer mmsServer = IedServer_getMmsServer(iedServer);
    MmsServer_installFileAccessHandler(mmsServer, fileAccessHandler, NULL);

    /*
     * 设置连接指示回调函数
     * 当客户端连接或断开时，connectionHandler 函数会被调用
     * 可用于记录审计日志或实现连接数限制
     */
    IedServer_setConnectionIndicationHandler(iedServer, (IedConnectionIndicationHandler) connectionHandler, NULL);

    /*
     * ========== 启动服务器 ==========
     * 开始监听指定端口的客户端连接请求
     */
    IedServer_start(iedServer, tcpPort);

    /* 检查服务器是否成功启动 */
    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    running = 1;

    /* 注册信号处理函数，使程序可以通过 Ctrl+C 优雅退出 */
    signal(SIGINT, sigint_handler);

    /*
     * ========== 主事件循环 ==========
     * 服务器在后台独立线程中运行，主线程只需等待退出信号
     * Thread_sleep(100) 每100毫秒检查一次运行状态
     */
    while (running)
        Thread_sleep(100);

    /*
     * ========== 清理资源 ==========
     * 停止服务器，关闭所有连接，释放内存
     */
    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);
    return 0;

} /* main() */
