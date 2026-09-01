# model_generator_dotnet 使用手册

`tools/model_generator_dotnet/` 是 libiec61850 项目中基于 .NET 编写的 IEC 61850 模型生成工具集。它读取 SCL 系列文件（`.icd` / `.cid` / `.scd` / `.iid`），生成 libiec61850 运行时所需的数据模型，可选择输出为：

- **静态模型（Static Model）**：生成 C 语言的 `.c` / `.h` 源文件，编译进服务端可执行程序，效率最高；
- **动态模型（Dynamic Model）**：生成 `.cfg` 文本配置文件，服务端启动时通过 `ConfigFileParser` 加载，便于免重编译更换模型。

它是 `tools/model_generator/`（早期的 Java 版本 `genmodel.jar` / `gendyncode.jar`）的 .NET 8 重写版本。功能对等，命令行接口有所简化。

---

## 1. 目录结构

```text
tools/model_generator_dotnet/
├── README.md                       # 简短的官方说明
├── Tools_ModelGenerator.sln        # Visual Studio 解决方案
│
├── SCLParser/                      # SCL 文件解析库 (netstandard2.0)
│   ├── SCLParser.csproj
│   └── src/                        # SCL 各元素的类定义：
│       ├── SclDocument.cs          #   顶层 SCL 文档
│       ├── DataModel.cs            #   IED/LD/LN/DO/DA 运行时对象
│       ├── DynamicModelGenerator.cs#   .cfg 文本生成器（被上层调用）
│       ├── SclDataTypeTemplates.cs #   <DataTypeTemplates> 解析
│       ├── SclCommunication.cs     #   <Communication> 解析
│       ├── SclGSEControl.cs / SclSMVControl.cs / SclLogControl.cs / SclSettingControl.cs
│       └── ...
│
├── StaticModelGenerator/           # 静态模型生成器 (netstandard2.0)
│   ├── StaticModelGenerator.csproj
│   └── src/
│       ├── StaticModelGenerator.cs #   顶层，遍历 IEDDataModel 输出 C/H
│       └── C_Structures.cs         #   一一对应 libiec61850 中的
│                                   #   IedModel/LogicalDevice/LogicalNode/
│                                   #   DataObject/DataAttribute/DataSet/
│                                   #   ReportControlBlock/GSEControlBlock/
│                                   #   SVControlBlock/LogControlBlock/... 结构体
│
├── DynamicModelGenerator/          # 动态模型生成器 (netstandard2.0)
│   ├── DynamicModelGenerator.csproj
│   └── DynamicModel.cs             #   打开 SCL、调用 SCLParser 里的 DynamicModelGenerator，输出 .cfg
│
└── Tools/                          # 命令行入口 (net8.0, Exe)
    ├── Tools.csproj
    ├── Program.cs                  # 参数解析 + 调度 Static/Dynamic 生成器
    └── ICDFiles/                   # 内置若干示例 ICD/CID/SCD 文件
```

各项目之间的引用关系：

```
Tools ──▶ StaticModelGenerator ──▶ SCLParser
     └──▶ DynamicModelGenerator ─▶ SCLParser
```

只有 `Tools` 是可执行程序（`OutputType=Exe`，`net8.0`），其余三个都是 `netstandard2.0` 类库，方便被其它 .NET 项目（比如你自己的桌面工具、Visual Studio 扩展等）直接引用。

---

## 2. 环境准备

- **.NET SDK 8.0 或更高版本**（`Tools/Tools.csproj` 的 `TargetFramework` 是 `net8.0`）。
  - 检查：`dotnet --version`
  - Windows / Linux / macOS 均可。
- 代码里没有平台相关依赖，Windows PowerShell、Linux bash 都能跑。
- 无需额外 NuGet 包。

---

## 3. 编译

在仓库任意位置打开终端，进入模块根目录：

```powershell
cd tools\model_generator_dotnet
```

### 3.1 用 SLN 编译整个方案

```powershell
dotnet build .\Tools_ModelGenerator.sln -c Release
```

