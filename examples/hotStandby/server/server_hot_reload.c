/*
 *  server_hot_reload.c
 *
 *  Demonstrates how to replace the complete data model of a running libiec61850
 *  server WITHOUT terminating the host process.
 *
 *  How it works
 *  ------------
 *  - The data model is NOT compiled in (no static_model.c). It is loaded at
 *    runtime from a .cfg file (as produced by the model generator from an
 *    ICD/CID/SCL file) using ConfigFileParser_createModelFromConfigFileEx().
 *
 *  - A "hot-swap" is triggered whenever the config file changes on disk
 *    (detected via FileSystem_getFileInfo()).
 *
 *  - Since IedServer maps the data model to MMS objects only once (at
 *    IedServer_create), the model cannot be modified while the server runs.
 *    Instead we perform a controlled instance swap inside the process:
 *
 *        1. parse and validate the NEW model first (old server keeps running
 *           if parsing fails)
 *        2. snapshot live process values into a reference based cache
 *        3. stop + destroy the old IedServer/IedModel
 *           (this closes all client connections!)
 *        4. create a new IedServer with the new model, restore the cached
 *           values by object reference (type checked) and start it again
 *
 *  - Clients are disconnected during step 3 and have to reconnect. They can
 *    detect the new configuration by re-reading the directory services and
 *    comparing ConfRev of RCBs / data sets (see client_hot_reload.c).
 *
 *  NOTE: Never call dynamic model creation/deletion functions while an
 *  IedServer is running! The MMS mapping holds direct pointers into the
 *  data model tree.
 */

#include "iec61850_server.h"
#include "iec61850_dynamic_model.h"
#include "iec61850_config_file_parser.h"

#include "hal_thread.h"
#include "hal_filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define CFG_PATH_MAX 256
#define REF_MAX      130

static char cfgPath[CFG_PATH_MAX] = "model.cfg";
static int tcpPort = 8102;

static volatile sig_atomic_t running = 1;

static IedModel* g_model = NULL;
static IedServer g_server = NULL;

/* ------------------------------------------------------------------
 * Reference based value cache
 *
 * Simulated process values are always written to this cache first and
 * then forwarded to the currently active server instance. When the data
 * model is exchanged, the cached values are re-applied to the new model
 * by object reference (only values with matching type are restored).
 * ------------------------------------------------------------------ */

typedef struct sCachedValue {
    char objectRef[REF_MAX];
    MmsValue* value;
    struct sCachedValue* next;
} CachedValue;

static CachedValue* valueCache = NULL;

static void
ValueCache_clear(void)
{
    CachedValue* cv = valueCache;

    while (cv) {
        CachedValue* next = cv->next;
        MmsValue_delete(cv->value);
        free(cv);
        cv = next;
    }

    valueCache = NULL;
}

static void
ValueCache_set(const char* objectRef, MmsValue* newValue)
{
    CachedValue* cv = valueCache;

    while (cv) {
        if (strcmp(cv->objectRef, objectRef) == 0)
            break;

        cv = cv->next;
    }

    if (cv) {
        MmsValue_delete(cv->value);
        cv->value = MmsValue_clone(newValue);
        return;
    }

    cv = (CachedValue*)malloc(sizeof(CachedValue));

    if (cv == NULL)
        return;

    strncpy(cv->objectRef, objectRef, REF_MAX - 1);
    cv->objectRef[REF_MAX - 1] = 0;
    cv->value = MmsValue_clone(newValue);
    cv->next = valueCache;
    valueCache = cv;
}

/*
 * Check that the found node is really a DataAttribute and that its static
 * type is compatible with the cached MmsValue. This protects against
 * references that disappeared or changed their type in the new model.
 */
