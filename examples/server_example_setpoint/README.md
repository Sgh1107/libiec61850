# server_example_setpoint — 遥调（模拟量设定值调节）测试专用示例

只做一件事：让客户端通过 IEC 61850 的**模拟量控制（APC）**下发设定值，
服务端校验、回写、并驱动一个"实际输出"点跟随变化，同时把过程打在终端上。

## 文件

| 文件 | 说明 |
|---|---|
| `setpoint_tests.cid` | 数据模型（**模型源头，改模型改这里**） |
| `static_model.c` / `.h` | 由 cid 生成的模型代码，**不要手改** |
| `server_example_setpoint.c` | 服务端业务代码 |
| `CMakeLists.txt` / `Makefile` | 构建脚本 |

## 数据模型

IED `setpointIO`，LD `GenericIO`，LN `GGIO1`：

| 对象 | CDC | FC | 含义 |
|---|---|---|---|
| `GGIO1.AnOut1` | APC | CO/MX/CF | 遥调目标通道 1（`mxVal.f` 为设定值） |
| `GGIO1.AnOut1Fb` | MV | MX | 通道 1 实际输出（服务端自动跟随） |
| `GGIO1.AnOut2` | APC | CO/MX/CF | 遥调目标通道 2 |
| `GGIO1.AnOut2Fb` | MV | MX | 通道 2 实际输出 |

- `ctlModel = direct-with-enhanced-security`（3）：无需 select，直接 operate，
  且会回 CommandTermination 回执（成功 CTM+ / 失败 CTM- + LastApplError）。
- 四个点都在数据集 `LLN0$SetpointEvents` 里，配了报告控制块 `SetpointRCB`，订阅即可看到变化。

## 修改模型后重新生成

```bash
cd tools/model_generator_dotnet/Tools/bin/Release/net8.0
dotnet Tools.dll 1 <path>/setpoint_tests.cid -ied setpointIO -ap accessPoint1 \
       -out static_model -modelprefix iedModel
```

生成物拷回本目录覆盖。Linux 下也可以用老工具：
`java -jar tools/model_generator/genmodel.jar setpoint_tests.cid`
（或直接 `make model`）。

> 注意：本仓库的生成器（.NET 与 Java 两版）原来把 `TrgOps` 的 8 号位读成非标准属性
> `period`，标准属性名 `integ` 会被静默丢弃 —— 也就是"XML 写了 integ，生成物里没有这一位"。
> 已在 `tools/model_generator_dotnet/SCLParser/src/SclElements.cs` 与
> `tools/model_generator/src/com/libiec61850/scl/model/TriggerOptions.java` 里改为
> 优先读 `integ`、兼容旧的 `period`。改动生成器后需重新 `dotnet build Tools/Tools.csproj -c Release`。

## 报告（RCB）侧：对照《setpoint 报告侧改动要求》

| 条款 | 落地情况 |
|---|---|
| 2.1 RCB 引用 | `setpointIOGenericIO/LLN0.RP.SetpointRCB`，单实例、名字不带序号 |
| 2.2 TrgOps | `dchg qchg dupd integ gi` 全开，生成物核对为 **0x1F** |
| 2.3 GI 前置条件 | 服务端不需特例；客户端在同一次 Set 里先写 `RptEna=true` 再写 `GI=true` 即可 |
| 2.4 OptFields | `seqNum timeStamp dataSet reasonCode entryID configRef` = **0xEF**（`dataRef` 默认不加，见下） |
| 2.5 indexed | 已去掉，写 `indexed="false"` |
| 2.6 RptEnabled max | `max="1"`（由 `indexed="false"` 决定，见下） |
| 2.7 数据集 | 4 个 FLOAT32 叶子；顺序即报告元素顺序；`mxVal.f` / `mag.f` 不指向结构体 |
| 2.8 IntgPd / BufTm | `intgPd=1000`（`integ` 已开，周期报告真实生效）、`bufTime=0` |
| 3.2 属性可写 | 仍是 `IedServer_create(&iedModel)`，六项可写默认全开 |
| 4.4 报告触发 | 所有值改动都走 `IedServer_update*` 接口 |
| 4.5 / S1 原子性 | `controlHandlerForSetpoint` 与 `updateOutputs` 都把一组更新包在 `lockDataModel/unlock` 里 |
| 4.6 报告条数 | 改为固定 **6 步**到位（`OUTPUT_FOLLOW_STEPS`），一次遥调 = 6 条变位报告 |
| S2 事件输出 | 注册 RCB 事件回调 + 连接指示回调，`RESERVED/ENABLE/GI/REPORT_CREATED` 等打到标准输出 |