编译产物默认位于 `Tools\bin\Release\net8.0\`，其中：

- `Tools.dll` —— 主入口
- `Tools.exe` —— Windows 下的启动器
- `SCLParser.dll` / `StaticModelGenerator.dll` / `DynamicModelGenerator.dll`
- `ICDFiles\` —— 通过 `EmbeddedResource + CopyToOutputDirectory=PreserveNewest` 复制过来的所有示例文件

### 3.2 只编译入口项目

如果只想编译主程序：

```powershell
dotnet build .\Tools\Tools.csproj -c Release
```

依赖的三个库会因项目引用被自动一并编译。

### 3.3 发布为独立可执行文件（可选）

若希望在没有安装 .NET 的机器上运行：

```powershell
dotnet publish .\Tools\Tools.csproj -c Release -r win-x64 --self-contained true -o publish
```

`publish\Tools.exe` 即可独立运行。

---

## 4. 命令行用法

`Tools` 的参数由 `Tools/Program.cs` 手工解析，格式为：

```
dotnet Tools.dll <generator option> <ICD file> [选项...]
```

或 Windows 直接：

```
Tools.exe <generator option> <ICD file> [选项...]
```

### 4.1 位置参数

| 位置 | 含义 | 说明 |
|------|------|------|
| `<generator option>` | 生成模式 | `1` = 静态模型（C/H），`2` = 动态模型（.cfg） |
| `<ICD file>` | SCL 文件路径 | 支持 `.icd` / `.cid` / `.scd` / `.iid`，任何符合 SCL 规范的 XML 都可 |

如果 `<generator option>` 不是 `1` 或 `2`，程序会打印帮助并退出。
如果不传任何参数，同样只打印帮助。

### 4.2 可选参数

参数解析顺序不敏感，只识别以下关键字（`Tools/Program.cs`）：

| 参数 | 含义 | 默认值 | 备注 |
|------|------|--------|------|
| `-ied <ied-name>` | 指定要生成的 IED 名称 | `SCL` 中第一个 `<IED>` | 一份 SCD 里可能包含多个 IED |
| `-ap <access-point-name>` | 指定 AccessPoint | 该 IED 的第一个 AccessPoint | 在 IED 内选择哪个访问点 |
| `-out <output-name>` | 输出文件名（不含扩展名） | `static_model` | 静态模式会生成 `<out>.c` + `<out>.h`；动态模式生成 `<out>.cfg` |
| `-modelprefix <model-prefix>` | C 变量前缀 | `iedModel` | 仅对静态模型有意义，是全局 `IedModel` 变量的名字，也是所有 `LogicalDevice/LogicalNode/...` 变量名的前缀 |
| `-initializeonce` | 只初始化一次 | 关闭 | 仅静态模型；生成的 `initializeValues()` 里每个 DA 会先判断 `mmsValue == NULL` 再赋值，防止运行时重复初始化覆盖 |

未识别的选项会打印：`Unknown option: "xxx"` 但不会终止。

### 4.3 参数解析细节

- `-ap`、`-ied`、`-out`、`-modelprefix` 后的下一个 `args[i+1]` 会被读作值，然后循环下标 `i++` 跳过；
- `-initializeonce` 没有值，是个开关；
- **参数顺序有隐式约束**：`Program.cs` 中 `args[0]` 必须是 `1` 或 `2`，`args[1]` 必须是 ICD 路径，其余可选参数从 `args[2]` 开始。

---

## 5. 使用示例

以下命令都假设已经 `cd` 到 `tools\model_generator_dotnet\Tools\bin\Release\net8.0\`（编译产物目录），示例 ICD 就在同级的 `ICDFiles\` 里。

### 5.1 生成静态模型（默认前缀 `iedModel`）

```powershell
dotnet Tools.dll 1 ICDFiles\genericIO.icd -ied simpleIO -ap accessPoint1 -out static_model -modelprefix iedModel
```

输出：

- `static_model.c`
- `static_model.h`

生成的 `.h` 中会包含：

```c
#ifndef STATIC_MODEL_H_
#define STATIC_MODEL_H_

#include <stdlib.h>
#include "iec61850_model.h"

extern IedModel iedModel;

extern LogicalDevice iedModel_GenericIO;
extern LogicalNode   iedModel_GenericIO_LLN0;
extern DataObject    iedModel_GenericIO_LLN0_Mod;
extern DataAttribute iedModel_GenericIO_LLN0_Mod_stVal;
/* ... */

#define IEDMODEL_GENERICIO (&iedModel_GenericIO)
#define IEDMODEL_GENERICIO_LLN0 (&iedModel_GenericIO_LLN0)
#define IEDMODEL_GENERICIO_LLN0_MOD (&iedModel_GenericIO_LLN0_Mod)
/* ... */

