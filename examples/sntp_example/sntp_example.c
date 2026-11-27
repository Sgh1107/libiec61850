/*
 * sntp_example.c
 * 客户端网络对时服务
 */

#include "sntp_client.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdio.h>

static bool running = true;

void
sigint_handler(int signalId)
{
    running = false;
}

static void
sntpUserCallback(void* parameter, bool isSynced)
{
    if (isSynced)
        printf("SNTP: Clock synchronized\n");
    else
        printf("SNTP: Clock not synchronized\n");
}

int
main(int argc, char** argv)
{
    // 建 UDP socket + 选好事件集
    SNTPClient client = SNTPClient_create();
    // 只支持绑定一个服务器
    SNTPClient_addServer(client, "192.168.178.74", 123);
    // 同步成功/失效时回调
    SNTPClient_setUserCallback(client, sntpUserCallback, NULL);
    // 16 秒轮询一次
    SNTPClient_setPollInterval(client, 16);
    // 绑 UDP:123 + 起线程
    SNTPClient_start(client);

    signal(SIGINT, sigint_handler);
    // 主线程只负责等 Ctrl+C
    while (running) {
        Thread_sleep(100);
    }

    SNTPClient_destroy(client);
}

