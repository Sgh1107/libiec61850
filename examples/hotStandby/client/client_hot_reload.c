/*
 *  client_hot_reload.c
 *
 *  Companion client for the "hot model reload" server example.
 *
 *  The client demonstrates how a client has to behave when the server
 *  exchanges its complete data model without terminating the process:
 *
 *  - it connects and discovers the server data model dynamically by the
 *    directory services (no compiled-in static model):
 *
 *      IedConnection_getLogicalDeviceList()      -> logical devices
 *      IedConnection_getLogicalDeviceDirectory() -> logical nodes
 *      IedConnection_getLogicalNodeDirectory()   -> RCBs / data sets / DOs
 *
 *  - it subscribes to ALL discovered unbuffered RCBs (falls back to all
 *    discovered buffered RCBs when no URCB exists), enables reporting and
 *    triggers a GI on each to get a full baseline of the data set contents
 *
 *  - when the server performs a hot-swap, the client connection is closed by
 *    the server. The client detects this, tears down the subscription and
 *    reconnects automatically -> then re-discovers the NEW data model
 *    (e.g. new points AnIn3/Ind3 in model_v2.cfg)
 *
 *  - while connected, it polls ConfRev of the subscribed RCB every 5 seconds.
 *    A changed ConfRev indicates that the server configuration/model was
 *    updated (this is the standard IEC 61850 mechanism for clients to detect
 *    configuration changes).
 */

#include "iec61850_client.h"

#include "hal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile sig_atomic_t running = 1;

static void
sigint_handler(int signalId)
{
    running = 0;
}

/* ------------------------------------------------------------------
 * subscription state (one entry per subscribed RCB)
 * ------------------------------------------------------------------ */

#define MAX_SUBSCRIPTIONS 8
#define MAX_RCBS          16

typedef struct sSubscription {
    char rcbRef[130];
    char rptId[130];
    char datSetRef[130];
    bool buffered;
    uint32_t confRev;
    ClientReportControlBlock rcb;
    LinkedList dataSetMembers; /* <char*> object references of the members */
} Subscription;

static Subscription subscriptions[MAX_SUBSCRIPTIONS];
static int numSubscriptions = 0;

static void
Subscription_init(Subscription* sub)
{
    memset(sub, 0, sizeof(Subscription));
}

static void
Subscription_teardown(Subscription* sub, IedConnection con)
{
    if (sub->rcb != NULL) {
        /* do not call this inside of the report callback! */
        IedConnection_uninstallReportHandler(con, sub->rcbRef);
        ClientReportControlBlock_destroy(sub->rcb);
        sub->rcb = NULL;
    }

    if (sub->dataSetMembers != NULL) {
        LinkedList_destroy(sub->dataSetMembers);
        sub->dataSetMembers = NULL;
    }

    sub->confRev = 0;
}

static void
teardownAllSubscriptions(IedConnection con)
{
    int i;

    for (i = 0; i < numSubscriptions; i++)
        Subscription_teardown(&subscriptions[i], con);

    numSubscriptions = 0;
}

/*
 * add an RCB reference to a list of RCB references (no duplicates)
 */
static void
addRcbRefToList(char refList[][130], int* numRefs, const char* rcbRef)
{
    int i;

    for (i = 0; i < *numRefs; i++) {
        if (strcmp(refList[i], rcbRef) == 0)
            return;
    }

    if (*numRefs < MAX_RCBS) {
        strncpy(refList[*numRefs], rcbRef, 129);
        refList[*numRefs][129] = 0;
        (*numRefs)++;
    }
}

static const char*
reasonToStr(ReasonForInclusion reason)
{
    switch (reason) {
    case IEC61850_REASON_DATA_CHANGE:
        return "dchg";
    case IEC61850_REASON_QUALITY_CHANGE:
        return "qchg";
    case IEC61850_REASON_DATA_UPDATE:
        return "dupd";
    case IEC61850_REASON_INTEGRITY:
        return "intg";
    case IEC61850_REASON_GI:
        return "GI";
    case IEC61850_REASON_UNKNOWN:
        return "?";
    default:
        return "-";
    }
}

