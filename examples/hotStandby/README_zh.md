[English](README.md) | 中文

# hotStandby - 数据模型热更新示例（服务端 + 客户端）

本示例演示如何**在不终止宿主进程的前提下，替换运行中服务的完整 IEC 61850
数据模型**，以及客户端如何感知并适配新模型。

它基于 libiec61850 的运行期模型加载能力
（`ConfigFileParser_createModelFromConfigFileEx()`、动态模型 API），
**不使用编译期静态模型**。

## 目录结构

```
hotStandby/
├── README.md                  英文说明
├── README_zh.md               本文件
├── CMakeLists.txt             构建入口
├── server/
│   ├── CMakeLists.txt
│   ├── server_hot_reload.c    服务端（从 .cfg 加载模型、监测文件变化、热切换）
│   ├── model_v1.cfg           初始数据模型（2 个遥测 + 2 个遥信，ConfRev=1）
│   └── model_v2.cfg           更新后的数据模型（+AnIn3、+Ind3，ConfRev=2）
└── client/
    ├── CMakeLists.txt
    └── client_hot_reload.c    客户端（动态发现 + 自动重连）
```

## 工作原理

### 服务端

1. 启动时通过 `ConfigFileParser_createModelFromConfigFileEx()` 从 `model.cfg`
   加载数据模型（该 `.cfg` 格式正是模型生成工具从 ICD/CID/SCL 文件生成的格式），
   并启动一个标准的 `IedServer`。
2. 所有模拟过程值都经过一个**基于对象引用的值缓存**
   （`"GenericIO/GGIO1.AnIn1.mag.f"` -> MmsValue）。缓存是主存储，
   值按引用转发给当前活跃的服务器实例。
3. 每秒轮询一次配置文件（`FileSystem_getFileInfo()`）。检测到变化时：
   - **先**解析并校验新模型（解析失败则旧服务器继续运行，不受影响）
   - 停止/销毁旧的 `IedServer`/`IedModel`
     （**这会关闭所有客户端连接**——这是无法回避的：MMS 映射与模型实例绑定）
   - 用新模型创建新的服务器实例，将缓存中仍存在（类型匹配）的值重新应用，
     然后重新启动

### 客户端

- 使用目录服务动态发现 LD / LN / RCB
  （`getLogicalDeviceList`、`getLogicalDeviceDirectory`、`getLogicalNodeDirectory`）
- **订阅所有发现的 URCB**（不存在 URCB 时回退为订阅所有 BRCB），
  使能报告（dchg/qchg/dupd/integrity/GI），并对每个 RCB 触发 GI 总召基线报告
- 打印每条收到的报告：成员引用、包含原因（reason）、值
- 感知服务端热切换导致的断线，清理订阅并自动重连 → 然后重新发现**新模型**
- 连接期间每 5 秒轮询所有订阅的 `ConfRev`：ConfRev 变化是 IEC 61850 标准的
  "服务端配置已变更"指示

## 实现细节

### 用到的 IEC 61850 机制 / 服务

| 机制 | 是否使用 | 在本示例中的角色 |
|---|---|---|
| MMS 报告 - 非缓存（URCB） | 是 | 主要事件传输通道；`urcbEvents01`（遥信）、`urcbAnalog01`（遥测） |
| MMS 报告 - 缓存（BRCB） | 是 | 发现 `brcbEvents01`；无 URCB 时作为回退使用 |
| 报告触发条件（TrgOps） | 是 | 客户端写入 dchg / qchg / dupd / integrity（IntgPd=1s）/ GI |
| 报告选项域（OptFlds） | 是 | 模型中配置（`options=175`：seqNum、时标、原因、数据集、缓冲溢出、confRev） |
| 目录服务（基于 GetNameList） | 是 | 动态发现 LD → LN → RCB / 数据集；客户端不内置任何静态模型 |
| ConfRev 变更检测 | 是 | "服务端配置已变更"的标准指示机制 |
| 数据模型读取（GetDataValues） | 隐含 | 数据集值只通过报告传输 |
| GOOSE | **否** | 未使用（需要 GSE 控制块 + 以太网层） |
| 采样值 SV（Sampled Values） | **否** | 未使用（需要 SVCB + 专用发布机制） |
| 控制服务（direct/SBO Oper） | **否** | v1/v2 模型中不含可控对象 |
| 日志/台账（LCB/Journal） | **否** | 未使用 |

### 服务端实现

