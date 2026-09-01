English | [中文](README_zh.md)

# hotStandby - Hot data model reload example (server + client)

This example demonstrates how to exchange the **complete IEC 61850 data model of a
running server without terminating the host process**, and how a client detects
and adapts to the new model.

It is based on the runtime model loading capabilities of libiec61850
(`ConfigFileParser_createModelFromConfigFileEx()`, dynamic model API) and uses no
compiled-in static model.

## Layout

```
hotStandby/
├── README.md                  this file
├── server/
│   ├── server_hot_reload.c    the server (loads model from .cfg, watches file, hot-swaps)
│   ├── model_v1.cfg           initial data model  (2 analogs, 2 indicators)
│   └── model_v2.cfg           updated data model  (+AnIn3, +Ind3, ConfRev=2)
└── client/
    └── client_hot_reload.c    client with dynamic discovery + auto reconnect
```

## How it works

### Server

1. Loads `model.cfg` at startup via `ConfigFileParser_createModelFromConfigFileEx()`
   (the same `.cfg` format that is generated from an ICD/CID/SCL file by the model
   generator tool) and starts a standard `IedServer`.
2. All simulated process values are written through a **reference based value cache**
   (`"GenericIO/GGIO1.AnIn1.mag.f"` -> MmsValue). The cache is the primary store;
   values are forwarded to the active server instance by object reference.
3. The config file is polled once per second (`FileSystem_getFileInfo()`).
   On change:
   - parse + validate the NEW model first (on failure the old server keeps running)
   - stop/destroy the old `IedServer`/`IedModel`
     (**this closes all client connections** - there is no way around this,
     the MMS mapping is bound to the model instance)
   - create the new server instance, re-apply all cached values that still exist
     in the new model (type checked), start it again

### Client

- discovers LDs / LNs / RCBs dynamically using the directory services
  (`getLogicalDeviceList`, `getLogicalDeviceDirectory`, `getLogicalNodeDirectory`)
- subscribes to ALL discovered unbuffered RCBs (falls back to all buffered
  RCBs when no URCB exists), enables reporting (dchg/qchg/dupd/integrity/GI)
  and triggers a GI baseline report on each
- prints every received report with member references, reason codes and values
- detects the disconnect caused by a server hot-swap, tears down the
  subscription and reconnects automatically -> then discovers the NEW model
- polls `ConfRev` of all subscriptions every 5 s while connected: a changed
  `ConfRev` is the standardized indicator for a changed server configuration

## Implementation notes

### IEC 61850 mechanisms / services used

| Mechanism | Used? | Role in this example |
|---|---|---|
| MMS reporting - unbuffered (URCB) | yes | main event transport; `urcbEvents01` (Indicators), `urcbAnalog01` (Measurements) |
| MMS reporting - buffered (BRCB) | yes | `brcbEvents01` is discovered and would be used when no URCB exists |
| Report trigger conditions (TrgOps) | yes | dchg / qchg / dupd / integrity (IntgPd = 1 s) / GI written by the client |
| Report option fields (OptFlds) | yes | configured in the model (`options=175`: seqNum, timestamp, reason, dataSet, bufferOverflow, confRev) |
| Directory services (GetNameList based) | yes | dynamic discovery of LDs -> LNs -> RCBs / data sets, no compiled-in model at the client |
| ConfRev change detection | yes | standardized indicator for "server configuration changed" |
| Data model services (GetDataValues) | implicit | data set values are transported inside reports only |
| GOOSE | **no** | not used (would need GSE control blocks + Ethernet layer) |
| Sampled Values (SV) | **no** | not used (would need SVCBs + dedicated publishing) |
| Controls (direct/SBO Oper) | **no** | models contain no controllable objects |
| Logs / Journals (LCB) | **no** | not used |

### Server implementation