/*
 * Report callback - prints all included data set members with their reason
 * for inclusion and the value.
 */
static void
reportHandler(void* parameter, ClientReport report)
{
    LinkedList members = (LinkedList)parameter;

    printf("\n>> REPORT: rcb=%s rptId=%s\n",
            ClientReport_getRcbReference(report),
            ClientReport_getRptId(report));

    if (ClientReport_hasTimestamp(report))
        printf("   timestamp: %llu ms\n",
                (unsigned long long)ClientReport_getTimestamp(report));

    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);

    int numValues = (dataSetValues != NULL) ? MmsValue_getArraySize(dataSetValues) : 0;
    int numMembers = LinkedList_size(members);

    int i;

    for (i = 0; i < numMembers; i++) {

        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);

        if (reason == IEC61850_REASON_NOT_INCLUDED)
            continue;

        char valBuffer[100] = { 0 };

        if ((i < numValues) && (dataSetValues != NULL)) {
            MmsValue* value = MmsValue_getElement(dataSetValues, i);

            if (value != NULL)
                MmsValue_printToBuffer(value, valBuffer, sizeof(valBuffer));
        }

        LinkedList member = LinkedList_get(members, i);

        const char* memberName = (member != NULL) ? (const char*)member->data : "<unknown>";

        printf("   %-55s (%s) %s\n", memberName, reasonToStr(reason), valBuffer);
    }
}

/*
 * Subscribe to a single RCB:
 *  - read its parameters (rptId, datSet, ConfRev)
 *  - read the data set members so the report handler can print names
 *  - install the report handler BEFORE enabling the report
 *  - enable reporting (dchg + qchg + dupd + integrity + GI) and trigger a GI
 *  - reserve unbuffered RCBs (buffered RCBs may not support Resv)
 */
static bool
subscribeToRcb(IedConnection con, const char* rcbRef, bool useBuffered)
{
    IedClientError error;

    if (numSubscriptions >= MAX_SUBSCRIPTIONS) {
        printf("WARNING: max number of subscriptions reached - skipping %s\n", rcbRef);
        return false;
    }

    Subscription* sub = &subscriptions[numSubscriptions];

    Subscription_init(sub);

    strncpy(sub->rcbRef, rcbRef, sizeof(sub->rcbRef) - 1);
    sub->buffered = useBuffered;

    sub->rcb = IedConnection_getRCBValues(con, &error, sub->rcbRef, NULL);

    if (sub->rcb == NULL) {
        printf("Failed to read RCB %s (error=%i)\n", sub->rcbRef, error);
        Subscription_teardown(sub, con);
        return false;
    }

    const char* rptId = ClientReportControlBlock_getRptId(sub->rcb);
    const char* datSet = ClientReportControlBlock_getDataSetReference(sub->rcb);

    strncpy(sub->rptId, rptId, sizeof(sub->rptId) - 1);
    strncpy(sub->datSetRef, datSet, sizeof(sub->datSetRef) - 1);

    sub->confRev = ClientReportControlBlock_getConfRev(sub->rcb);

    bool isDeletable = false;

    sub->dataSetMembers = IedConnection_getDataSetDirectory(con, &error, sub->datSetRef, &isDeletable);

    if (error != IED_ERROR_OK) {
        printf("Failed to read data set directory %s (error=%i)\n", sub->datSetRef, error);
        Subscription_teardown(sub, con);
        return false;
    }

    /* install the handler BEFORE enabling the report */
    IedConnection_installReportHandler(con, sub->rcbRef, sub->rptId,
            reportHandler, (void*)sub->dataSetMembers);

    ClientReportControlBlock_setTrgOps(sub->rcb,
            TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_DATA_UPDATE |
            TRG_OPT_INTEGRITY | TRG_OPT_GI);

    ClientReportControlBlock_setRptEna(sub->rcb, true);

    uint32_t elementsToSet = RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_GI;

    if (sub->buffered == false) {
        ClientReportControlBlock_setResv(sub->rcb, true);
        elementsToSet |= RCB_ELEMENT_RESV;
    }

    IedConnection_setRCBValues(con, &error, sub->rcb, elementsToSet, true);

    if (error != IED_ERROR_OK)
        printf("WARNING: setRCBValues failed for %s (error=%i)\n", sub->rcbRef, error);

    printf("\nSubscribed to %s (rptId=%s datSet=%s confRev=%u%s)\n",
            sub->rcbRef, sub->rptId, sub->datSetRef, sub->confRev,
            sub->buffered ? " buffered" : "");

    numSubscriptions++;

    return true;
}

