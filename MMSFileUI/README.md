# MMSFileUI — MMS 文件浏览器

基于 **libiec61850 动态库**（ctypes 调用）+ **Python tkinter** 的 IEC 61850 MMS 文件服务图形界面。
连接服务器后即可像资源管理器一样浏览文件/目录，无需手动执行 `dir` 命令。

## 功能

- 🔗 连接 / 断开 IEC 61850 服务器（可配置 IP 和端口，默认 102）
- 📂 浏览服务器文件目录，**双击进入子目录**，⬆ 上级 / ⟳ 刷新
- ⬇ 下载服务器文件（状态栏实时显示已接收字节数）
- ⬆ 上传本地文件到服务器当前目录
- 🗑 删除服务器文件（带确认）
- 中文错误提示（对应 `IedClientError` 错误码）

## 运行

```bash
cd MMSFileUI
python main.py
```

依赖：Python 3.8+（仅标准库，无需 pip 安装任何包）。

## 动态库

程序启动时按以下顺序自动查找 `iec61850.dll`（Linux 下为 `libiec61850.so`）：

1. `../libiec61850-1.6/build_win_vs2022/src/Debug/`（及 Release）
2. `../libiec61850-1.6/build*/src/Debug/`
3. 本程序目录下的 `iec61850.dll`
4. 系统搜索路径（PATH）

如果放到了其他位置，也可以在代码里显式指定：

```python
from mms_client import MmsFileClient
client = MmsFileClient(lib_path=r"D:\path\to\iec61850.dll")
```

## 配套服务器

可用项目自带的示例服务器测试（文件存储根目录为运行目录下的 `vmd-filestore/`）：

```bash
cd libiec61850-1.6/build_win_vs2022/examples/server_example_files/Debug
mkdir vmd-filestore
server_example_files.exe 8102
```

GUI 中填 IP `127.0.0.1`、端口 `8102` 后点“连接”。

## 文件结构

| 文件 | 说明 |
| --- | --- |
| `main.py` | GUI 界面（ttk/clam 主题，所有 MMS 操作在工作线程中执行，界面不卡顿） |
| `mms_client.py` | ctypes 封装：连接、列目录、下载（回调）、上传、删除 |

## 注意事项

- MMS 协议没有独立的“目录列举”区分文件/目录，程序采用“双击尝试列子目录”的探测方式；如果不是目录会在状态栏提示
- 服务器端可通过 `MmsServer_installFileAccessHandler` 限制删除/重命名等操作（见 `server_example_files.c` 的 `fileAccessHandler`），被拒绝时 GUI 会显示“访问被禁止”
- 双击进入目录、返回上级均基于路径拼接（`subdir/name`），与 `file-tool` 的 `subdir` 命令一致
