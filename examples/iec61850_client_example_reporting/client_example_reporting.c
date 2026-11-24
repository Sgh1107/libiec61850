/*
 * client_example_reporting.c
 * 这个客户端示例用的是非缓存报告块
 * This example is intended to be used with server_example_basic_io or server_example_goose.
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#include "hal_thread.h"

static int running = 0;

void sigint_handler(int signalId)
{
    running = 0;
}


void reportCallbackFunction(void* parameter, ClientReport report)
{
    LinkedList dataSetDirectory = (LinkedList) parameter;
    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);
    printf("received report for %s with rptId %s\n", ClientReport_getRcbReference(report), ClientReport_getRptId(report));

    if (ClientReport_hasTimestamp(report)) {        // 有时间戳 缓存型
        time_t unixTime = ClientReport_getTimestamp(report) / 1000;
#ifdef WIN32
		char* timeBuf = ctime(&unixTime);
#else
		char timeBuf[30];
		ctime_r(&unixTime, timeBuf);
#endif
        printf("  report contains timestamp (%u): %s", (unsigned int) unixTime, timeBuf);
    }

    if (dataSetDirectory) {
        int i;
        for (i = 0; i < LinkedList_size(dataSetDirectory); i++) {
            ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);
            if (reason != IEC61850_REASON_NOT_INCLUDED) {
                char valBuffer[500];
                sprintf(valBuffer, "no value");
                if (dataSetValues) {
                    MmsValue* value = MmsValue_getElement(dataSetValues, i);
                    if (value) {
                        MmsValue_printToBuffer(value, valBuffer, 500);
                    }
                }
                LinkedList entry = LinkedList_get(dataSetDirectory, i);
                char* entryName = (char*) entry->data;
                printf("  %s (included for reason %i): %s\n", entryName, reason, valBuffer);
            }
        }
    }

}

int main(int argc, char** argv)
{
    char* hostname;
    int tcpPort = 102;

    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    running = 1;
    signal(SIGINT, sigint_handler);
    IedClientError error;
    IedConnection con = IedConnection_create();
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {
        ClientReportControlBlock rcb = NULL;
        ClientDataSet clientDataSet = NULL;
        LinkedList dataSetDirectory = NULL;

        /* read data set directory */
        dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);
        if (error != IED_ERROR_OK) {
            printf("Reading data set directory failed!\n");
        //    goto exit_error;
        }

        /* read data set */
        clientDataSet = IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);
        if (clientDataSet == NULL) {
            printf("failed to read dataset\n");
           // goto exit_error;
        }

        /* Read RCB values */
        rcb = IedConnection_getRCBValues(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB01", NULL);
        if (error != IED_ERROR_OK) {
            printf("getRCBValues service error!\n");
            goto exit_error;
        }

        /* prepare the parameters of the RCP */
        // 1. 设为 true 表示本客户端要独占这个 RCB，防止别的客户端同时使能它
        ClientReportControlBlock_setResv(rcb, true);
        // 2. TrgOps触发条件 [TRG_OPT_DATA_CHANGED 数值变化触发] [TRG_OPT_QUALITY_CHANGED 品质变化触发] [TRG_OPT_GI 响应总召唤触发]
        ClientReportControlBlock_setTrgOps(rcb, TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_GI);
        // 3. DatSet 数据集引用
        ClientReportControlBlock_setDataSetReference(rcb, "simpleIOGenericIO/LLN0$Events"); /* NOTE the "$" instead of "." ! */
        // 4. RptEna 报告使能开关打开报告，服务器才开始真正给你发报告
        ClientReportControlBlock_setRptEna(rcb, true);
        // 5. GI 总召唤标志
        ClientReportControlBlock_setGI(rcb, true);

        /* Configure the report receiver */
        IedConnection_installReportHandler(con, "simpleIOGenericIO/LLN0.RP.EventsRCB", ClientReportControlBlock_getRptId(rcb), reportCallbackFunction,
                (void*) dataSetDirectory);

        /* Write RCB parameters and enable report */
        // 把rcb发给服务器 -> client_report_control.c 实际执行的就是MMS写操作
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RESV | RCB_ELEMENT_DATSET | RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_GI, true);
        if (error != IED_ERROR_OK) {
            printf("setRCBValues service error!\n");
            goto exit_error;
        }

        Thread_sleep(1000);

        /* Trigger GI Report */
        ClientReportControlBlock_setGI(rcb, true);
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_GI, true);
        if (error != IED_ERROR_OK) {
            printf("Error triggering a GI report (code: %i)\n", error);
        }

        while (running) {
            Thread_sleep(10);
            IedConnectionState conState = IedConnection_getState(con);
            if (conState != IED_STATE_CONNECTED) {
                printf("Connection closed by server!\n");
                running = 0;
            }
        }

        /* disable reporting */
        ClientReportControlBlock_setRptEna(rcb, false);
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);

exit_error:
        IedConnection_close(con);
        if (clientDataSet)
            ClientDataSet_destroy(clientDataSet);

        if (rcb)
            ClientReportControlBlock_destroy(rcb);

        if (dataSetDirectory)
            LinkedList_destroy(dataSetDirectory);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    IedConnection_destroy(con);
    return 0;
}