/* ------------------------------------------------------------------
 * dynamic discovery of the server data model + report subscription
 * ------------------------------------------------------------------ */

static bool
discoverAndSubscribe(IedConnection con)
{
    IedClientError error;

    char urcbRefs[MAX_RCBS][130];
    int numUrcbs = 0;
    char brcbRefs[MAX_RCBS][130];
    int numBrcbs = 0;

    memset(urcbRefs, 0, sizeof(urcbRefs));
    memset(brcbRefs, 0, sizeof(brcbRefs));

    /* ---------------------------------------------
     * browse: LDs -> LNs -> RCBs
     * --------------------------------------------- */

    LinkedList deviceList = IedConnection_getLogicalDeviceList(con, &error);

    if (error != IED_ERROR_OK) {
        printf("Failed to get logical device list (error=%i)\n", error);
        return false;
    }

    LinkedList device = LinkedList_getNext(deviceList);

    while (device != NULL) {

        char* ldName = (char*)device->data;

        printf("LD: %s\n", ldName);

        LinkedList logicalNodes = IedConnection_getLogicalDeviceDirectory(con, &error, ldName);

        if (error == IED_ERROR_OK) {

            LinkedList logicalNode = LinkedList_getNext(logicalNodes);

            while (logicalNode != NULL) {

                char* lnName = (char*)logicalNode->data;

                char lnRef[130];
                snprintf(lnRef, sizeof(lnRef), "%s/%s", ldName, lnName);

                /* discover unbuffered RCBs (RP) */
                LinkedList urcbs = IedConnection_getLogicalNodeDirectory(con, &error, lnRef, ACSI_CLASS_URCB);

                if (error == IED_ERROR_OK) {
                    LinkedList element = LinkedList_getNext(urcbs);

                    while (element != NULL) {
                        char* rcbName = (char*)element->data;

                        printf("   %s.RP.%s\n", lnRef, rcbName);

                        char rcbRefBuf[130];
                        snprintf(rcbRefBuf, sizeof(rcbRefBuf), "%s.RP.%s", lnRef, rcbName);
                        addRcbRefToList(urcbRefs, &numUrcbs, rcbRefBuf);

                        element = LinkedList_getNext(element);
                    }

                    LinkedList_destroy(urcbs);
                }

                /* discover buffered RCBs (BR) */
                LinkedList brcbs = IedConnection_getLogicalNodeDirectory(con, &error, lnRef, ACSI_CLASS_BRCB);

                if (error == IED_ERROR_OK) {
                    LinkedList element = LinkedList_getNext(brcbs);

                    while (element != NULL) {
                        char* rcbName = (char*)element->data;

                        printf("   %s.BR.%s\n", lnRef, rcbName);

                        char rcbRefBuf[130];
                        snprintf(rcbRefBuf, sizeof(rcbRefBuf), "%s.BR.%s", lnRef, rcbName);
                        addRcbRefToList(brcbRefs, &numBrcbs, rcbRefBuf);

                        element = LinkedList_getNext(element);
                    }

                    LinkedList_destroy(brcbs);
                }

                logicalNode = LinkedList_getNext(logicalNode);
            }
        }

        LinkedList_destroy(logicalNodes);

        device = LinkedList_getNext(device);
    }

    LinkedList_destroy(deviceList);

    /* ---------------------------------------------
     * subscribe to ALL discovered RCBs
     * (URCBs preferred, fall back to BRCBs when no URCB exists)
     * --------------------------------------------- */

    char (*candidateList)[130];
    int numCandidates;
    bool useBuffered;

    if (numUrcbs > 0) {
        candidateList = urcbRefs;
        numCandidates = numUrcbs;
        useBuffered = false;

        printf("\nSubscribing to %i unbuffered report(s)...\n", numCandidates);
    }
    else if (numBrcbs > 0) {
        candidateList = brcbRefs;
        numCandidates = numBrcbs;
        useBuffered = true;

        printf("\nNo unbuffered reports found - subscribing to %i buffered report(s)...\n", numCandidates);
    }
    else {
        printf("No report control block found on server!\n");
        return false;
    }

    int i;
    int successCount = 0;

    for (i = 0; i < numCandidates; i++) {
        if (subscribeToRcb(con, candidateList[i], useBuffered))
            successCount++;
    }

    if (successCount == 0) {
        printf("Failed to subscribe to any report control block!\n");
        teardownAllSubscriptions(con);
        return false;
    }

    printf("\nActive subscriptions: %i of %i\n", successCount, numCandidates);

    return true;
}