static bool
dataAttributeMatchesCachedValue(DataAttribute* da, MmsValue* cached)
{
    if ((da == NULL) || (cached == NULL))
        return false;

    if (da->modelType != DataAttributeModelType)
        return false;

    if (da->mmsValue == NULL)
        return false;

    switch (da->type) {
    case IEC61850_BOOLEAN:
        return MmsValue_getType(cached) == MMS_BOOLEAN;
    case IEC61850_FLOAT32:
        return MmsValue_getType(cached) == MMS_FLOAT;
    case IEC61850_FLOAT64:
        return MmsValue_getType(cached) == MMS_FLOAT; /* MMS_FLOAT covers 32 and 64 bit */
    case IEC61850_INT32:
    case IEC61850_INT64:
    case IEC61850_ENUMERATED:
        return MmsValue_getType(cached) == MMS_INTEGER;
    case IEC61850_INT32U:
        return MmsValue_getType(cached) == MMS_UNSIGNED;
    default:
        return false;
    }
}

static void
applyCachedValues(void)
{
    int restored = 0;
    int skipped = 0;

    CachedValue* cv = valueCache;

    while (cv) {
        DataAttribute* da = (DataAttribute*)
                IedModel_getModelNodeByShortObjectReference(g_model, cv->objectRef);

        if (dataAttributeMatchesCachedValue(da, cv->value)) {
            /* NOTE: updateAttributeValue copies the value (MmsValue_update),
             * ownership of the cached value stays here */
            IedServer_updateAttributeValue(g_server, da, cv->value);
            restored++;
        }
        else {
            skipped++;
        }

        cv = cv->next;
    }

    printf("HOT-SWAP: value cache applied -> %i restored, %i skipped\n", restored, skipped);
}

/*
 * Write a process value: store it in the cache and forward it to the
 * active server instance (if the reference exists in the current model).
 */
static void
updatePoint(const char* shortObjectRef, MmsValue* newValue /* ownership taken */)
{
    ValueCache_set(shortObjectRef, newValue);

    if ((g_server != NULL) && (g_model != NULL)) {
        DataAttribute* da = (DataAttribute*)
                IedModel_getModelNodeByShortObjectReference(g_model, shortObjectRef);

        if (dataAttributeMatchesCachedValue(da, newValue))
            IedServer_updateAttributeValue(g_server, da, newValue);
    }

    MmsValue_delete(newValue);
}

static void
printModelSummary(IedModel* model)
{
    LogicalDevice* ld = model->firstChild;
    int ldCount = 0;

    printf("HOT-SWAP: new model summary:\n");

    while (ld) {
        ldCount++;

        printf("  LD(%i): %s\n", ldCount, ld->name);

        LogicalNode* ln = (LogicalNode*)ld->firstChild;

        while (ln) {
            int doCount = 0;

            ModelNode* dn = ln->firstChild;

            while (dn) {
                doCount++;
                dn = dn->sibling;
            }

            printf("    LN: %s (%i DOs)\n", ln->name, doCount);

            ln = (LogicalNode*)ln->sibling;
        }

        ld = (LogicalDevice*)ld->sibling;
    }
}

/* ------------------------------------------------------------------
 * Hot swap of the data model
 * ------------------------------------------------------------------ */

static void
hotSwapDataModel(void)
{
    printf("\n=== HOT-SWAP: loading new data model from \"%s\" ===\n", cfgPath);

    /* 1. try to build the new model - the old server keeps running on failure */
    IedModel* newModel = ConfigFileParser_createModelFromConfigFileEx(cfgPath);

    if (newModel == NULL) {
        printf("HOT-SWAP ABORTED: failed to parse \"%s\" -> keeping old model\n", cfgPath);
        return;
    }

    /* 2. simple sanity check of the new model */
    if (IedModel_getDeviceByInst(newModel, "GenericIO") == NULL) {
        printf("HOT-SWAP ABORTED: new model has no LD \"GenericIO\" -> keeping old model\n");
        IedModel_destroy(newModel);
        return;
    }

    /* 3. exchange the server instance
     *
     * IedServer_stop closes the listening socket AND all client connections!
     * There is no way around this - the MMS mapping is bound to the model
     * instance. Clients have to reconnect afterwards.
     */
    IedServer_stop(g_server);
    IedServer_destroy(g_server);
    IedModel_destroy(g_model);

    g_model = newModel;
    g_server = IedServer_create(g_model);

    /* 4. restore process values by object reference */
    applyCachedValues();

    IedServer_start(g_server, tcpPort);

    if (IedServer_isRunning(g_server) == false) {
        printf("FATAL: failed to restart server on port %i\n", tcpPort);
        running = 0;
        return;
    }

    printf("=== HOT-SWAP complete: same process, new data model (clients may reconnect) ===\n");

    printModelSummary(g_model);
}

