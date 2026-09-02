**这个工具运行在客户端**，用于连接远程的 MMS 服务器（如 IEC 61850 IED），执行各种 MMS 协议操作。

---

## 🎯 功能总结

| 选项 | 功能 | 对应 MMS 服务 |
|------|------|--------------|
| `-i` | 获取服务器身份信息 | Identify |
| `-d` | 列出所有域 | GetDomainNames |
| `-t` | 获取域目录（变量+日志） | GetDomainDirectory |
| `-r` | 读取变量值 | Read |
| `-v` | 读取变量列表（数据集）值 | Read (NamedVariableList) |
| `-z` | 获取变量列表目录 | GetNamedVariableListDirectory |
| `-f` | 列出文件 | GetFileDirectory |
| `-g` | 获取文件属性 | GetFileDirectory (单个) |
| `-x` | 删除文件 | FileDelete |
| `-j` | 读取日志 | ReadJournal |
| `-m` | 打印原始报文（调试） | Raw Message Handler |

---

## 💡 使用建议

| 场景 | 推荐命令 |
|------|---------|
| 快速测试服务器是否在线 | `mms_utility -h 192.168.31.57 -i` |
| 查看服务器有哪些数据 | `mms_utility -h 192.168.31.57 -d` |
| 读取某个变量值 | `mms_utility -h 192.168.31.57 -a "domain" -r "var"` |
| 查看文件列表 | `mms_utility -h 192.168.31.57 -f` |
| 调试协议交互 | `mms_utility -h 192.168.31.57 -m -d` |