int
main(int argc, char** argv)
{
    char* hostname = "localhost";
    int tcpPort = 8102;

    if (argc > 1)
        hostname = argv[1];

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    signal(SIGINT, sigint_handler);

    printf("libiec61850 hot model-reload example client\n");
    printf("-------------------------------------------\n");
    printf("connect to %s:%i\n\n", hostname, tcpPort);

    int generation = 0;

    while (running) {

        /* ---------------- outer loop: (re-)connection ---------------- */

        IedClientError error;

        IedConnection con = IedConnection_create();

        IedConnection_connect(con, &error, hostname, tcpPort);

        if (error != IED_ERROR_OK) {
            printf("Connection failed (error=%i) - retrying...\n", error);
            IedConnection_destroy(con);
            Thread_sleep(2000);
            continue;
        }

        generation++;

        printf("\n=== CONNECTED (generation #%i) ===\n", generation);

        /* discover the current data model and subscribe to reports */
        bool subscribed = discoverAndSubscribe(con);

        /* ---------------- inner loop: monitor connection ---------------- */

        int tick = 0;

        while (running) {

            Thread_sleep(500);
            tick++;

            IedConnectionState conState = IedConnection_getState(con);

            if (conState != IED_STATE_CONNECTED) {
                printf("\n=== DISCONNECTED (server probably performed a hot-swap) ===\n");
                break;
            }

            /*
             * poll ConfRev of all subscriptions every ~5 seconds.
             * If the server updates its configuration/model, ConfRev changes -
             * even while we stay connected this is the standard indicator.
             */
            if (subscribed && (tick % 10) == 0) {

                int sIdx;

                for (sIdx = 0; sIdx < numSubscriptions; sIdx++) {

                    Subscription* sub = &subscriptions[sIdx];

                    IedClientError pollError;

                    sub->rcb = IedConnection_getRCBValues(con, &pollError, sub->rcbRef, sub->rcb);

                    if (pollError == IED_ERROR_OK) {
                        uint32_t confRev = ClientReportControlBlock_getConfRev(sub->rcb);

                        if (confRev != sub->confRev) {
                            printf("\n*** %s: ConfRev changed: %u -> %u "
                                    "(server configuration/model was updated!) ***\n",
                                    sub->rcbRef, sub->confRev, confRev);

                            sub->confRev = confRev;
                        }
                    }
                }
            }
        }

        teardownAllSubscriptions(con);

        IedConnection_close(con);
        IedConnection_destroy(con);

        /* back to outer loop -> reconnect and re-discover the new model */
    }

    return 0;
}