static void
sigint_handler(int signalId)
{
    running = 0;
}

int
main(int argc, char** argv)
{
    if (argc > 1) {
        strncpy(cfgPath, argv[1], CFG_PATH_MAX - 1);
        cfgPath[CFG_PATH_MAX - 1] = 0;
    }

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    printf("libiec61850 hot model-reload example server\n");
    printf("-------------------------------------------\n");
    printf("config file : %s\n", cfgPath);
    printf("TCP port    : %i\n", tcpPort);
    printf("\nWhile this server is running, replace the config file content\n");
    printf("(e.g. copy model_v2.cfg over model.cfg) to trigger a hot-swap.\n\n");

    signal(SIGINT, sigint_handler);

    /* initial load of the data model */
    g_model = ConfigFileParser_createModelFromConfigFileEx(cfgPath);

    if (g_model == NULL) {
        printf("ERROR: failed to load data model from \"%s\"\n", cfgPath);
        return 1;
    }

    g_server = IedServer_create(g_model);

    IedServer_start(g_server, tcpPort);

    if (IedServer_isRunning(g_server) == false) {
        printf("ERROR: failed to start server on port %i\n", tcpPort);
        IedServer_destroy(g_server);
        IedModel_destroy(g_model);
        return 1;
    }

    printModelSummary(g_model);

    /* remember initial state of the config file for change detection */
    uint64_t lastModificationTime = 0;
    uint32_t lastFileSize = 0;
    bool fileInfoValid = false;

    if (FileSystem_getFileInfo(cfgPath, &lastFileSize, &lastModificationTime))
        fileInfoValid = true;

    int iteration = 0;

    while (running) {

        iteration++;

        /*
         * simulate changing process values.
         *
         * Note: points AnIn3/Ind3 only exist in model_v2.cfg. They are written
         * to the cache anyway, so they automatically appear after a hot-swap
         * to v2 without any code change ("write-through" cache concept).
         */

        float anIn1 = (float)(iteration % 200) / 2.0f;   /* sawtooth 0..100 */
        float anIn2 = 100.f - anIn1;
        float anIn3 = (float)(iteration % 300) / 3.0f;   /* sawtooth 0..100 */

        updatePoint("GenericIO/GGIO1.AnIn1.mag.f", MmsValue_newFloat(anIn1));
        updatePoint("GenericIO/GGIO1.AnIn2.mag.f", MmsValue_newFloat(anIn2));
        updatePoint("GenericIO/GGIO1.AnIn3.mag.f", MmsValue_newFloat(anIn3));

        updatePoint("GenericIO/GGIO1.Ind1.stVal", MmsValue_newBoolean(((iteration / 5) % 2) == 0));
        updatePoint("GenericIO/GGIO1.Ind2.stVal", MmsValue_newBoolean(((iteration / 3) % 2) == 0));
        updatePoint("GenericIO/GGIO1.Ind3.stVal", MmsValue_newBoolean(((iteration / 7) % 2) == 0));

        /* check for config file change -> trigger hot-swap */
        uint32_t fileSize = 0;
        uint64_t modificationTime = 0;

        if (FileSystem_getFileInfo(cfgPath, &fileSize, &modificationTime)) {
            if (fileInfoValid &&
                    ((fileSize != lastFileSize) || (modificationTime != lastModificationTime)))
            {
                hotSwapDataModel();
            }

            lastFileSize = fileSize;
            lastModificationTime = modificationTime;
            fileInfoValid = true;
        }

        Thread_sleep(1000);
    }

    printf("\nShutting down...\n");

    IedServer_stop(g_server);
    IedServer_destroy(g_server);
    IedModel_destroy(g_model);

    ValueCache_clear();

    return 0;
}
