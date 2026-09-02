/*
 * client_example_ClientGooseControl.c
 *
 * 功能：演示如何通过MMS协议，远程管理服务器上的GOOSE控制块（GoCB）。
 * 通过读取和修改GoCB参数，可以实现对GOOSE发送行为的远程控制。
 *
 * 注意：此示例需配合 server_example_basic_io 或 server_example_goose 运行。
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>

#include "hal_thread.h"

int main(int argc, char** argv)
{
    char* hostname;
    int tcpPort = 102;

    /* 解析命令行参数：服务器IP和端口 */
    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    IedClientError error;
    IedConnection con = IedConnection_create();

    /* 1. 同步连接服务器 */
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK)
    {
        /* 2. 从服务器读取指定GoCB的当前配置值 */
        /*    这里读取的是 "simpleIOGenericIO/LLN0.gcbEvents" 这个GoCB */
        ClientGooseControlBlock goCB = IedConnection_getGoCBValues(con, &error, "simpleIOGenericIO/LLN0.gcbEvents", NULL);

        /* 打印读取到的值，作为基线参考 */
        bool GoEna = ClientGooseControlBlock_getGoEna(goCB);
        printf("GoEna Value: %d\n", GoEna);

        const char* id = ClientGooseControlBlock_getGoID(goCB);
        printf("GoID Value: %s\n", id);

        const char* datset = ClientGooseControlBlock_getDatSet(goCB);
        printf("GoDatset Value: %s\n", datset);

        /* 3. 在客户端本地修改GoCB配置（仅修改内存，不涉及服务器） */
        ClientGooseControlBlock_setGoID(goCB, "analog");
        ClientGooseControlBlock_setDatSet(goCB, "simpleIOGenericIO/LLN0$AnalogValues");
        ClientGooseControlBlock_setGoEna(goCB, false);

        /* 4. 将修改同步到服务器 */
        /*    注意：按IEC 61850标准，GoID和DatSet通常为只读属性，
         *     只有GoEna是可写的，因此此操作可能会失败，并返回错误码。
         *     这正是代码注释 "Throws error because only GoEna is writeable" 的含义。
         */
        IedConnection_setGoCBValues(con, &error, goCB, GOCB_ELEMENT_GO_ID | GOCB_ELEMENT_DATSET | GOCB_ELEMENT_GO_ENA, true);

        if (error != IED_ERROR_OK)
            printf("Fail to Set Values to Server (code: %i)\n", error);

        /* 5. 再次从服务器读取，验证修改结果 */
        goCB = IedConnection_getGoCBValues(con, &error, "simpleIOGenericIO/LLN0.gcbEvents", NULL);

        bool GoEnaUpdate = ClientGooseControlBlock_getGoEna(goCB);
        printf("GoEna Value: %d\n", GoEnaUpdate);

        const char* idUpdate = ClientGooseControlBlock_getGoID(goCB);
        printf("GoID Value: %s\n", idUpdate);

        const char* datsetUpdate = ClientGooseControlBlock_getDatSet(goCB);
        printf("GoDatset Value: %s\n", datsetUpdate);

        printf("\n");

        /* 保持连接一段时间，便于观察 */
        Thread_sleep(50000);

close_connection:
        IedConnection_close(con);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    IedConnection_destroy(con);

    return 0;
}