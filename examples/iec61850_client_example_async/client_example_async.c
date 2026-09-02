/*
 * client_example_async.c
 * 功能说明：
 * 这个示例程序演示了如何使用 libiec61850 的异步客户端 API
 * 所有操作都是非阻塞的，通过回调函数处理结果
 *
 * 主要演示内容：
 * - 异步连接
 * - 异步读取单个数据对象
 * - 异步读取/写入数据集
 * - 异步获取变量类型信息
 * - 异步获取服务器目录
 * - 异步控制操作
 *
 * 注意：本示例需要配合 server_example_basic_io 使用
 * 
 *          场景	                同步	    异步
 *   命令行工具（file-tool）	    ✅	        ❌
 *   GUI 应用程序	                ❌	       ✅
 *   需要同时监控多个 IED	         ❌	        ✅
 *   嵌入式实时系统	                 ❌	        ✅
 *   简单的测试/验证	             ✅	        ❌
 *   需要非阻塞的 Web 服务	         ❌	        ✅
 */


#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>

#include "hal_thread.h"

/* 全局变量：保存客户端数据集引用，用于后续操作 */
static ClientDataSet clientDataSet = NULL;

/**
 * 打印 MMS 值的辅助函数
 * @param name  值的名称（用于显示）
 * @param value 要打印的 MMS 值
 */
static void printValue(char* name, MmsValue* value)
{
    char buf[1000];
    MmsValue_printToBuffer(value, buf, 1000);
    printf("%s: %s\n", name, buf);
}

/**
 * 读取单个数据对象的回调函数
 * 当异步读取操作完成时被调用
 * @param invokeId  调用ID（用于匹配请求和响应）
 * @param parameter 用户自定义参数（这里传入的是变量名）
 * @param err       错误码（IED_ERROR_OK 表示成功）
 * @param value     读取到的 MMS 值
 */
static void readObjectHandler(uint32_t invokeId, void* parameter, IedClientError err, MmsValue* value)
{
    if (err == IED_ERROR_OK)
    {
        /* 打印读取到的值 */
        printValue((char*) parameter, value);

        /* 使用完毕后释放 MMS 值对象 */
        MmsValue_delete(value);
    }
    else
    {
        printf("Failed to read object %s (err=%i)\n", (char*) parameter, err);
    }
}

/**
 * 读取数据集的回调函数
 * 当异步读取数据集操作完成时被调用
 * @param invokeId  调用ID
 * @param parameter 用户自定义参数
 * @param err       错误码
 * @param dataSet   读取到的数据集对象（包含数据集中所有数据项的值）
 */
static void readDataSetHandler(uint32_t invokeId, void* parameter, IedClientError err, ClientDataSet dataSet)
{
    if (err == IED_ERROR_OK)
    {
        clientDataSet = dataSet;
        printf("Data set has %d entries\n", ClientDataSet_getDataSetSize(dataSet));

        /* 获取数据集中的所有值 */
        MmsValue* values = ClientDataSet_getValues(dataSet);

        /* 数据集的值的类型是 MMS 数组（MMS_ARRAY） */
        if (MmsValue_getType(values) == MMS_ARRAY)
        {
            int i;
            for (i = 0; i < MmsValue_getArraySize(values); i++)
            {
                printf("  [%i]", i);
                printValue("", MmsValue_getElement(values, i));
            }
        }
    }
    else
    {
        printf("Failed to read data set (err=%i)\n", err);
    }
}

/**
 * 写入数据集的回调函数
 * 当异步写入数据集操作完成时被调用
 * @param invokeId       调用ID
 * @param parameter      用户自定义参数
 * @param err            错误码
 * @param accessResults  每个数据项的访问结果列表（可能包含单个元素的写入状态）
 */
