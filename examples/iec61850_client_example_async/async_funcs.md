## 📊 异步 API 调用汇总

| 操作 | 异步函数 | 回调函数 | 功能 |
|------|---------|---------|------|
| 连接 | `IedConnection_connectAsync` | -（轮询状态） | 非阻塞连接到服务器 |
| 获取服务器目录 | `IedConnection_getServerDirectoryAsync` | `getServerDirectoryHandler` | 获取所有逻辑设备名称 |
| 获取设备变量 | `IedConnection_getLogicalDeviceVariablesAsync` | `getNameListHandler` | 获取逻辑设备下的变量列表 |
| 获取设备数据集 | `IedConnection_getLogicalDeviceDataSetsAsync` | `getNameListHandler` | 获取逻辑设备下的数据集列表 |
| 读取数据对象 | `IedConnection_readObjectAsync` | `readObjectHandler` | 读取单个数据对象的值 |
| 读取数据集 | `IedConnection_readDataSetValuesAsync` | `readDataSetHandler` | 读取数据集中所有值 |
| 写入数据集 | `IedConnection_writeDataSetValuesAsync` | `writeDataSetHandler` | 写入数据集中的多个值 |
| 获取变量类型 | `IedConnection_getVariableSpecificationAsync` | `getVarSpecHandler` | 获取变量的类型定义 |
| 控制操作 | `ControlObjectClient_operateAsync` | `controlActionHandler` | 执行控制命令 |
| 释放连接 | `IedConnection_releaseAsync` | -（轮询状态） | 非阻塞断开连接 |

---

## 💡 关键设计要点

1. **非阻塞模式**：所有 `*Async` 函数调用后立即返回，不等待服务器响应
2. **回调驱动**：每个请求都对应一个回调函数，响应到达时自动调用
3. **状态轮询**：通过 `IedConnection_getState()` 轮询连接状态变化
4. **内存管理**：回调完成后需要手动释放 `MmsValue` 等资源
5. **单线程并发**：可以在一个线程中同时发起多个请求，由库内部管理并发