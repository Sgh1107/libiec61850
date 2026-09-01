/*
 * file-tool.c
 *
 * 这个示例程序演示了 IEC 61850 文件服务的用法
 *
 * - 如何浏览服务器的文件系统
 * - 如何从服务器下载文件
 *
 * 注意：本程序需要配合 server_example3 或 server_example_files 使用
 *
 *
 * 使用示例：
 *   ./file-tool -h 192.168.1.100 dir              # 列出根目录
 *   ./file-tool -h 192.168.1.100 subdir configs   # 列出子目录
 *   ./file-tool -h 192.168.1.100 get fault.bin    # 下载文件
 *   ./file-tool -h 192.168.1.100 set config.cfg   # 上传文件  file-tool.exe -h 192.168.31.57 set .\testicd.icd
 *   ./file-tool -h 192.168.1.100 del old.log      # 删除文件
 */

#include "iec61850_client.h"   /* IEC 61850 客户端 API 头文件 */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <libgen.h>            /* Unix/Linux 下的 basename/dirname 函数 */
#endif

/* ========== Windows 平台兼容性实现 ========== */
#ifdef _WIN32
static char _dirname[1000];    /* 存储目录名的缓冲区 */

/**
 * 获取路径的目录部分（Windows 版本）
 * 例如：输入 "C:/data/file.txt" 返回 "C:/data"
 */
static char*
dirname(char* path)
{
    char* lastSep = NULL;
    int len = strlen(path);
    int i = 0;

    /* 从后向前查找最后一个路径分隔符 */
    while (i < len)
    {
        if (path[i] == '/' || path[i] == ':' || path[i] == '\\')
            lastSep = path + i;
        i++;
    }

    if (lastSep)
    {
        strcpy(_dirname, path);
        _dirname[lastSep - path] = 0;  /* 在分隔符位置截断字符串 */
    }
    else
        strcpy(_dirname, "");           /* 没有分隔符，返回空字符串 */

    return _dirname;
}

static char _basename[1000];   /* 存储文件名的缓冲区 */

/**
 * 获取路径的文件名部分（Windows 版本）
 * 例如：输入 "C:/data/file.txt" 返回 "file.txt"
 */
static char*
basename(char* path)
{
    char* lastSep = NULL;
    int len = strlen(path);
    int i = 0;

    /* 从后向前查找最后一个路径分隔符 */
    while (i < len)
    {
        if (path[i] == '/' || path[i] == ':' || path[i] == '\\')
            lastSep = path + i;
        i++;
    }

    if (lastSep)
        strcpy(_basename, lastSep + 1);  /* 返回分隔符后面的部分 */
    else
        strcpy(_basename, path);          /* 没有分隔符，返回整个路径 */

    return _basename;
}
#endif

/* ========== 全局配置参数 ========== */
static char* hostname = "localhost";   /* 服务器地址，默认本地 */
static int tcpPort = 102;              /* MMS 标准端口号 */
static char* filename = NULL;          /* 操作的目标文件名或目录名 */
static bool singleRequest = false;     /* 是否使用单次请求模式（用于处理分页） */

/**
 * 支持的文件操作类型枚举
 * - None: 无操作（错误状态）
 * - Dir:  列出目录内容
 * - Info: 查看文件信息
 * - Del:  删除文件
 * - Get:  从服务器下载文件
 * - Set:  上传文件到服务器
 */
typedef enum
{
    FileOperationType_None = 0,
    FileOperationType_Dir,
    FileOperationType_Info,
    FileOperationType_Del,
    FileOperationType_Get,
    FileOperationType_Set
} FileOperationType;

static FileOperationType operation = FileOperationType_None;  /* 当前要执行的操作类型 */

/**
 * 文件下载回调函数
 * 当服务器分块发送文件数据时，此函数会被反复调用
 * 
 * @param parameter  用户自定义参数（这里指向本地文件指针）
 * @param buffer     接收到的数据块
 * @param bytesRead  当前数据块的大小（字节数）
 * @return true继续接收，false中止传输
 */