1. **运行期模型加载** —— `ConfigFileParser_createModelFromConfigFileEx(path)`
   在运行期用动态模型 API 构建出完整的 `IedModel`
   （`LogicalDevice_create`、`LogicalNode_create`、`DataObject_create`、
   `DataAttribute_create`、`DataSet_create`、`ReportControlBlock_create`……）。
   `.cfg` 文件格式与模型生成工具（`genmodel.jar`）从 SCL/ICD/CID 文件输出的
   格式完全一致——因此本示例无需改代码即可适配任意真实设备描述文件。
2. **值写入与报告触发** —— 所有更新都走
   `IedServer_updateAttributeValue(server, da, value)`。库内部把值拷贝进模型的
   MmsValue 缓存（`MmsValue_update`），并对所有观察该点的 RCB 评估触发条件
   （`checkForChangedTriggers`）。应用层完全不直接操作报告——只负责推值。
   （本演示未更新 `.q` 品质和 `.t` 时标属性。）
3. **点位查找** ——
   `IedModel_getModelNodeByShortObjectReference(model, "GenericIO/GGIO1.AnIn1.mag.f")`。
   "短引用"指 LD 部分是纯粹的 LD 实例名（不带可配置的 IED 名称前缀）。
4. **为什么换实例而不是原地改模型** —— `IedServer_create()` 内部，MMS 映射层
   （`MmsMapping`）会依据模型树的*裸指针*构建完整的 MMS 对象树
   （域、命名变量、RCB/数据集对应的命名变量列表等）。之后修改或释放模型节点
   会在映射层留下悬垂指针 → 未定义行为/崩溃。所以交换模型的唯一安全方式是：
   stop → destroy → create → start。而 `IedServer_stop()` 会关闭监听套接字
   **以及所有客户端关联**。
5. **跨切换的值迁移** —— 一个写透（write-through）缓存
   （`CachedValue` 结构体，以短对象引用为键，存放 `MmsValue_clone` 克隆）
   位于模拟逻辑与服务器之间：

   ```
   updatePoint(ref, val)
        |-- ValueCache_set(ref, val)            <- 主存储（无条件写入）
        '-- 若当前活跃模型中存在该点（类型校验通过）:
              IedServer_updateAttributeValue(...)
   ```

   创建新服务器实例后，`applyCachedValues()` 遍历缓存，在**新**模型中逐个解析
   引用，检查兼容性（`dataAttributeMatchesCachedValue()`：节点必须是
   DataAttribute，且 `da->type` 能映射到缓存的 `MmsType`），对匹配的值执行应用。
   这就是 v1→v2 切换后 AnIn3 的锯齿相位得以延续（而不是从 0 开始）的原因；
   被删除的点也能优雅降级。
6. **变更检测与两阶段切换** —— 通过 `FileSystem_getFileInfo()`
   （HAL API，Linux/Windows 通用）每秒轮询一次配置文件，比较大小+mtime。
   新模型在停掉旧服务器**之前**完成解析与校验，因此损坏的配置文件永远不会
   弄挂正在运行的服务（输出 "HOT-SWAP ABORTED ... keeping old model"）。

### 客户端实现

1. **连接管理** —— `IedConnection_connect()` 建立 MMS 关联
   （ISO 各层由 libiec61850 内部处理）。每 500 ms 用
   `IedConnection_getState()` 轮询连接状态。
2. **动态发现链路**（客户端不编译任何静态模型）：

   ```
   IedConnection_getLogicalDeviceList(con)                 -> LD 名列表
   IedConnection_getLogicalDeviceDirectory(con, ld)        -> LN 名列表
   IedConnection_getLogicalNodeDirectory(con, lnRef, ACSI_CLASS_URCB/BRCB)
                                                           -> RCB 名列表
   完整 RCB 引用: "<LD>/<LN>.RP.<name>"（非缓存）或 ".BR.<name>"（缓存）
   ```

   所有发现的 URCB 都会被订阅（无 URCB 时回退订阅所有 BRCB）。
   最多维护 `MAX_SUBSCRIPTIONS`（8）个并行订阅。