#endif /* STATIC_MODEL_H_ */
```

生成的 `.c` 中会包含：

- 所有 `DataSet` / `DataSetEntry`
- 所有 `LogicalDevice` / `LogicalNode` / `DataObject` / `DataAttribute` 的 C 结构体实例
- 所有 `ReportControlBlock` / `GSEControlBlock` / `SVControlBlock` / `LogControlBlock` / `Log` / `SettingGroupControlBlock`
- 一个 `initializeValues()` 函数，把从 SCL `<DAI><Val>` 中读到的默认值以 `MmsValue_new...(...)` 的形式绑定到对应 DA
- 顶层 `IedModel iedModel = {...}`，其 `initializer` 字段指向 `initializeValues`

把这两个文件与 libiec61850 服务端一起编译，即可跑起来。

### 5.2 生成静态模型（自定义前缀）

```powershell
dotnet Tools.dll 1 ICDFiles\simpleIO_direct_control.icd `
    -ied TEMPLATE -ap accessPoint1 `
    -out my_model -modelprefix myModel
```

此时全局对象改叫 `myModel`，所有派生 `#define` 也换成 `MYMODEL_*` 前缀。这在同一进程里加载多个模型时很有用。

### 5.3 启用 `-initializeonce`

```powershell
dotnet Tools.dll 1 ICDFiles\genericIO.icd -out static_model -modelprefix iedModel -initializeonce
```

差异发生在 `initializeValues()`：

```c
/* 未开启 */
iedModel_GenericIO_GGIO1_Mod_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);

/* 开启 -initializeonce */
if (!iedModel_GenericIO_GGIO1_Mod_stVal.mmsValue)
    iedModel_GenericIO_GGIO1_Mod_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);
```

场景：当运行时可能多次调用初始化函数、或期望在热加载配置后保留旧值时使用。

### 5.4 生成动态模型

```powershell
dotnet Tools.dll 2 ICDFiles\genericIO.icd -ied simpleIO -ap accessPoint1 -out generic_io
```

输出：`generic_io.cfg`。文件是一个类似小型 DSL 的文本：

```text
MODEL(simpleIO){
LD(GenericIO){
LN(LLN0){
DO(Mod 0){
DA(stVal 0 12 0 1 0)=1;
DA(q 0 23 0 2 0);
DA(t 0 22 0 0 0);
DA(ctlModel 0 12 4 0 0)=0;
}
...
DS(Events){
DE(GGIO1$ST$SPCSO1$stVal);
DE(GGIO1$ST$SPCSO2$stVal);
...
}
RC(EventsRCB - 0 Events 4294967295 20 47 50 1000);
GC(gse1 AppID1 dataSet1 1 0 10 1000){
PA(4 1 1000 010CCD010001);
}
}
}
}
```

对应的解释：

- `MODEL(...)` / `LD(...)` / `LN(...)` / `DO(...)` / `DA(...)` —— 模型层次结构；
- `DA` 的参数依次为：数组长度、`AttributeType`、`FunctionalConstraint`、`TrgOps`、`sAddr`；`=值` 是默认值；
- `DS(...) { DE(...) }` —— 数据集及其 FCDA；
- `RC(...)` —— ReportControlBlock；
- `LC(...)` —— LogControlBlock；`LOG(...)` —— Log；
- `GC(...) { PA(...) }` —— GOOSE 控制块及其物理通信地址（VLAN priority、VLAN id、AppID、MAC）；
- `SMVC(...) { PA(...) }` —— 采样值控制块；
- `SG(actSG numOfSGs)` —— 设置组控制。

服务端加载：

```c
#include "iec61850_config_file_parser.h"

FILE *cfg = fopen("generic_io.cfg", "r");
IedModel *model = ConfigFileParser_createModelFromConfigFileEx(cfg);
fclose(cfg);
IedServer server = IedServer_create(model);
IedServer_start(server, 102);
```

### 5.5 从多 IED 的 SCD 中挑一个

```powershell
dotnet Tools.dll 1 ICDFiles\simpleIO_direct_control_goose.scd `
    -ied IED2 -ap AP1 -out ied2_model -modelprefix ied2
