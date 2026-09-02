/*
 * mms_utility.c
 *
 * 功能说明：
 * 这是一个 MMS（制造报文规范）协议层的通用命令行工具。
 * 它基于 libiec61850 的底层 MMS 客户端 API，提供了访问 MMS 服务器的各种功能。
 *
 * 与 file-tool 的区别：
 *   - file-tool: 基于 IEC 61850 上层服务，仅支持文件操作
 *   - mms_utility: 基于 MMS 底层协议，支持变量、域、文件、日志等完整 MMS 服务
 *
 * 使用场景：
 *   - MMS 协议学习和调试
 *   - 验证服务器的 MMS 实现是否正确
 *   - 访问 IEC 61850 上层未封装的底层 MMS 功能
 *   - 读取服务器的日志（Journal）条目
 *   - 配合 -m 选项分析原始 MMS 报文
 *
 * 注意：此工具运行在客户端，需要连接到远程 MMS 服务器
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>          /* getopt() 函数，用于命令行参数解析 */
#include "string_utilities.h"
#include "iec61850_common.h"
#include "mms_client_connection.h"
#include "conversions.h"

/**
 * 打印帮助信息
 */
static void
print_help()
{
    printf("MMS utility (libiec61850 %s) options:\n", LibIEC61850_getVersionString());
    printf("-h <hostname> specify hostname\n");
    printf("-p <port> specify port\n");
    printf("-l <max_pdu_size> specify maximum PDU size\n");
    printf("-d show list of MMS domains\n");
    printf("-i show server identity\n");
    printf("-t <domain_name> show domain directory\n");
    printf("-r <variable_name> read domain variable\n");
    printf("-c <component_name> specify component name for variable read\n");
    printf("-a <domain_name> specify domain for read or write command\n");
    printf("-f show file list\n");
    printf("-g <filename> get file attributes\n");
    printf("-x <filename> delete file\n");
    printf("-j <domainName/journalName> read journal\n");
    printf("-v <variable list_name> read domain variable list\n");
    printf("-z <variable list_name> get domain variable list directory\n");
    printf("-y <index> array index for read access\n");
    printf("-m print raw MMS messages\n");
}

/**
 * MMS 文件目录回调函数
 * 当遍历文件目录时被调用，每找到一个文件就打印文件名
 * 
 * @param parameter    用户参数（用于传递上下文）
 * @param filename     文件名
 * @param size         文件大小（字节）
 * @param lastModified 最后修改时间（毫秒时间戳）
 */
static void
mmsFileDirectoryHandler(void* parameter, char* filename, uint32_t size, uint64_t lastModified)
{
    char* lastName = (char*) parameter;

    strcpy(lastName, filename);  /* 保存最后一个文件名，用于分页续传 */

    printf("%s\n", filename);
}

/**
 * MMS 获取文件属性回调函数
 * 打印文件的详细信息：文件名、大小、最后修改时间
 * 
 * @param parameter    用户参数
 * @param filename     文件名
 * @param size         文件大小（字节）
 * @param lastModified 最后修改时间（毫秒时间戳）
 */
static void
mmsGetFileAttributeHandler(void* parameter, char* filename, uint32_t size, uint64_t lastModified)
{
    char gtString[30];
    /* 将毫秒时间戳转换为 GeneralizedTime 字符串格式（如 "20260902031820.482Z"） */
    Conversions_msTimeToGeneralizedTime(lastModified, (uint8_t*) gtString);

    printf("FILENAME: %s\n", filename);
    printf("SIZE: %u\n", size);
    printf("DATE: %s\n", gtString);
}

/**
 * 打印日志条目
 * 遍历并打印 Journal（日志）中的所有条目及其变量
 * 
 * @param journalEntries 日志条目链表
 */