1. **Runtime model loading** - `ConfigFileParser_createModelFromConfigFileEx(path)`
   builds an entire `IedModel` at runtime using the dynamic model API
   (`LogicalDevice_create`, `LogicalNode_create`, `DataObject_create`,
   `DataAttribute_create`, `DataSet_create`, `ReportControlBlock_create`...).
   The `.cfg` file format is exactly what the model generator tool
   (`genmodel.jar`) emits from an SCL/ICD/CID file - so this example works with
   any real device description without code changes.
2. **Value handling and report triggering** - all updates go through
   `IedServer_updateAttributeValue(server, da, value)`. The library copies the
   value into the model's internal MmsValue cache (`MmsValue_update`) and
   internally evaluates the trigger conditions of all RCBs observing that
   point (`checkForChangedTriggers`). The application never touches reports
   directly - it only pushes values. (`.q` quality and `.t` timestamp
   attributes are left untouched by this demo.)
3. **Point lookup** -
   `IedModel_getModelNodeByShortObjectReference(model, "GenericIO/GGIO1.AnIn1.mag.f")`.
   "Short" reference means the LD part is the plain LD instance name
   (without the configurable IED name prefix).
4. **Why instance swap instead of live model editing** - inside
   `IedServer_create()` the MMS mapping (`MmsMapping`) builds the complete MMS
   object tree (domains, named variables, named variable lists for RCBs/data
   sets) holding *raw pointers* into the data model tree. Modifying or freeing
   model nodes afterwards leaves dangling pointers there => undefined
   behavior/crash. Therefore the only safe way to exchange the model is
   stop -> destroy -> create -> start. `IedServer_stop()` closes the listening
   socket AND all client associations.
5. **Value migration across swaps** - a write-through cache
   (`CachedValue`, keyed by short object reference, stores `MmsValue_clone`s)
   sits between the simulation and the server:

   ```
   updatePoint(ref, val)
        |-- ValueCache_set(ref, val)          <- primary store (always)
        '-- if node exists in active model:   <- type checked
              IedServer_updateAttributeValue(...)
   ```

   After creating the new server instance, `applyCachedValues()` iterates the
   cache, resolves each reference in the NEW model, checks compatibility
   (`dataAttributeMatchesCachedValue()`: node must be a DataAttribute,
   `da->type` must map to the cached `MmsType`) and applies matching values.
   This is why AnIn3 keeps its sawtooth phase after a v1 -> v2 swap instead
   of starting at 0, and points removed from the model degrade gracefully.
6. **Change detection with two-phase swap** - the config file is polled once
   per second via `FileSystem_getFileInfo()` (HAL API - works on Linux and
   Windows) comparing size + mtime. The new model is parsed and validated
   BEFORE the old server is stopped, so a broken config file can never take
   down the running server ("HOT-SWAP ABORTED ... keeping old model").

### Client implementation

1. **Connection handling** - `IedConnection_connect()` establishes the MMS
   association (ISO layers handled by libiec61850). The connection state is
   polled every 500 ms with `IedConnection_getState()`.
2. **Dynamic discovery chain** (no static model compiled in):

   ```
   IedConnection_getLogicalDeviceList(con)                 -> LD names
   IedConnection_getLogicalDeviceDirectory(con, ld)        -> LN names
   IedConnection_getLogicalNodeDirectory(con, lnRef, ACSI_CLASS_URCB/BRCB)
                                                           -> RCB names
   full RCB reference: "<LD>/<LN>.RP.<name>"  or ".BR.<name>" for buffered
   ```

   All discovered URCBs are subscribed (fallback: all BRCBs when no URCB
   exists). Up to `MAX_SUBSCRIPTIONS` parallel subscriptions are maintained.