3. **单个 RCB 的订阅时序**（顺序很重要！）：

   ```
   rcb = IedConnection_getRCBValues(con, &err, rcbRef, NULL)   读 RCB 参数
         -> 得到 rptId、datSetRef、confRev
   members = IedConnection_getDataSetDirectory(con, &err, datSetRef)
                                                               成员名列表
   IedConnection_installReportHandler(con, rcbRef, rptId, handler, members)
                                                               必须在使能之前！
   ClientReportControlBlock_setTrgOps(rcb, dchg|qchg|dupd|integrity|GI)
   ClientReportControlBlock_setRptEna(rcb, true)
   IedConnection_setRCBValues(con, &err, rcb,
           TRG_OPS|RPT_ENA|GI (+URCB 还要 RESV), true)          原子使能+总召
   ```

4. **报告解码** —— 回调收到 `ClientReport`。值数组与数据集条目一一对齐；
   通过 `ClientReport_getReasonForInclusion(report, i)` 过滤未包含的条目，
   返回值为 `dchg / qchg / dupd / intg(egrity) / GI` 之一。
   成员名来自订阅时捕获的数据集目录。
5. **韧性循环** —— 外层循环带重试地重连；内层循环监测连接状态，
   并每约 5 秒轮询所有订阅的 ConfRev。断线时卸载全部 handler、销毁 RCB 对象
   和成员列表（`teardownAllSubscriptions()`），下一代连接做全量重新发现——
   这正是客户端在服务端热切换后得知点位增删的方式。

## 编译

作为库构建系统的一部分：

```sh
mkdir build && cd build
cmake ..
cmake --build . --target server_hot_reload client_hot_reload          # Linux / 单配置生成器
cmake --build . --config Release --target server_hot_reload client_hot_reload   # MSVC 多配置
```

产物位于 `build/examples/hotStandby/server|client[/Release|Debug]`。

### Windows：让 Debug 配置可以运行

MSVC 默认链接*动态*调试版 CRT（`/MDd`），需要 `vcruntime140d.dll` 和
`ucrtbased.dll`。这两个调试运行库 DLL **既不在常规 Visual Studio 安装里、
也不在 redistributable 里**，所以默认编出的 Debug 版启动即退出，
报错 `STATUS_DLL_NOT_FOUND (0xC0000135)`。

因此本构建树配置为按配置静态链接 CRT（`Release`→`/MT`，`Debug`→`/MTd`）：

```powershell
cd build
cmake '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' ..
cmake --build . --config Debug --target server_hot_reload client_hot_reload
```

这样两个配置产出的都是独立可执行文件，不需要额外设置 DLL 搜索路径
（需要 CMake >= 3.15）。若要恢复默认动态链接，删除 build 目录重新
`cmake ..` 即可。

## 运行测试

1. 启动服务端（必须在本目录下运行，才能找到 `model.cfg`）：

   ```sh
   cd examples/hotStandby/server
   cp model_v1.cfg model.cfg          # 首次运行需要；Windows 用 Copy-Item
   <binary-path>/server_hot_reload ./model.cfg 8102
   # Windows 示例:
   ..\..\..\build\examples\hotStandby\server\Debug\server_hot_reload.exe .\model.cfg 8102
   ```

2. 第二个终端启动客户端：

   ```sh
   <binary-path>/client_hot_reload localhost 8102
   ```

   可以看到报告持续到达（integrity 周期 1 s，BufTm = 50 ms）。

3. 两端都在运行时触发热切换：

   ```sh
   cp model_v2.cfg model.cfg      # 在 server 目录执行；Windows 用 Copy-Item .\model_v2.cfg .\model.cfg -Force
   ```

   一秒之内服务端会在同一进程内重建数据模型。客户端断开后自动重连、
   重新发现模型（此时包含 AnIn3 / Ind3，且 ConfRev=2），继续接收报告。

## 重要说明 / 已知限制

- 切换窗口期（< 1 s）MMS 端口关闭，**所有客户端都会断开**，必须重连。
  这是 libiec61850 固有的：MMS 对象树在 `IedServer` 创建时一次性从数据模型
  生成。
- 绝不要在 `IedServer` 运行期间修改其数据模型
  （动态模型 create/delete 函数只在 `IedServer_create` 之前有效）。
- 只有短对象引用仍存在于新模型且类型兼容的点，其值才会跨切换保留。
  从模型中消失的点仍留在缓存里，直到被覆盖或进程退出。
- 本例的值缓存只映射了少数基本类型（bool、float、int…）；需要更多类型请扩展
  `dataAttributeMatchesCachedValue()`。
- 客户端最多维护 `MAX_SUBSCRIPTIONS`（8）个并行 RCB 订阅；
  生产级客户端应做更动态的管理。
