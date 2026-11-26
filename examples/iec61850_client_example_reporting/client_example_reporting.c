/*
 * client_example_reporting.c
 * 这个客户端示例订阅缓存报告块（BRCB）：server_example_basic_io 静态模型中的 Measurements
 * 配套服务端：server_example_basic_io（IED 名 simpleIO + LD GenericIO）
 * 数据集 Measurements：AnIn1~4 的 mag.f 和 q（共 8 个成员）
 * RCB：LLN0.BR.Measurements01（buffered="true", rptID="Measurements"）
 * MMS domain = IED 名(simpleIO) + LDevice inst(GenericIO) = simpleIOGenericIO
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

    /* BRCB 报告带 EntryID，可据此做断线续传 */
    if (ClientReport_getEntryId(report) != NULL) {
        MmsValue* entryId = ClientReport_getEntryId(report);
        uint8_t* buf = MmsValue_getOctetStringBuffer(entryId);
        printf("  entryID: %02x%02x%02x%02x%02x%02x%02x%02x\n",
               buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    }

    /* bufOvfl=true 表示缓存溢出、有报告被淘汰，应考虑触发 GI 重新同步 */
    if (ClientReport_getBufOvfl(report))
        printf("  *** buffer overflow: some reports were lost, consider a GI ***\n");

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
    printf("-------------------------------------------------------------------\n");
    printf("-------------------   IEC61850 RCB CLI Test   ------------- -------\n");
    printf("-------------------------------------------------------------------\n");
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

        /* read data set directory —— 数据集改为 Measurements */
        dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, "simpleIOGenericIO/LLN0.Measurements", NULL);
        if (error != IED_ERROR_OK) {
            printf("Reading data set directory failed!\n");
        //    goto exit_error;
        }

        /* read data set */
        clientDataSet = IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.Measurements", NULL);
        if (clientDataSet == NULL) {
            printf("failed to read dataset\n");
           // goto exit_error;
        }

        /* Read RCB values —— BR 表示 BRCB，实例名带 01 后缀 */
        rcb = IedConnection_getRCBValues(con, &error, "simpleIOGenericIO/LLN0.BR.Measurements01", NULL);
        if (error != IED_ERROR_OK) {
            printf("getRCBValues service error!\n");
            goto exit_error;
        }

        /* prepare the parameters of the RCB */
        // 1. BRCB 没有 Resv（那是 URCB 的），改用 PurgeBuf 清空历史缓存，从头开始收
        ClientReportControlBlock_setPurgeBuf(rcb, true);
        // 2. TrgOps触发条件：ICD 里该 BRCB 只写了 period="false"，需由客户端显式指定
        ClientReportControlBlock_setTrgOps(rcb, TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_GI);
        // 3. DatSet 数据集引用
        ClientReportControlBlock_setDataSetReference(rcb, "simpleIOGenericIO/LLN0$Measurements"); /* NOTE the "$" instead of "." ! */
        // 4. RptEna 报告使能开关打开报告，服务器才开始真正给你发报告
        ClientReportControlBlock_setRptEna(rcb, true);
        // 5. GI 总召唤标志
        ClientReportControlBlock_setGI(rcb, true);

        /* Configure the report receiver */
        IedConnection_installReportHandler(con, "simpleIOGenericIO/LLN0.BR.Measurements01", ClientReportControlBlock_getRptId(rcb), reportCallbackFunction,
                (void*) dataSetDirectory);

#if 0
        /* Write RCB parameters and enable report —— 掩码把 RESV 换成 PURGE_BUF */
        // 把rcb发给服务器 -> client_report_control.c 实际执行的就是MMS写操作
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_PURGE_BUF | RCB_ELEMENT_DATSET | RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_GI, true);
        if (error != IED_ERROR_OK) {
            printf("setRCBValues service error!\n");
            goto exit_error;
        }
#endif
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_PURGE_BUF | RCB_ELEMENT_DATSET | RCB_ELEMENT_TRG_OPS, false);
        if (error != IED_ERROR_OK) {
            printf("setRCBValues params error!\n");
            goto exit_error;
        }

        // 步骤2：单独使能
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
        if (error != IED_ERROR_OK) {
            printf("setRCBValues enable error!\n");
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