### 三处必须知道的不一致

1. **`indexed` 与 `RptEnabled max` 互斥。** 生成器只在 `indexed="true"` 时才按 `max` 生成
   `SetpointRCB01…N`；想要文档 2.1 要求的 `…RP.SetpointRCB`（不带序号），就必须
   `indexed="false"`，而生成器随即要求 `max` 必须为 1。
   也就是说"名字正好是 SetpointRCB"和"多个 RCB 实例允许并发客户端"不能同时成立。
   这里按 2.1（必须项）取舍：单实例，第二个客户端会明确拿到拒绝，正好对上自查第 9 步。
   真要多客户端并发，改回 `indexed="true"` + `max="4"`，但客户端引用必须换成 `SetpointRCB01…04`。

2. **变位触发位必须声明在叶子上。** 数据集成员指向 `mxVal.f` / `mag.f`，
   而服务端检测变位只看"被 update 的那个属性自己的 `triggerOptions`"
   （`ied_server.c` 的 `checkForChangedTriggers`），父结构 `mxVal`/`mag` 上的 `dchg`
   **不会**被叶子继承。只写在父结构上时表现为：GI 报告有、周期报告有、**变位报告一条没有**。
   现在 `DAType AnalogueValue_1` 的 `BDA f` 上写了 `dchg="true" dupd="true"`。

3. **`dataRef` 默认关掉。** 文档 2.4 说它是可选项。数据集固定 4 个成员，
   接收侧本来就按下标取值，加上只会让报文变大，所以保持 `0xEF`。
   需要自描述报告时再把 `dataRef="true"` 打开（生成物 `OptFlds` 会变成 0xFF）。

### 实测记录（端口 8102，libiec61850 1.6 自带客户端）

```
[RCB] event=RESERVED → ENABLE → GI → REPORT_CREATED    启用 + 全询正常
[RCB] event=SET_PARAM param=RptEna err=OK               属性可写
收到 GI 报告 1 条（4 个成员，reason=GI）
遥调 250 → 收到 6 条变位报告（41.67 / 83.33 / 125 / 166.67 / 208.33 / 250.00），seq 连续
IntgPd=1000 → 每 5 秒 5 条 reason=INTEGRITY 的周期报告
RptEna=false → DISABLE；Resv=false → UNRESERVED；随即重新启用 → 成功
强杀客户端进程 → 服务端自动 DISABLE + UNRESERVED，重连后可立即重新启用
```

> 已知的接收侧现象（两条，都是库行为，不是模型配错）：
>
> 1. 变位报告里 `reason` 有时被 libiec61850 1.6 的**客户端**解成 `UNKNOWN(32)`
>    （GI / 周期报告的 reason 正常）。服务端日志的 `REPORT_CREATED` 与报告里的值、`seqNum`
>    都是对的，判断"报告为什么来"时可以先看服务端事件，不要只看客户端解出来的 reason。
> 2. 非缓冲 RCB 发第一条报告时，服务端会**就地改写** RCB 的 `OptFlds`
>    （`reporting.c` 把 bufOvfl、entryID 两位清掉，这两项只对 BRCB 有意义）。
>    所以模型写的 `0xEF` 在客户端订阅后可能读回 `0x8F`；这不是你的写操作失败。
>    诊断客户端时建议只比对 `TrgOps / IntgPd / DatSet / RptEna`，`OptFlds` 只看 `reason`、`seqNum` 位。

## 运行

```bash
./server_example_setpoint [port]     # 默认 102，需要管理员权限
```

终端输出示例：

