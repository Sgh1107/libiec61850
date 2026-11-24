/*
 *  reporting.h
 *
 *  Copyright 2013-2019 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#ifndef REPORTING_H_
#define REPORTING_H_

typedef struct sReportBufferEntry ReportBufferEntry;

// 缓存里的一条报告实体
struct sReportBufferEntry {
    uint8_t entryId[8];     // 8字节条目号BRCB 的核心标识
    uint64_t timeOfEntry;   // 该条目的时间戳（UTC 毫秒）
    int entryLength:30;     // 位域占30位，表示整个条目（含头 + 数据）的字节长度; 30位能存储1GB的单条缓存
    unsigned int flags:2;   /* bit 0 (1 = 完整性报告), bit 1 (1 = isGiReport) */
    ReportBufferEntry* next;// 指向下一条目的指针
};

// 缓存队列容器
typedef struct {
    uint8_t* memoryBlock;   // 一整块连续静态内存的首地址; 所有条目的头和数据都塞进这块内存
    int memoryBlockSize;    // 该内存块总字节数
    int reportsCount;       /* 当前缓存了多少条报告 */
    ReportBufferEntry* oldestReport;
    ReportBufferEntry* lastEnqueuedReport;
    ReportBufferEntry* nextToTransmit;      // 发送游标，指向"下一条该发的报告"。发完只前移它、不删条目这是 BRCB 能补发的根本
    bool isOverflow;        /* 溢出标志 */

    Semaphore lock;         /* protect access to report buffer */
} ReportBuffer;

typedef struct {
    char* name;     // RCB 名字
    MmsDomain* domain;
    LogicalNode* parentLN;
    MmsValue* rcbValues;

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    Semaphore rcbValuesLock;
#endif

    MmsValue* inclusionField;
    DataSet* dataSet;
    bool isDynamicDataSet;
    bool enabled;
    bool reserved;      // 是否被客户端预约
    bool buffered;      /* true if report is a buffered report */

    MmsValue** bufferedDataSetValues;   /* used to buffer values during bufTm time */
    MmsValue** valueReferences;         /* array to store value references for fast access */

    bool gi;            /* flag to indicate that a GI report is triggered */

    uint16_t sqNum;     // 序列号（BRCB 16 位 / URCB 8 位）
    uint32_t intgPd;    /* integrity period in ms 完整性周期 */
    uint32_t bufTm;     // 缓冲时间
    uint64_t nextIntgReportTime; /* 下次完整性报告的绝对时刻 */
    uint64_t reservationTimeout;
    MmsServerConnection clientConnection;   // 绑定到哪个客户端连接

    uint64_t lastEntryId;

    int triggerOps;

    bool hasOwner;      /* indicate that owner field is present 是否支持 Owner 字段（Ed2） */

    ////////////// 分段报告 //////////////
    bool segmented;                 /* indicates that a segmented report is in process */
    int startIndexForNextSegment;   /* start data set index for the next report segment */
    MmsValue* subSeqVal;            /* sub sequence value for segmented reporting */
    uint64_t segmentedReportTimestamp; /* time stamp used for all report segments */

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    Semaphore createNotificationsMutex;  /* { covered by mutex } */
#endif

    // 每个数据集成员一个字节记录它的触发原因（dchg/qchg/dupd），由 ReportControl_valueUpdated() 打点
    uint8_t* inclusionFlags;             /* { covered by mutex } */
    bool triggered;                      /* { covered by mutex } */
    uint64_t reportTime;                 /* { covered by mutex } */

    /*
     * BRCB专属
     */
    bool isBuffering;   /* true if buffered RCB is buffering (datSet is set to a valid value) */
    bool isResync;      /* true if buffered RCB is in resync state */
    int resvTms;        /* -1 for preconfigured client, 0 - not reserved, > 0 reserved by client */
    ReportBuffer* reportBuffer;
    MmsValue* timeOfEntry;

    ReportControlBlock* rcb;
    ReportControlBlock* sibling; /* backup sibling field of original ReportControlBlock */

    IedServer server;
} ReportControl;

LIB61850_INTERNAL ReportControl*
ReportControl_create(bool buffered, LogicalNode* parentLN, int reportBufferSize, IedServer server);

LIB61850_INTERNAL void
ReportControl_destroy(ReportControl* self);

LIB61850_INTERNAL void
ReportControl_valueUpdated(ReportControl* self, int dataSetEntryIndex, int flag, bool modelLocked);

LIB61850_INTERNAL MmsValue*
ReportControl_getRCBValue(ReportControl* rc, const char* elementName);

LIB61850_INTERNAL MmsVariableSpecification*
Reporting_createMmsBufferedRCBs(MmsMapping* self, MmsDomain* domain, LogicalNode* logicalNode, int reportsCount);

LIB61850_INTERNAL MmsVariableSpecification*
Reporting_createMmsUnbufferedRCBs(MmsMapping* self, MmsDomain* domain, LogicalNode* logicalNode, int reportsCount);

LIB61850_INTERNAL MmsDataAccessError
Reporting_RCBWriteAccessHandler(MmsMapping* self, ReportControl* rc, const char* elementName, MmsValue* value, MmsServerConnection connection);

LIB61850_INTERNAL bool
ReportControl_readAccess(ReportControl* rc, MmsMapping* mmsMapping, MmsServerConnection connection, char* elementName);

LIB61850_INTERNAL void
Reporting_activateBufferedReports(MmsMapping* self);

/* periodic check if reports have to be sent */
LIB61850_INTERNAL void
Reporting_processReportEvents(MmsMapping* self, uint64_t currentTimeInMs);

/* check if report have to be sent after data model update */
LIB61850_INTERNAL void
Reporting_processReportEventsAfterUnlock(MmsMapping* self);

/* send reports in report buffer */
LIB61850_INTERNAL void
Reporting_sendReports(MmsMapping* self, MmsServerConnection connection);

LIB61850_INTERNAL void
Reporting_deactivateAllReports(MmsMapping* self);

LIB61850_INTERNAL void
Reporting_deactivateReportsForConnection(MmsMapping* self, MmsServerConnection connection);

#endif /* REPORTING_H_ */
