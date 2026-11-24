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

## 运行

```bash
./server_example_setpoint [port]     # 默认 102，需要管理员权限
```

终端输出示例：

```
[   2.83s] [CTRL] 来自 127.0.0.1:54970 遥调 GGIO1.AnOut1: 0.00 -> 250.00  (ctlNum=1)
[   7.78s] [FOLLOW] GGIO1.AnOut1 实际输出已跟随到 250.00
[  14.87s] [CTRL] 来自 127.0.0.1:54970 遥调 GGIO1.AnOut1: 250.00 -> 2000.00  (ctlNum=2)
           ↳ 超出量程 [0, 1000]，拒绝
```

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