```

若 `-ied` 或 `-ap` 未指定：

- `Program.cs` → `StaticModelGenerator.cs` / `DynamicModel.cs` 会分别取 `sclParser.IEDs.First()` 和 `ied.AccessPoints.First()`；
- 找不到时抛异常 `"IED model not found in SCL file! Exit."` 或 `"AccessPoint not found in SCL file! Exit."`。

### 5.6 直接调用 `dotnet run`（开发时）

不必先 `build`，可在 `Tools` 目录下：

```powershell
cd tools\model_generator_dotnet\Tools
dotnet run -- 1 ICDFiles\genericIO.icd -out ..\..\..\out\generic_io -modelprefix iedModel
```

`--` 后面的所有参数都会原样透传给 `Main(string[] args)`。

---

## 6. 生成的静态模型与 libiec61850 结构体的映射

以 `StaticModelGenerator/src/C_Structures.cs` 中的类名 → libiec61850 C 结构体：

| .NET 类 | 生成的 C 结构体 | 说明 |
|---------|------------------|------|
| `C_IEDModelStructure` | `IedModel` | 顶层模型；`modelPrefix` 决定变量名 |
| `C_LogicalDeviceStructure` | `LogicalDevice` | 一个 LD |
| `C_LogicalNodeStructure` | `LogicalNode` | 一个 LN |
| `C_DataObjectStructure` | `DataObject` | 支持数组（`elementCount`/`arrayIndex`） |
| `C_DataAttributeStructure` | `DataAttribute` | 携带 `AttributeType`、`FC`、触发选项、默认 `MmsValue` |
| `C_DataSetStructure` / `C_DatasetEntry` | `DataSet` / `DataSetEntry` | 数据集及其 FCDA 项 |
| `C_ReportControlBlockStructure` | `ReportControlBlock` | 若 `indexed="true"` 会生成多个实例，命名附加 `01`/`02`... |
| `C_GSEControlBlockStructure` | `GSEControlBlock` (+ 静态 `PhyComAddress`) | 需要 `<Communication>` 中的 `<GSE>` 提供 MAC/VLAN 信息 |
| `C_SMVControlBlockStructure` | `SVControlBlock` (+ 静态 `PhyComAddress`) | 同上，来自 `<SMV>` |
| `C_LogControlBlockStructure` | `LogControlBlock` | |
| `C_LogStructure` | `Log` | |
| `C_SettingGroupStructure` | `SettingGroupControlBlock` | 每个 LN 最多一个 |
| `C_InitializeValues` | `initializeValues()` 中的一行赋值 | 支持 INT / ENUM / BOOL / FLOAT / STRING / TIMESTAMP / OCTET_STRING / CODEDENUM 等类型 |

`FC=SE`（Setting Group Editable）的 DA 会额外派生一份 `_SE` 副本，父节点名字里会带 `_SE_` 前缀。这是 `StaticModelGenerator.createDataAttributeCStructure` 中的专门处理。

---

## 7. 作为库嵌入自己的 .NET 项目

三个 `netstandard2.0` 库可以被 WPF/Console/ASP.NET 直接引用。核心用法：

```csharp
using IEC61850.SCL;
using IEC61850.SCL.DataModel;
using StaticModelGenerator;

var doc = new SclDocument("path/to/file.icd");
var ied = doc.IEDs.First();
var ap  = ied.AccessPoints.First();
IEDDataModel model = doc.GetDataModel(ied.Name, ap.Name);

// 生成 C/H
using var cOut = new FileStream("out.c", FileMode.Create);
using var hOut = new FileStream("out.h", FileMode.Create);
new StaticModelGenerator.StaticModelGenerator(
    fileName:        "path/to/file.icd",
    icdFile:         "path/to/file.icd",
    cOut:            cOut,
    hOut:            hOut,
    outputFileName:  "out",
    iedName:         ied.Name,
    accessPointName: ap.Name,
    modelPrefix:     "iedModel",
    initializeOnce:  false);

// 或生成 .cfg
using var cfg = new FileStream("out.cfg", FileMode.Create);
new DynamicModel.DynamicModel("path/to/file.icd", cfg, ied.Name, ap.Name);
```

这样你就能在自己的工具中集成模型生成（比如做 IDE 插件、批量转换器等）。

---

## 8. 内置示例 ICD

`Tools/ICDFiles/` 里的样例都会被 `Tools.csproj` 用 `PreserveNewest` 复制到输出目录，可以拿来做冒烟测试：

| 文件 | 特点 |
|------|------|
| `simpleIO_direct_control.icd` | 最简单的 GGIO + 直接控制模型，适合入门 |
| `simpleIO_direct_control.cid` | 上面的实例化版本 |
| `simpleIO_direct_control_goose.cid` / `.scd` | 包含 GSE 控制块，用于测试 GOOSE 生成 |
| `simpleIO_control_tests.cid` | 完整的控制模型测试用例（多种 ctlModel） |
| `simpleIO_ltrk_tests.icd` | 服务追踪（`LTRK`）相关 |
| `genericIO.icd` | 内嵌完整数据集、Report、GOOSE 等，最有代表性 |
| `complexModel.icd` | 复杂多 LD 模型 |
| `sampleModel.icd` / `sampleModel_with_dataset.icd` | 教学示例 |
| `sampleModel_errors.icd` | 故意包含错误，用于测试 SCLParser 的容错 |
| `inverter3ph.icd` / `inverter_with_report.icd` | 逆变器场景 |
| `cid_example_deadband.cid` | 死区示例 |

推荐首次运行：

```powershell
dotnet Tools.dll 1 ICDFiles\simpleIO_direct_control.icd `
    -out simpleIO -modelprefix iedModel
dotnet Tools.dll 2 ICDFiles\simpleIO_direct_control.icd -out simpleIO
```