```
[   2.83s] [CTRL] 来自 127.0.0.1:54970 遥调 GGIO1.AnOut1: 0.00 -> 250.00  (ctlNum=1)
[   2.95s] [FOLLOW] GGIO1.AnOut1 实际输出已跟随到 250.00
[  14.87s] [CTRL] 来自 127.0.0.1:54970 遥调 GGIO1.AnOut1: 250.00 -> 2000.00  (ctlNum=2)
  [拒绝] GGIO1.AnOut1 超出量程 [0, 1000]，2000.00 未写入
```

## 手动改值（测客户端 RCB 用）

服务端跑起来后可以直接在它的终端里敲命令改值，用来观察客户端收报告是否正确，
不用每次去写一个 operate 报文：

| 命令 | 作用 |
|---|---|
| `1 <值>` | 写 `AnOut1.mxVal.f`（设定值，0~1000），反馈会分 6 步跟随 → **6 条变位报告** |
| `2 <值>` | 写 `AnOut2.mxVal.f` |
| `3 <值>` | **直接**写 `AnOut1Fb.mag.f`（绕过跟随）→ **1 条变位报告** |
| `4 <值>` | 直接写 `AnOut2Fb.mag.f` |
| `s` | 打印四个点当前值 + 报告订阅状态（预留/启用/对端/累计报告数） |
| `?` | 重印命令说明 |
| `q` | 退出服务端（等同 Ctrl+C） |

```
> 1 250
[   6.12s] [手动] GGIO1.AnOut1 设定值 -> 250.00（反馈开始跟随）
> 3 700
[   9.44s] [手动] GGIO1.AnOut1Fb 反馈值 -> 700.00（直接写，不触发跟随）
> s
  ---- 当前值 ----
   [1] GGIO1.AnOut1     设定   250.00   反馈   250.00
   [2] GGIO1.AnOut2     设定     0.00   反馈     0.00
  ---- 报告订阅（按 RCB 事件推出来的） ----
   RCB setpointIOGenericIO/LLN0.RP.SetpointRCB
   预留 是   启用 是   对端 127.0.0.1:56746
   报告累计 14 条，最后一条 0.31 秒前
```

几点设计上的取舍：

- **命令跑在独立线程**。主循环每 100 ms 跑一次 `updateOutputs()`，在主线程里阻塞读 stdin
  会把反馈跟随和退出都卡住。
- **写入走 `applySetpoint()` / `applyFeedback()`**，与控制处理函数共用同一条路径，
  都带 `lockDataModel/unlock` 且走 `IedServer_update*`——否则不会产生报告（见 4.4）。
- **区分"写设定值"和"写反馈值"**：前者是 6 条报告，后者正好 1 条，核对 `seqNum` 递增最方便。
- 重复写同一个值不会产生变位报告，只会产生一条 **data-update** 报告（验证 `dupd` 用）。
- 订阅状态是服务端从 RCB 事件里自己记的：1.6 没有"服务端读 RCB 值"的接口。

## 客户端写法

```c
/* 注意：MMS 域名带 IED 名前缀 → setpointIOGenericIO，别硬编码 GenericIO */
ControlObjectClient c = ControlObjectClient_create("setpointIOGenericIO/GGIO1.AnOut1", con);
ControlObjectClient_setOrigin(c, NULL, 3);          /* 3 = remote-control */

MmsValue* v = MmsValue_newFloat(250.0f);            /* 直接传 float，库自动包成 AnalogueValue */
ControlObjectClient_operate(c, v, 0);
```

## 四个坑

1. **APC 的 ctlVal 是结构体**（AnalogueValue），服务端 handler 收到的 `MmsValue` 类型是
   `MMS_STRUCTURE`，必须 `MmsValue_getElement(value, 0)` 才能取到 `f`。
2. **MMS 域名 = IED 名 + LD inst**：`setpointIO` + `GenericIO` → `setpointIOGenericIO`。
   客户端最好先 `IedConnection_getLogicalDeviceList()` 再拼引用。
3. **operate 返回 true ≠ 成功**：direct 模型的 operate 响应先回正，
   真正的结果在 CommandTermination。超量程那次的 operate 也返回 true。
4. **量程校验在服务端做**（0~1000），拒绝时通过 `ControlAction_setAddCause()` 把原因带回客户端。