static bool
downloadHandler(void* parameter, uint8_t* buffer, uint32_t bytesRead)
{
    FILE* fp = (FILE*)parameter;  /* 将参数转换为文件指针 */

    printf("received %i bytes\n", bytesRead);

    if (bytesRead > 0)
    {
        /* 将接收到的数据追加写入本地文件 */
        if (fwrite(buffer, bytesRead, 1, fp) != 1)
        {
            printf("Failed to write local file!\n");
            return false;  /* 写入失败，中止传输 */
        }
    }

    return true;  /* 继续接收后续数据 */
}

/**
 * 打印程序使用说明（帮助信息）
 */
static void
printHelp()
{
    printf("file-tool [options] <operation> [<parameters>]\n");
    printf("  Options:\n");
    printf("    -h <hostname/IP>   # 指定服务器地址\n");
    printf("    -p portnumber      # 指定端口号（默认102）\n");
    printf("    -s                 # 单次请求模式（处理分页响应）\n");
    printf("  Operations\n");
    printf("     dir               # 列出根目录\n");
    printf("     subdir <dirname>  # 列出子目录\n");
    printf("     info <filename>   # 显示文件信息\n");
    printf("     del <filename>    # 删除文件\n");
    printf("     get <filename>    # 下载文件\n");
    printf("     set <filename>    # 上传文件\n");
}

/**
 * 解析命令行参数
 * 
 * @param argc  参数个数
 * @param argv  参数数组
 * @return 0成功，非0失败
 */
static int
parseOptions(int argc, char** argv)
{
    int currentArgc = 1;
    int retVal = 0;

    while (currentArgc < argc)
    {
        if (strcmp(argv[currentArgc], "-h") == 0)
        {
            /* 解析服务器主机名/IP地址 */
            hostname = argv[++currentArgc];
        }
        else if (strcmp(argv[currentArgc], "-p") == 0)
        {
            /* 解析端口号 */
            tcpPort = atoi(argv[++currentArgc]);
        }
        else if (strcmp(argv[currentArgc], "-s") == 0)
        {
            /* 启用单次请求模式 */
            singleRequest = true;
        }
        else if (strcmp(argv[currentArgc], "del") == 0)
        {
            /* 删除文件操作 */
            operation = FileOperationType_Del;
            filename = argv[++currentArgc];
        }
        else if (strcmp(argv[currentArgc], "dir") == 0)
        {
            /* 列出根目录操作 */
            operation = FileOperationType_Dir;
        }
        else if (strcmp(argv[currentArgc], "subdir") == 0)
        {
            /* 列出子目录操作，附带目录名参数 */
            operation = FileOperationType_Dir;
            filename = argv[++currentArgc];
        }
        else if (strcmp(argv[currentArgc], "info") == 0)
        {
            /* 查看文件信息操作 */
            operation = FileOperationType_Info;
            filename = argv[++currentArgc];
        }
        else if (strcmp(argv[currentArgc], "get") == 0)
        {
            /* 下载文件操作 */
            operation = FileOperationType_Get;
            filename = argv[++currentArgc];
        }
        else if (strcmp(argv[currentArgc], "set") == 0)
        {
            /* 上传文件操作 */
            operation = FileOperationType_Set;
            filename = argv[++currentArgc];
        }
        else
        {
            printf("Unknown operation!\n");
            return 1;
        }

        currentArgc++;
    }

    return retVal;
}

/**
 * 显示服务器目录内容
 * 
 * @param con 已建立的 IEC 61850 连接对象
 */
void
showDirectory(IedConnection con)
{
    IedClientError error;
    bool moreFollows = false;  /* 标记是否有更多文件（分页用） */

    LinkedList rootDirectory;

    /* 根据是否使用单次请求模式，选择不同的 API 调用方式 */
    if (singleRequest)
        rootDirectory = IedConnection_getFileDirectoryEx(con, &error, filename, NULL, &moreFollows);
    else
        rootDirectory = IedConnection_getFileDirectory(con, &error, filename);

    if (error != IED_ERROR_OK)
    {
        printf("Error retrieving file directory\n");
    }
    else
    {
        /* 遍历目录条目链表 */
        LinkedList directoryEntry = LinkedList_getNext(rootDirectory);

        while (directoryEntry != NULL)
        {
            FileDirectoryEntry entry = (FileDirectoryEntry)directoryEntry->data;

            /* 打印文件名和文件大小 */
            printf("%s %i\n", FileDirectoryEntry_getFileName(entry), 
                             FileDirectoryEntry_getFileSize(entry));

            directoryEntry = LinkedList_getNext(directoryEntry);
        }

        /* 释放链表占用的内存 */
        LinkedList_destroyDeep(rootDirectory, (LinkedListValueDeleteFunction)FileDirectoryEntry_destroy);
    }

    /* 如果有更多文件未返回，提示用户 */
    if (moreFollows)
        printf("\n- MORE FILES AVAILABLE -\n");
}