对比生成的 `simpleIO.c` / `simpleIO.h` 和 `simpleIO.cfg`，能直观理解两种模型的差异。

---

## 9. 与 Java 版 `model_generator/` 的区别

| 维度 | Java 版 (`tools/model_generator/`) | .NET 版 (`tools/model_generator_dotnet/`) |
|------|------------------------------------|-------------------------------------------|
| 运行时 | JVM（`genmodel.jar` / `gendyncode.jar`） | .NET 8 (`Tools.exe` / `Tools.dll`) |
| 命令 | `java -jar genmodel.jar ...` | `dotnet Tools.dll 1\|2 ...` |
| 静态 vs 动态 | 分成两个 jar | 用 `1`/`2` 一个入口切换 |
| 参数风格 | 位置参数 | `-ied` / `-ap` / `-out` / `-modelprefix` / `-initializeonce` |
| 可嵌入 | 需要 JVM 依赖 | 直接 `dotnet add reference` 引用 |

两者生成的 `.c/.h` 与 `.cfg` 目标格式完全一致，能被同一份 libiec61850 服务端加载。

---

## 10. 常见问题排查

1. **`IED model not found in SCL file! Exit.`**
   - SCL 中不存在你指定的 `-ied` 名字。检查 XML 里 `<IED name="...">` 值，注意大小写与空格。
2. **`AccessPoint not found in SCL file! Exit.`**
   - `-ap` 与 IED 内 `<AccessPoint name="...">` 不匹配。
3. **`GSE not found for GoCB xxx`**
   - `<LN0>` 定义了 `<GSEControl>` 但 `<Communication>` 里没有对应的 `<GSE cbName="xxx">`。缺失的是通信地址，静态模型中该 GoCB 会没有 `PhyComAddress`。
4. **枚举默认值报错 `Value X in DAI ... does not exist in enumerated type Y`**
   - `<DAI><Val>` 里的字符串不在对应 `<EnumType>` 的 `<EnumVal>` 之中。修好 SCL 后重新生成。
5. **`Unknown default value for ... type: OTHER`**
   - SCL 中某个 DA 的类型在生成器里没有明确映射（例如 `Struct`/自定义 `BDA` 未展开）。多数情况下是 SCL 写法不规范导致 SCLParser 无法识别 `bType`。
6. **中文/非 ASCII 值乱码**
   - 生成器直接把 `Val` 写进 `MmsValue_newVisibleString("...")`。`VisibleString` 按 ISO 8859-1 处理，中文应使用 `UNICODE_STRING_255` 或 `OCTET_STRING`，并确认源 SCL 是 UTF-8 保存。
7. **多个 `ReportControl indexed="true"` 生成的实例数量对不上**
   - 由 `<RptEnabled max="N"/>` 控制；未写 `RptEnabled` 时按 `max=1` 生成。见 `StaticModelGenerator.createDataCStructure`。
8. **`-modelprefix` 改了以后 C 代码报重复定义**
   - 记得同时更新头文件中 `extern IedModel <前缀>;` 的引用；使用 `#define IEDMODEL_...` 派生宏时前缀会自动大写。

---

## 11. 快速命令速查

```powershell
# 编译整个方案
dotnet build .\Tools_ModelGenerator.sln -c Release

# 生成静态模型
dotnet Tools.dll 1 <scl-file> [-ied <name>] [-ap <name>] [-out <name>] [-modelprefix <name>] [-initializeonce]

# 生成动态模型
dotnet Tools.dll 2 <scl-file> [-ied <name>] [-ap <name>] [-out <name>]

# 帮助 (等价于不带任何参数)
dotnet Tools.dll
```

至此，`model_generator_dotnet` 里各工具的编译方式、命令行接口、生成产物、映射关系与常见陷阱都已梳理完毕，可以按需集成到你自己的 IEC 61850 服务端构建流程中。