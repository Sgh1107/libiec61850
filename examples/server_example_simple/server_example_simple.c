/*
 *  server_example_simple.c - 带详细调试信息
 */

#include "iec61850_server.h"
#include "iec61850_config_file_parser.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int running = 0;
static IedServer iedServer = NULL;

void sigint_handler(int signalId)
{
    running = 0;
}

int main(int argc, char** argv)
{
    const char* configFile = "model.cfg";
    int tcpPort = 102;

    if (argc > 1) {
        configFile = argv[1];
    }
    if (argc > 2) {
        tcpPort = atoi(argv[2]);
    }

    printf("========================================\n");
    printf("Debug: Loading model\n");
    printf("========================================\n");
    printf("Config file: %s\n", configFile);

    // 检查文件
    FILE* file = fopen(configFile, "rb");
    if (file == NULL) {
        printf("ERROR: Cannot open %s\n", configFile);
        return -1;
    }

    // 读取并显示文件内容
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n", size);
    
    char* content = (char*)malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    
    printf("File content:\n---BEGIN---\n%s\n---END---\n", content);
    
    // 检查格式
    if (strstr(content, "MODEL(") != NULL) {
        printf("[SUCC] Found MODEL( declaration\n");
    } else {
        printf("[ERR] No MODEL( declaration found!\n");
    }
    
    if (strstr(content, "LD(") != NULL) {
        printf("[SUCC] Found LD( declaration\n");
    } else {
        printf("[ERR] No LD( declaration found!\n");
    }
    
    free(content);

    // 尝试加载
    printf("\nCalling ConfigFileParser_createModelFromConfigFileEx...\n");
    IedModel* iedModel = ConfigFileParser_createModelFromConfigFileEx(configFile);
    
    if (iedModel == NULL) {
        printf("ERROR: Model loading failed!\n");
        printf("Check that model.cfg format is correct.\n");
        return -1;
    }

    printf("Model loaded successfully!\n");
    printf("Model name: %s\n", iedModel->name);

    // 创建服务器
    iedServer = IedServer_create(iedModel);
    
    // 启动服务器
    IedServer_start(iedServer, tcpPort);
    
    if (!IedServer_isRunning(iedServer)) {
        printf("ERROR: Server failed to start\n");
        IedServer_destroy(iedServer);
        IedModel_destroy(iedModel);
        return -1;
    }

    printf("Server started on port %d\n", tcpPort);
    printf("Press Ctrl+C to stop\n");

    running = 1;
    signal(SIGINT, sigint_handler);

    while (running) {
        Thread_sleep(1000);
    }

    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);
    IedModel_destroy(iedModel);

    return 0;
}