static void
writeDataSetHandler(uint32_t invokeId, void* parameter, IedClientError err, LinkedList /* <MmsValue*> */accessResults)
{
    if (err == IED_ERROR_OK)
    {
        if (accessResults)
        {
            int i = 0;
            LinkedList element = LinkedList_getNext(accessResults);
            /* 遍历每个数据项的写入结果 */
            while (element)
            {
                MmsValue* accessResultValue = (MmsValue*) LinkedList_getData(element);
                printf("  access-result[%i]", i);
                printValue("", accessResultValue);
                element = LinkedList_getNext(element);
                i++;
            }
            /* 释放结果链表及其中的 MMS 值 */
            LinkedList_destroyDeep(accessResults, (LinkedListValueDeleteFunction) MmsValue_delete);
        }
    }
    else
    {
        printf("Failed to write data set (err=%i)\n", err);
    }
}

/**
 * 报告回调函数
 * 当服务器通过报告控制块（RCB）主动推送报告时被调用
 * @param parameter 用户自定义参数
 * @param report    接收到的报告对象
 */
static void reportCallbackFunction(void* parameter, ClientReport report)
{
    /* 获取报告中的数据值 */
    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);
    printf("received report for %s\n", ClientReport_getRcbReference(report));

    /* 遍历报告中的前 4 个数据项 */
    int i;
    for (i = 0; i < 4; i++)
    {
        /* 获取数据项被包含在报告中的原因（如数据变化、品质变化等） */
        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);

        /* 如果该数据项确实被包含在报告中（即发生了变化） */
        if (reason != IEC61850_REASON_NOT_INCLUDED)
        {
            printf("  GGIO1.SPCSO%i.stVal: %i (included for reason %i)\n", i,
                    MmsValue_getBoolean(MmsValue_getElement(dataSetValues, i)), reason);
        }
    }
}

/**
 * 获取变量规格信息的回调函数
 * 当异步获取变量类型定义操作完成时被调用
 * @param invokeId  调用ID
 * @param parameter 用户自定义参数（变量名）
 * @param err       错误码
 * @param spec      变量规格信息对象（包含类型、数组维度等）
 */
static void
getVarSpecHandler (uint32_t invokeId, void* parameter, IedClientError err, MmsVariableSpecification* spec)
{
    if (err == IED_ERROR_OK)
    {
        printf("variable: %s has type %d\n", (char*) parameter, MmsVariableSpecification_getType(spec));

        /* 使用完毕后释放规格对象 */
        MmsVariableSpecification_destroy(spec);
    }
    else
    {
        printf("Failed to get variable specification for object %s (err=%i)\n", (char*) parameter, err);
    }
}

/**
 * 获取名称列表的回调函数
 * 用于处理逻辑设备变量或数据集名称列表的异步响应
 * @param invokeId     调用ID
 * @param parameter    用户自定义参数（逻辑设备名称）
 * @param err          错误码
 * @param nameList     名称列表（链表中的每个元素是一个 char* 字符串）
 * @param moreFollows  是否还有更多名称（用于分页）
 */
static void
getNameListHandler(uint32_t invokeId, void* parameter, IedClientError err, LinkedList nameList, bool moreFollows)
{
    if (err != IED_ERROR_OK)
    {
        printf("Get name list error: %d\n", err);
    }
    else
    {
        char* ldName = (char*) parameter;
        LinkedList element = LinkedList_getNext(nameList);
        /* 遍历并打印所有名称 */
        while (element)
        {
            char* variableName = (char*) LinkedList_getData(element);

            printf("  %s/%s\n", ldName, variableName);

            element = LinkedList_getNext(element);
        }

        /* 释放名称列表（注意：链表中的字符串不需要释放，因为它们由库管理） */
        LinkedList_destroy(nameList);

        /* 释放用户参数 */
        free(ldName);
    }
}

