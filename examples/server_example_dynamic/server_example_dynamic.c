/*
 *  server_example_dynamic.c
 *
 *  This example shows how to build a data model at runtime by API calls.
 *
 *  ------------------------------------------------------------------
 *  在此基础上增加了"遥调（模拟量设定值调节）"测试能力：
 *
 *    GGIO1.AnOut1    : APC（可控模拟设定值，direct-with-enhanced-security）
 *                      客户端直接 operate 下发模拟量设定值，超量程拒绝
 *    GGIO1.AnOut1Fb  : MV（模拟的实际输出值）
 *                      服务端按一阶跟随逼向设定值，用来观察
 *                      "遥调下发 -> 输出跟随变化"的完整链路
 *
 *  两个点都加入数据集 events，客户端订阅报告即可看到变化；
 *  enhanced 安全模型还会带回 CommandTermination 回执（成/败都有）。
 *  ------------------------------------------------------------------
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* 遥调设定值量程，超出则拒绝 */
#define ANALOG_SETPOINT_MIN 0.f
#define ANALOG_SETPOINT_MAX 1000.f

/* 实际输出跟随速率：每个控制周期逼近剩余偏差的比例 */
#define OUTPUT_FOLLOW_RATE 0.2f
#define OUTPUT_FOLLOW_EPS  0.01f

static int running = 0;
static IedServer iedServer = NULL;
static uint64_t startTime = 0;

/* APC: GGIO1.AnOut1 */
static DataAttribute* anOut1Setpoint = NULL;    /* mxVal.f (MX) */
static DataAttribute* anOut1Timestamp = NULL;   /* t       (MX) */

/* MV: GGIO1.AnOut1Fb */
static DataAttribute* anOut1FbValue = NULL;     /* mag.f (MX) */
static DataAttribute* anOut1FbTimestamp = NULL; /* t     (MX) */

static float targetSetpoint = 0.f;   /* 最近一次遥调下发的目标值 */
static float outputValue = 0.f;      /* 模拟的实际输出值 */
static bool outputFollowed = true;   /* 输出是否已跟到目标 */
static bool followPrinted = true;    /* 本次跟随是否已打印 */

void sigint_handler(int signalId)
{
	running = 0;
}

static double
elapsedSeconds(void)
{
    if (startTime == 0)
        return 0.0;

    return (Hal_getTimeInMs() - startTime) / 1000.0;
}

/*
 * APC 的控制处理函数：客户端 select + operate 后由服务器调用。
 * 这里把 ctlVal 写进 mxVal.f 并盖时间戳，同时更新跟随目标。
 */
static ControlHandlerResult
controlHandlerForAnalogOutput(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    uint64_t timestamp = Hal_getTimeInMs();
    ClientConnection clientCon = ControlAction_getClientConnection(action);
    const char* peer = (clientCon != NULL) ? ClientConnection_getPeerAddress(clientCon) : "(local)";

    (void) parameter;

    /*
     * 注意：APC 的 ctlVal 是 AnalogueValue 结构体（含 f / i），
     * 服务器传给 handler 的是整个结构，而不是裸 FLOAT32。
     * 所以这里先取第 0 个元素（f）。
     */
    MmsValue* ctlValFloat = value;

    if (MmsValue_getType(value) == MMS_STRUCTURE)
        ctlValFloat = MmsValue_getElement(value, 0);

    if ((ctlValFloat == NULL) || (MmsValue_getType(ctlValFloat) != MMS_FLOAT)) {
        printf("[%7.2fs] [CTRL] ctlVal 类型不是 FLOAT32，拒绝\n", elapsedSeconds());
        return CONTROL_RESULT_FAILED;
    }

    float newValue = MmsValue_toFloat(ctlValFloat);
    float oldValue = MmsValue_toFloat(anOut1Setpoint->mmsValue);

    printf("[%7.2fs] [CTRL] 来自 %s 遥调 GGIO1.AnOut1: %.2f -> %.2f  (ctlNum=%i%s)\n",
            elapsedSeconds(), peer, oldValue, newValue,
            ControlAction_getCtlNum(action),
            test ? ", Test" : "");

    if ((newValue < ANALOG_SETPOINT_MIN) || (newValue > ANALOG_SETPOINT_MAX)) {
        printf("           ↳ 超出量程 [%.0f, %.0f]，拒绝\n",
                ANALOG_SETPOINT_MIN, ANALOG_SETPOINT_MAX);

        /* 把拒绝原因带给客户端（随 CommandTermination- / LastApplError 一起返回） */
        ControlAction_setAddCause(action, ADD_CAUSE_INVALID_POSITION);

        fflush(stdout);
        return CONTROL_RESULT_FAILED;
    }

    IedServer_updateFloatAttributeValue(iedServer, anOut1Setpoint, newValue);
    IedServer_updateUTCTimeAttributeValue(iedServer, anOut1Timestamp, timestamp);

    targetSetpoint = newValue;
    outputFollowed = false;
    followPrinted = false;

    fflush(stdout);

    return CONTROL_RESULT_OK;
}