/**
 * 从服务器下载文件
 * 
 * @param con 已建立的 IEC 61850 连接对象
 */
void
getFile(IedConnection con)
{
    IedClientError error;

    char* bname = strdup(filename);   /* 复制文件名，因为 basename 可能修改原字符串 */
    char* localFilename = basename(bname);  /* 提取文件名部分作为本地文件名 */

    /* 以二进制写入模式打开本地文件 */
    FILE* fp = fopen(localFilename, "wb");

    if (fp != NULL)
    {
        /* 调用 IEC 61850 API 下载文件，使用回调函数处理接收的数据 */
        IedConnection_getFile(con, &error, filename, downloadHandler, (void*)fp);

        if (error != IED_ERROR_OK)
            printf("Failed to get file!\n");

        fclose(fp);  /* 关闭本地文件 */
    }
    else
        printf("Failed to open file %s\n", localFilename);

    free(bname);  /* 释放复制的字符串 */
}

/**
 * 上传文件到服务器
 * 
 * @param con 已建立的 IEC 61850 连接对象
 */
void
setFile(IedConnection con)
{
    IedClientError error;

    /* 复制路径，因为 dirname/basename 可能修改原字符串 */
    char* dirc = strdup(filename);
    char* basec = strdup(filename);

    char* localDirName = dirname(dirc);   /* 获取本地文件的目录部分 */
    char* localFileName = basename(basec); /* 获取本地文件的名称部分 */

    printf("local dir: %s\n", localDirName);
    printf("local file: %s\n", localFileName);

    /*
     * 重要：设置服务器端文件存储基路径时，
     * 需要在末尾添加路径分隔符！
     */
    strcpy(dirc, localDirName);
    strcat(dirc, "/");

    printf("filestore basepath: %s\n", dirc);

    /* 设置服务器端的文件存储基路径 */
    IedConnection_setFilestoreBasepath(con, dirc);

    /* 将本地文件上传到服务器（参数：本地文件名，服务器端文件名） */
    IedConnection_setFile(con, &error, localFileName, localFileName);

    if (error != IED_ERROR_OK)
        printf("Failed to set file! (code=%i)\n", error);

    free(dirc);
    free(basec);
}

/**
 * 删除服务器上的文件
 * 
 * @param con 已建立的 IEC 61850 连接对象
 */
void
deleteFile(IedConnection con)
{
    IedClientError error;

    IedConnection_deleteFile(con, &error, filename);

    if (error != IED_ERROR_OK)
        printf("Failed to delete file! (code=%i)\n", error);
}

/**
 * 程序入口函数
 */
int
main(int argc, char** argv)
{
    /* 参数不足时显示帮助信息 */
    if (argc < 2)
    {
        printHelp();
        return 0;
    }

    /* 解析命令行参数 */
    parseOptions(argc, argv);

    /* 如果未指定任何有效操作，显示帮助信息 */
    if (operation == FileOperationType_None)
    {
        printHelp();
        return 0;
    }

    IedClientError error;

    /* 创建 IEC 61850 客户端连接对象 */
    IedConnection con = IedConnection_create();

    /* 连接到指定的服务器（IP/端口） */
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK)
    {
        /* 根据用户指定的操作类型执行对应的功能 */
        switch (operation)
        {
        case FileOperationType_Dir:
            showDirectory(con);
            break;
        case FileOperationType_Get:
            getFile(con);
            break;
        case FileOperationType_Del:
            deleteFile(con);
            break;
        case FileOperationType_Info:
            /* 注：此功能尚未实现，仅占位 */
            printf("Info operation not implemented yet.\n");
            break;
        case FileOperationType_Set:
            setFile(con);
            break;
        case FileOperationType_None:
            break;
        }

        /* 断开连接 */
        IedConnection_abort(con, &error);
    }
    else
    {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    /* 销毁连接对象，释放资源 */
    IedConnection_destroy(con);
    return 0;
}