3. **Subscription sequence per RCB** (order matters):

   ```
   rcb = IedConnection_getRCBValues(con, &err, rcbRef, NULL)   read parameters
         -> rptId, datSetRef, confRev
   members = IedConnection_getDataSetDirectory(con, &err, datSetRef)
                                                               member names
   IedConnection_installReportHandler(con, rcbRef, rptId, handler, members)
                                                               BEFORE enabling!
   ClientReportControlBlock_setTrgOps(rcb, dchg|qchg|dupd|integrity|GI)
   ClientReportControlBlock_setRptEna(rcb, true)
   IedConnection_setRCBValues(con, &err, rcb,
           TRG_OPS|RPT_ENA|GI (+RESV for URCB), true)          atomic enable+GI
   ```

4. **Report decoding** - the handler receives a `ClientReport`. The values
   array is aligned with the data set entries; entries not included in the
   report are filtered via
   `ClientReport_getReasonForInclusion(report, i)` which yields one of
   `dchg / qchg / dupd / intg(egrity) / GI`. Member names come from the data
   set directory captured at subscription time.
5. **Resilience loop** - outer loop reconnects with retry; inner loop monitors
   connection state and polls ConfRev of every subscription every ~5 s. On
   disconnect all handlers are uninstalled, RCB objects and member lists
   destroyed (`teardownAllSubscriptions()`), and the next generation performs
   a full re-discovery - which is how the client learns about added/removed
   points after a server hot-swap.

## Building

Build as part of the library build system:

```sh
mkdir build && cd build
cmake ..
cmake --build . --target server_hot_reload client_hot_reload          # Linux / single-config
cmake --build . --config Release --target server_hot_reload client_hot_reload   # MSVC multi-config
```

Binaries are placed in `build/examples/hotStandby/server|client[/Release|Debug]`.

### Windows: making the Debug configuration runnable

By default MSVC links the *dynamic* debug CRT (`/MDd`), which requires
`vcruntime140d.dll` and `ucrtbased.dll`. These debug runtime DLLs are **not**
part of normal Visual Studio installations or the redistributable, so a plain
Debug build exits immediately with `STATUS_DLL_NOT_FOUND (0xC0000135)`.

This build tree is therefore configured to statically link the CRT per
configuration (`Release` -> `/MT`, `Debug` -> `/MTd`):

```sh
cmake -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" ..
cmake --build . --config Debug --target server_hot_reload client_hot_reload
```

With this setting both configurations produce standalone executables that run
without any extra DLL search path. (Requires CMake >= 3.15.)

## Running the demo

1. Start the server (from this directory so it finds `model.cfg`):

   ```sh
   cd examples/hotStandby/server
   cp model_v1.cfg model.cfg
   <binary-path>/server_hot_reload ./model.cfg 8102
   # Windows example:
   ..\..\..\build\examples\hotStandby\server\Debug\server_hot_reload.exe .\model.cfg 8102
   ```

2. Start the client in a second terminal:

   ```sh
   <binary-path>/client_hot_reload localhost 8102
   ```

   You will see reports arriving (integrity period is 1 s, BufTm = 50 ms).

3. Trigger the hot-swap while both are running:

   ```sh
   cp model_v2.cfg model.cfg      # in the server directory
   ```

   Within one second the server rebuilds its data model inside the same process.
   The client gets disconnected, reconnects automatically, re-discovers the model
   (now containing AnIn3 / Ind3 and ConfRev = 2) and continues receiving reports.

## Important notes / limitations

- During the swap window (< 1 s) the MMS port is closed; **all clients are
  disconnected** and must reconnect. This is inherent to libiec61850: the MMS
  object tree is created once per `IedServer` instance from the data model.
- Never modify a data model while an `IedServer` is running on it
  (dynamic model create/delete functions are only valid before
  `IedServer_create`).
- Values are preserved across swaps only for points whose short object
  reference still exists in the new model with a compatible type. Points that
  disappear keep living in the cache until overwritten or process exit.
- The value cache here maps only a few basic types (bool, float, int...);
  extend `dataAttributeMatchesCachedValue()` if you need more types.
- The client maintains up to `MAX_SUBSCRIPTIONS` (8) parallel RCB
  subscriptions; a production client would manage this more dynamically.