/**
 * 获取服务器目录的回调函数
 * 当异步获取服务器目录操作完成时被调用
 * 这个函数会遍历所有逻辑设备（LD），并为每个 LD 异步获取其变量和数据集的名称列表
 * @param invokeId     调用ID
 * @param parameter    用户自定义参数（这里传入的是 IedConnection 对象）
 * @param err          错误码
 * @param nameList     服务器目录名称列表（所有逻辑设备名称）
 * @param moreFollows  是否还有更多条目
 */
static void
getServerDirectoryHandler(uint32_t invokeId, void* parameter, IedClientError err, LinkedList nameList, bool moreFollows)
{
    IedConnection con = (IedConnection) parameter;

    if (err != IED_ERROR_OK)
    {
        printf("Get server directory error: %d\n", err);
    }
    else
    {
        LinkedList element = LinkedList_getNext(nameList);

        /* 遍历每个逻辑设备，获取其内部的变量和数据集列表 */
        while (element)
        {
            char* ldName = (char*) LinkedList_getData(element);
            IedClientError cerr;
            printf("LD: %s variables:\n", ldName);
            /* 异步获取逻辑设备下的所有变量（FCD/FCDA）名称 */
            IedConnection_getLogicalDeviceVariablesAsync(con, &cerr, ldName, NULL, NULL, 
                                                          getNameListHandler, strdup(ldName));

            printf("LD: %s data sets:\n", ldName);
            /* 异步获取逻辑设备下的所有数据集名称 */
            IedConnection_getLogicalDeviceDataSetsAsync(con, &err, ldName, NULL, NULL, 
                                                        getNameListHandler, strdup(ldName));
            element = LinkedList_getNext(element);
        }
        /* 释放服务器目录名称列表 */
        LinkedList_destroy(nameList);
    }
}

/**
 * 控制操作的回调函数
 * 当异步控制操作完成时被调用
 * 
 * @param invokeId  调用ID
 * @param parameter 用户自定义参数
 * @param err       错误码
 * @param type      控制操作类型（操作、取消、时间激活等）
 * @param success   控制操作是否成功
 */
static void
controlActionHandler(uint32_t invokeId, void* parameter, IedClientError err, ControlActionType type, bool success)
{
    printf("control: ID: %d type: %i err: %d success: %i\n", invokeId, type, err, success);
}

/**
 * 程序主函数
 */