int main(int argc, char** argv) {

    int tcpPort = 102;

    if (argc > 1) {
        tcpPort = atoi(argv[1]);
    }

    /*********************
     * Setup data model
     ********************/

    IedModel* model = IedModel_create("testmodel");

    LogicalDevice* lDevice1 = LogicalDevice_create("SENSORS", model);

    LogicalNode* lln0 = LogicalNode_create("LLN0", lDevice1);

    DataObject* lln0_mod = CDC_ENS_create("Mod", (ModelNode*) lln0, 0);
    DataObject* lln0_health = CDC_ENS_create("Health", (ModelNode*) lln0, 0);

    SettingGroupControlBlock_create(lln0, 1, 1);

    /* Add a temperature sensor LN */
    LogicalNode* ttmp1 = LogicalNode_create("TTMP1", lDevice1);
    DataObject* ttmp1_tmpsv = CDC_SAV_create("TmpSv", (ModelNode*) ttmp1, 0, false);

    DataAttribute* temperatureValue = (DataAttribute*) ModelNode_getChild((ModelNode*) ttmp1_tmpsv, "instMag.f");
    DataAttribute* temperatureTimestamp = (DataAttribute*) ModelNode_getChild((ModelNode*) ttmp1_tmpsv, "t");

    LogicalNode* ggio1 = LogicalNode_create("GGIO1", lDevice1);

    /*
     * --- 遥调目标：APC，direct-with-enhanced-security ---
     * 选 direct 而非 SBO：dynamic 模型生成的 SBOw 结构体（ctlVal 为 AnalogueValue）
     * 与客户端库默认构造的 select 参数不匹配，select 会失败；
     * direct 模型无需 select，operate 直接进 handler，且增强型会回 CommandTermination。
     */
    DataObject* ggio1_anOut1 = CDC_APC_create("AnOut1", (ModelNode*)ggio1, 0,
            CDC_CTL_MODEL_DIRECT_ENHANCED, false);

    anOut1Setpoint = (DataAttribute*) ModelNode_getChild((ModelNode*) ggio1_anOut1, "mxVal.f");
    anOut1Timestamp = (DataAttribute*) ModelNode_getChild((ModelNode*) ggio1_anOut1, "t");

    /* --- 模拟的实际输出：MV，跟随设定值 --- */
    DataObject* ggio1_anOut1Fb = CDC_MV_create("AnOut1Fb", (ModelNode*) ggio1, 0, false);

    anOut1FbValue = (DataAttribute*) ModelNode_getChild((ModelNode*) ggio1_anOut1Fb, "mag.f");
    anOut1FbTimestamp = (DataAttribute*) ModelNode_getChild((ModelNode*) ggio1_anOut1Fb, "t");

    DataSet* dataSet = DataSet_create("events", lln0);
    DataSetEntry_create(dataSet, "TTMP1$MX$TmpSv$instMag$f", -1, NULL);
    DataSetEntry_create(dataSet, "GGIO1$MX$AnOut1$mxVal$f", -1, NULL);
    DataSetEntry_create(dataSet, "GGIO1$MX$AnOut1Fb$mag$f", -1, NULL);

    uint8_t rptOptions = RPT_OPT_SEQ_NUM | RPT_OPT_TIME_STAMP | RPT_OPT_REASON_FOR_INCLUSION;

    ReportControlBlock_create("events01", lln0, "events01", false, NULL, 1, TRG_OPT_DATA_CHANGED, rptOptions, 50, 0);
    ReportControlBlock_create("events02", lln0, "events02", false, NULL, 1, TRG_OPT_DATA_CHANGED, rptOptions, 50, 0);

    GSEControlBlock_create("gse01", lln0, "events01", "events", 1, false, 200, 3000);

    /*********************
     * run server
     ********************/

	iedServer = IedServer_create(model);

	/* 安装 APC 的控制处理函数（select / operate 最终都落到这里） */
	IedServer_setControlHandler(iedServer, ggio1_anOut1, controlHandlerForAnalogOutput, NULL);

	/* MMS server will be instructed to start listening to client connections. */
	IedServer_start(iedServer, tcpPort);

	if (!IedServer_isRunning(iedServer)) {
		printf("Starting server failed! Exit.\n");
		IedServer_destroy(iedServer);
		exit(-1);
	}

	running = 1;

	startTime = Hal_getTimeInMs();

	/* 关闭 stdout 缓冲，保证日志重定向到文件时也能实时看到 */
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("=========================================================\n");
	printf(" 遥调测试服务端已启动，端口 %d\n", tcpPort);
	printf("   SENSORS/GGIO1.AnOut1     APC 设定值（select + operate 下发）\n");
	printf("   SENSORS/GGIO1.AnOut1Fb   MV  模拟实际输出（自动跟随设定值）\n");
	printf("   量程 %.0f ~ %.0f，超限将被拒绝\n", ANALOG_SETPOINT_MIN, ANALOG_SETPOINT_MAX);
	printf("   Ctrl+C 退出\n");
	printf("=========================================================\n");
	fflush(stdout);

	signal(SIGINT, sigint_handler);

	float val = 0.f;

	while (running) {
	    IedServer_lockDataModel(iedServer);

	    IedServer_updateUTCTimeAttributeValue(iedServer, temperatureTimestamp, Hal_getTimeInMs());
	    IedServer_updateFloatAttributeValue(iedServer, temperatureValue, val);

	    /* 实际输出向遥调目标一阶跟随，模拟 AO 通道的响应过程 */
	    float delta = targetSetpoint - outputValue;

	    if (fabsf(delta) > OUTPUT_FOLLOW_EPS) {
	        outputValue += delta * OUTPUT_FOLLOW_RATE;

	        if (fabsf(targetSetpoint - outputValue) <= OUTPUT_FOLLOW_EPS) {
	            outputValue = targetSetpoint;
	            outputFollowed = true;
	        }

	        IedServer_updateFloatAttributeValue(iedServer, anOut1FbValue, outputValue);
	        IedServer_updateUTCTimeAttributeValue(iedServer, anOut1FbTimestamp, Hal_getTimeInMs());
	    }
	    else if (!outputFollowed) {
	        outputValue = targetSetpoint;
	        outputFollowed = true;

	        IedServer_updateFloatAttributeValue(iedServer, anOut1FbValue, outputValue);
	        IedServer_updateUTCTimeAttributeValue(iedServer, anOut1FbTimestamp, Hal_getTimeInMs());
	    }

	    IedServer_unlockDataModel(iedServer);

	    /* 每次遥调跟到位后打印一次，便于在终端观察闭环结果 */
	    if (outputFollowed && !followPrinted) {
	        printf("[%7.2fs] [FOLLOW] 实际输出已跟随到 %.2f\n", elapsedSeconds(), outputValue);
	        fflush(stdout);
	        followPrinted = true;
	    }

	    val += 0.1f;

		Thread_sleep(100);
	}

	/* stop MMS server - close TCP server socket and all client sockets */
	IedServer_stop(iedServer);

	/* Cleanup - free all resources */
	IedServer_destroy(iedServer);

	/* destroy dynamic data model */
	IedModel_destroy(model);
  return 0;
} /* main() */