static void
printJournalEntries(LinkedList journalEntries)
{
    char buf[1024];

    LinkedList journalEntriesElem = LinkedList_getNext(journalEntries);

    while (journalEntriesElem != NULL) {

        MmsJournalEntry journalEntry = (MmsJournalEntry) LinkedList_getData(journalEntriesElem);

        /* 打印日志条目ID */
        MmsValue_printToBuffer(MmsJournalEntry_getEntryID(journalEntry), buf, 1024);
        printf("EntryID: %s\n", buf);
        
        /* 打印日志发生时间 */
        MmsValue_printToBuffer(MmsJournalEntry_getOccurenceTime(journalEntry), buf, 1024);
        printf("  occurence time: %s\n", buf);

        /* 遍历并打印该日志条目中的所有变量 */
        LinkedList journalVariableElem = LinkedList_getNext(journalEntry->journalVariables);

        while (journalVariableElem != NULL) {

            MmsJournalVariable journalVariable = (MmsJournalVariable) LinkedList_getData(journalVariableElem);

            printf("   variable-tag: %s\n", MmsJournalVariable_getTag(journalVariable));
            MmsValue_printToBuffer(MmsJournalVariable_getValue(journalVariable), buf, 1024);
            printf("   variable-value: %s\n", buf);

            journalVariableElem = LinkedList_getNext(journalVariableElem);
        }

        journalEntriesElem = LinkedList_getNext(journalEntriesElem);
    }
}

/**
 * 打印原始 MMS 报文的回调函数
 * 用于调试，以十六进制格式打印发送和接收的原始 MMS 报文
 * 
 * @param parameter      用户参数
 * @param message        报文数据
 * @param messageLength  报文长度
 * @param received       true=接收，false=发送
 */
void
printRawMmsMessage(void* parameter, uint8_t* message, int messageLength, bool received)
{
    if (received)
        printf("RECV: ");
    else
        printf("SEND: ");

    int i;
    for (i = 0; i < messageLength; i++) {
        printf("%02x ", message[i]);
    }

    printf("\n");
}

/**
 * 程序主函数
 */