int main(int argc, char** argv)
{
    char* hostname;
    int tcpPort = 102;

    /* 解析命令行参数：主机名和端口 */
    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    IedClientError error;

    /* 创建客户端连接对象 */
    IedConnection con = IedConnection_create();

    /* 异步连接服务器（立即返回，不阻塞） */
    IedConnection_connectAsync(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK)
    {
        bool success = true;

        /* 轮询等待连接完成（最多等待到连接成功或失败） */
        while (IedConnection_getState(con) != IED_STATE_CONNECTED)
        {
            /* 如果连接关闭，说明连接失败 */
            if (IedConnection_getState(con) == IED_STATE_CLOSED)
            {
                success = false;
                break;
            }

            Thread_sleep(10);  /* 等待 10ms 再检查 */
        }

        if (success)
        {
            /* ========== 1. 获取服务器目录 ========== */
            IedConnection_getServerDirectoryAsync(con, &error, NULL, NULL, 
                                                   getServerDirectoryHandler, con);

            if (error != IED_ERROR_OK)
            {
                printf("read server directory error %i\n", error);
            }

            Thread_sleep(1000);  /* 等待异步操作完成 */

            /* ========== 2. 读取单个数据对象 ========== */
            IedConnection_readObjectAsync(con, &error, 
                                           "simpleIOGenericIO/GGIO1.AnIn1.mag.f", 
                                           IEC61850_FC_MX, 
                                           readObjectHandler, 
                                           "simpleIOGenericIO/GGIO1.AnIn1.mag.f");

            if (error != IED_ERROR_OK)
            {
                printf("read object error %i\n", error);
            }

            /* ========== 3. 读取另一个数据对象 ========== */
            IedConnection_readObjectAsync(con, &error, 
                                           "simpleIOGenericIO/GGIO1.AnIn2.mag.f", 
                                           IEC61850_FC_MX, 
                                           readObjectHandler, 
                                           "simpleIOGenericIO/GGIO1.AnIn2.mag.f");

            if (error != IED_ERROR_OK)
            {
                printf("read object error %i\n", error);
            }

            /* ========== 4. 获取变量类型信息 ========== */
            IedConnection_getVariableSpecificationAsync(con, &error, 
                                                         "simpleIOGenericIO/GGIO1.AnIn1", 
                                                         IEC61850_FC_MX, 
                                                         getVarSpecHandler, 
                                                         "simpleIOGenericIO/GGIO1.AnIn1");

            if (error != IED_ERROR_OK)
            {
                printf("get variable specification error %i\n", error);
            }

            /* ========== 5. 读取数据集值 ========== */
            IedConnection_readDataSetValuesAsync(con, &error, 
                                                  "simpleIOGenericIO/LLN0.Events", 
                                                  NULL, 
                                                  readDataSetHandler, NULL);

            if (error != IED_ERROR_OK)
            {
                printf("read data set error %i\n", error);
            }

            /* ========== 6. 写入数据集值 ========== */
            /* 构造要写入的数据：4 个布尔值 [true, false, true, false] */
            LinkedList values = LinkedList_create();
            LinkedList_add(values, MmsValue_newBoolean(true));
            LinkedList_add(values, MmsValue_newBoolean(false));
            LinkedList_add(values, MmsValue_newBoolean(true));
            LinkedList_add(values, MmsValue_newBoolean(false));

            IedConnection_writeDataSetValuesAsync(con, &error, 
                                                   "simpleIOGenericIO/LLN0.Events", 
                                                   values, 
                                                   writeDataSetHandler, NULL);

            if (error != IED_ERROR_OK)
            {
                printf("write data set error %i\n", error);
            }

            /* 释放 values 链表（注意：内部 MmsValue 会在回调中释放） */
            LinkedList_destroyDeep(values, (LinkedListValueDeleteFunction) MmsValue_delete);

            Thread_sleep(1000);  /* 等待异步操作完成 */

            /* ========== 7. 控制操作 ========== */
            /* 创建一个控制对象客户端，用于操作 GGIO1.SPCSO1 这个可控开关 */
            ControlObjectClient controlClient = ControlObjectClient_create("simpleIOGenericIO/GGIO1.SPCSO1", con);

            if (controlClient != NULL)
            {
                /* 设置控制操作的来源信息（用于追溯） */
                ControlObjectClient_setOrigin(controlClient, "test1", CONTROL_ORCAT_AUTOMATIC_REMOTE);

                /* 构造控制值：true（合闸/启动） */
                MmsValue* ctlVal = MmsValue_newBoolean(true);

                /* 异步发送操作命令 */
                ControlObjectClient_operateAsync(controlClient, &error, ctlVal, 0, 
                                                  controlActionHandler, NULL);

                if (error != IED_ERROR_OK)
                {
                    printf("Failed to send operate %i\n", error);
                }

                MmsValue_delete(ctlVal);  /* 释放控制值 */
            }
            else
            {
                printf("Failed to connect to control object\n");
            }
        }

        Thread_sleep(1000);  /* 等待所有异步操作完成 */

        /* ========== 8. 异步释放连接 ========== */
        IedConnection_releaseAsync(con, &error);

        if (error != IED_ERROR_OK)
        {
            printf("Release returned error: %d\n", error);
        }
        else
        {
            /* 等待连接完全关闭 */
            while (IedConnection_getState(con) != IED_STATE_CLOSED)
            {
                Thread_sleep(10);
            }
        }
    }
    else
    {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    /* 清理资源 */
    if (clientDataSet)
        ClientDataSet_destroy(clientDataSet);

    IedConnection_destroy(con);
    return 0;
}