int main(int argc, char** argv)
{
    int returnCode = 0;

    /* ===== 默认参数 ===== */
    char* hostname = StringUtils_copyString("localhost");
    int tcpPort = 102;              /* MMS 标准端口 */
    int maxPduSize = 65000;         /* 最大 PDU 大小（65KB） */
    int arrayIndex = -1;            /* 数组索引，-1 表示不使用数组访问 */

    char* domainName = NULL;
    char* variableName = NULL;
    char* componentName = NULL;
    char* filename = NULL;
    char* journalName = NULL;

    /* ===== 操作标志 ===== */
    int readDeviceList = 0;          /* -d: 列出所有域 */
    int getDeviceDirectory = 0;      /* -t: 获取域目录 */
    int identifyDevice = 0;          /* -i: 获取服务器身份 */
    int readWriteHasDomain = 0;      /* -a: 指定域 */
    int readVariable = 0;            /* -r: 读取变量 */
    int showFileList = 0;            /* -f: 列出文件 */
    int getFileAttributes = 0;       /* -g: 获取文件属性 */
    int readJournal = 0;             /* -j: 读取日志 */
    int printRawMmsMessages = 0;     /* -m: 打印原始报文 */
    int deleteFile = 0;              /* -x: 删除文件 */
    int readVariableList = 0;        /* -v: 读取变量列表（数据集）值 */
    int readDataSetDirectory = 0;    /* -z: 获取变量列表目录（数据集成员） */

    int c;

    /* ===== 解析命令行参数 ===== */
    while ((c = getopt(argc, argv, "mifdh:p:l:t:a:r:g:j:x:v:c:y:z:")) != -1)
    {
        switch (c) {
        case 'm':
            printRawMmsMessages = 1;
            break;

        case 'h':
            free(hostname);
            hostname = StringUtils_copyString(optarg);
            break;
        case 'p':
            tcpPort = atoi(optarg);
            break;
        case 'l':
            maxPduSize = atoi(optarg);
            break;
        case 'd':
            readDeviceList = 1;
            break;
        case 'i':
            identifyDevice = 1;
            break;
        case 't':
            getDeviceDirectory = 1;
            domainName = StringUtils_copyString(optarg);
            break;
        case 'a':
            readWriteHasDomain = 1;
            domainName = StringUtils_copyString(optarg);
            break;
        case 'r':
            readVariable = 1;
            variableName = StringUtils_copyString(optarg);
            break;
        case 'c':
            componentName = StringUtils_copyString(optarg);
            break;
        case 'v':
            readVariableList = 1;
            variableName = StringUtils_copyString(optarg);
            break;
        case 'z':
            readDataSetDirectory = 1;
            variableName = StringUtils_copyString(optarg);
            break;

        case 'f':
            showFileList = 1;
            break;
        case 'g':
            getFileAttributes = 1;
            filename = StringUtils_copyString(optarg);
            break;
        case 'x':
            deleteFile = 1;
            filename = StringUtils_copyString(optarg);
            break;

        case 'j':
            readJournal = 1;
            journalName = StringUtils_copyString(optarg);
            break;

        case 'y':
            arrayIndex = atoi(optarg);
            break;

        default:
            print_help();
            return 0;
        }
    }

    /* ===== 创建 MMS 连接 ===== */
    MmsConnection con = MmsConnection_create();

    MmsError error;

    /* 设置本地细节：最大 MMS PDU 大小 */
    MmsConnection_setLocalDetail(con, maxPduSize);

    /* 如果启用了 -m 选项，安装原始报文打印回调 */
    if (printRawMmsMessages)
        MmsConnection_setRawMessageHandler(con, (MmsRawMessageHandler) printRawMmsMessage, NULL);

    /* ===== 连接到服务器 ===== */
    if (!MmsConnection_connect(con, &error, hostname, tcpPort))
    {
        printf("MMS connect failed!\n");

        if (error != MMS_ERROR_NONE)
            returnCode = error;

        goto exit;
    }
    else
        printf("MMS connected.\n");

    /* ===== 1. 获取服务器身份信息 (-i) ===== */
    if (identifyDevice)
    {
        MmsServerIdentity* identity =
                MmsConnection_identify(con, &error);

        if (error != MMS_ERROR_NONE)
            returnCode = error;

        if (identity != NULL)
        {
            printf("\nServer identity:\n----------------\n");
            printf("  vendor:\t%s\n", identity->vendorName);
            printf("  model:\t%s\n", identity->modelName);
            printf("  revision:\t%s\n", identity->revision);
        }
        else
            printf("Reading server identity failed!\n");
    }

    /* ===== 2. 列出所有域 (-d) ===== */
    if (readDeviceList)
    {
        printf("\nDomains present on server:\n--------------------------\n");
        LinkedList nameList = MmsConnection_getDomainNames(con, &error);

        if (error != MMS_ERROR_NONE)
            returnCode = error;

        if (nameList)
        {
            LinkedList_printStringList(nameList);
            LinkedList_destroy(nameList);
        }
    }

    /* ===== 3. 获取域目录 (-t) ===== */
    if (getDeviceDirectory)
    {
        /* 获取域中的所有变量名 */
        LinkedList variableList = MmsConnection_getDomainVariableNames(con, &error,
                domainName);

        if (error != MMS_ERROR_NONE)
            returnCode = error;

        if (variableList)
        {
            LinkedList element = LinkedList_getNext(variableList);

            printf("\nMMS domain variables for domain %s\n", domainName);

            while (element != NULL)
            {
                char* name = (char*) element->data;

                printf("  %s\n", name);

                element = LinkedList_getNext(element);
            }

            LinkedList_destroy(variableList);
        }
        else {
            printf("\nFailed to read domain directory (error=%d)\n", error);
        }

        /* 获取域中的所有日志（Journal）名称 */
        variableList = MmsConnection_getDomainJournals(con, &error, domainName);

        if (error != MMS_ERROR_NONE)
            returnCode = error;

        if (variableList)
        {
            LinkedList element = variableList;

            printf("\nMMS journals for domain %s\n", domainName);

            while ((element = LinkedList_getNext(element)) != NULL) {
                char* name = (char*) element->data;

                printf("  %s\n", name);
            }

            LinkedList_destroy(variableList);
        }
        else {
            printf("\nFailed to read domain journals (error=%d)\n", error);
        }
    }

    /* ===== 4. 读取日志 (-j) ===== */
    if (readJournal)
    {
        printf("  read journal %s...\n", journalName);

        /* 解析 domain/journal 格式 */
        char* logDomain = journalName;
        char* logName = strchr(journalName, '/');

        if (logName != NULL)
        {
            logName[0] = 0;      /* 分割字符串：logDomain = 前半部分 */
            logName++;           /* logName = 后半部分（journal名） */

            uint64_t timestamp = Hal_getTimeInMs();

            /* 构造时间范围：从现在往前推 6000 秒（约 1.67 小时） */
            MmsValue* startTime = MmsValue_newBinaryTime(false);
            MmsValue_setBinaryTime(startTime, timestamp - 6000000000);

            MmsValue* endTime = MmsValue_newBinaryTime(false);
            MmsValue_setBinaryTime(endTime, timestamp);

            bool moreFollows;

            /* 按时间范围读取日志条目 */
            LinkedList journalEntries = MmsConnection_readJournalTimeRange(con, &error, logDomain, logName, startTime, endTime,
                    &moreFollows);

            if (error != MMS_ERROR_NONE)
                returnCode = error;

            MmsValue_delete(startTime);
            MmsValue_delete(endTime);

            if (journalEntries != NULL)
            {
                bool readNext;

                /* 循环读取，直到没有更多条目 */
                do
                {
                    readNext = false;

                    /* 获取最后一条日志的 ID 和时间戳，用于分页续读 */
                    LinkedList lastEntry = LinkedList_getLastElement(journalEntries);
                    MmsJournalEntry lastJournalEntry = (MmsJournalEntry) LinkedList_getData(lastEntry);

                    MmsValue* nextEntryId = MmsValue_clone(MmsJournalEntry_getEntryID(lastJournalEntry));
                    MmsValue* nextTimestamp = MmsValue_clone(MmsJournalEntry_getOccurenceTime(lastJournalEntry));

                    /* 打印当前页的日志条目 */
                    printJournalEntries(journalEntries);

                    /* 释放当前页的内存 */
                    LinkedList_destroyDeep(journalEntries, (LinkedListValueDeleteFunction)
                            MmsJournalEntry_destroy);

                    /* 如果还有更多条目，继续读取下一页 */
                    if (moreFollows)
                    {
                        char buf[100];
                        MmsValue_printToBuffer(nextEntryId, buf, 100);

                        printf("READ NEXT AFTER entryID: %s ...\n", buf);

                        journalEntries = MmsConnection_readJournalStartAfter(con, &error, logDomain, logName, nextTimestamp, nextEntryId, &moreFollows);

                        MmsValue_delete(nextEntryId);
                        MmsValue_delete(nextTimestamp);

                        readNext = true;
                    }
                }
                while ((moreFollows == true) || (readNext == true));
            }
        }
        else
            printf("  Invalid log name!\n");
    }

    /* ===== 5. 读取变量值 (-r) ===== */
    if (readVariable)
    {
        if (readWriteHasDomain)
        {
            MmsValue* result;

            /* 根据是否指定了组件名和数组索引，调用不同的读取函数 */
            if (componentName == NULL)
            {
                if (arrayIndex == -1) {
                    /* 读取普通变量 */
                    result = MmsConnection_readVariable(con, &error, domainName, variableName);
                }
                else {
                    /* 读取数组中的单个元素 */
                    result = MmsConnection_readSingleArrayElementWithComponent(con, &error, domainName, variableName, arrayIndex, NULL);
                }
            }
            else {
                if (arrayIndex == -1) {
                    /* 读取结构体变量的某个组件 */
                    result = MmsConnection_readVariableComponent(con, &error, domainName, variableName, componentName);
                }
                else {
                    /* 读取数组中某个元素的结构体组件 */
                    result = MmsConnection_readSingleArrayElementWithComponent(con, &error, domainName, variableName, arrayIndex, componentName);
                }
            }

            if (error != MMS_ERROR_NONE)
            {
                printf("Reading variable failed: (ERROR %i)\n", error);

                returnCode = error;
            }
            else
            {
                printf("Read SUCCESS\n");

                if (result != NULL)
                {
                    char outbuf[1024];

                    MmsValue_printToBuffer(result, outbuf, 1024);

                    printf("%s\n", outbuf);

                    MmsValue_delete(result);
                }
                else
                    printf("result: NULL\n");
            }
        }
        else
        {
            printf("Reading VMD scope variable not yet supported!\n");
        }
    }

    /* ===== 6. 读取变量列表（数据集）值 (-v) ===== */
    if (readVariableList)
    {
        if (readWriteHasDomain)
        {
            MmsValue* variables = MmsConnection_readNamedVariableListValues(con, &error, domainName, variableName, true);

            if (error != MMS_ERROR_NONE)
            {
                printf("Reading variable failed: (ERROR %i)\n", error);

                returnCode = error;
            }
            else
            {
                printf("Read SUCCESS\n");
            }
        }
        else
            printf("Reading VMD scope variable list not yet supported!\n");
    }

    /* ===== 7. 获取变量列表目录（数据集成员列表）(-z) ===== */
    if (readDataSetDirectory)
    {
        if (readWriteHasDomain)
        {
            bool deletable = false;

            LinkedList varListDir = MmsConnection_readNamedVariableListDirectory(con, &error, domainName, variableName, &deletable);

            if (error != MMS_ERROR_NONE)
            {
                printf("Reading variable list directory failed: (ERROR %i)\n", error);

                returnCode = error;
            }
            else
            {
                LinkedList varListElem = LinkedList_getNext(varListDir);

                int listIdx = 0;

                /* 遍历并打印每个数据项的访问规格 */
                while (varListElem)
                {
                    MmsVariableAccessSpecification* varAccessSpec = (MmsVariableAccessSpecification*)LinkedList_getData(varListElem);

                    if (varAccessSpec->arrayIndex)
                        printf("[%i] %s/%s(%i)%s\n", listIdx, varAccessSpec->domainId, varAccessSpec->itemId, varAccessSpec->arrayIndex, varAccessSpec->componentName == NULL ? "" : varAccessSpec->componentName);
                    else
                        printf("[%i] %s/%s\n", listIdx, varAccessSpec->domainId, varAccessSpec->itemId);

                    listIdx++;

                    varListElem = LinkedList_getNext(varListElem);
                }

                printf("Read SUCCESS\n");
            }
        }
        else
            printf("Reading VMD scope variable list not yet supported!\n");
    }

    /* ===== 8. 列出所有文件 (-f) ===== */
    if (showFileList)
    {
        char lastName[300];
        lastName[0] = 0;

        char* continueAfter = NULL;

        /* 循环获取文件列表，支持分页 */
        while (MmsConnection_getFileDirectory(con, &error, "", continueAfter, mmsFileDirectoryHandler, lastName))
        {
            if (error != MMS_ERROR_NONE)
                returnCode = error;

            continueAfter = lastName;
        }
    }

    /* ===== 9. 获取文件属性 (-g) ===== */
    if (getFileAttributes)
    {
        MmsConnection_getFileDirectory(con, &error, filename, NULL, mmsGetFileAttributeHandler, NULL);

        if (error != MMS_ERROR_NONE)
            returnCode = error;
    }

    /* ===== 10. 删除文件 (-x) ===== */
    if (deleteFile)
    {
        MmsConnection_fileDelete(con, &error, filename);

        if (error != MMS_ERROR_NONE)
        {
            printf("Delete file failed: (ERROR %i)\n", error);
            returnCode = error;
        }
        else
        {
            printf("File deleted\n");
        }
    }

/* ===== 清理资源并退出 ===== */
exit:
    free(hostname);
    free(domainName);
    free(variableName);
    free(journalName);
    free(componentName);

    MmsConnection_destroy(con);

    return returnCode;
}
