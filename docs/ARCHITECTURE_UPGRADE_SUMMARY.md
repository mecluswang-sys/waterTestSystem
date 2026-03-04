# 项目架构升级完成 - 终端-操作台(Terminal-Station)模式

## 📋 变更概述

已成功将项目从**单一PC结构**升级为**一个终端托四个操作台的分布式架构**。

## 🏗️ 新架构组成

### 1. 中央终端(Terminal Server) - `TerminalServer`

- **位置**: 中央PC运行
- **职责**:
  - 连接并管理Siemens S7-1200 PLC
  - 实时轮询PLC的传感器数据（压力、温度、流量等）
  - 通过TCP服务器(端口5555)管理四个操作台的连接
  - 广播最新的传感器数据到所有连接的操作台
  - 接收并执行操作台发送的控制命令

### 2. 操作台客户端(Station Client) - `StationClient`  

- **数量**: 最多4个独立实例
- **位置**: 可部署在网络中不同的PC上
- **职责**:
  - 连接到中央Terminal
  - 接收实时传感器数据
  - 向用户显示当前系统状态
  - 处理用户操作并发送控制命令到Terminal

### 3. 网络通信协议(Network Protocol)

- **文件**: `NetworkProtocol.h/cpp`
- **传输**: 基于TCP/IP的二进制协议
- **消息类型**:
  - 站点注册/心跳/断开
  - 数据更新(T→S)
  - 控制命令(S→T)
  - 继电器/泵/阀门控制

## 📁 新增文件

```
include/
├── NetworkProtocol.h       # 网络协议定义
├── TerminalServer.h        # 终端服务器
└── StationClient.h         # 操作台客户端

src/
├── NetworkProtocol.cpp     # 网络协议实现
├── TerminalServer.cpp      # 终端服务器实现
├── StationClient.cpp       # 操作台客户端实现
└── S7PLCClient_stub.cpp    # Snap7库的stub实现

docs/
└── ARCHITECTURE_TERMINAL_STATION.md  # 详细架构文档
```

## 🔧 CMakeLists.txt 更新

- ✅ 添加 Qt6::Network 支持
- ✅ 添加新的源文件(NetworkProtocol, TerminalServer, StationClient)
- ⚠️ 临时禁用Snap7依赖(使用stub实现)

## 🔄 通信流程

```
Terminal Server                          Operation Station (Client)
┌─────────────────┐                     ┌──────────────────┐
│ TerminalServer  │←──── TCP Conn ──────→│  StationClient   │
│                 │                     │                  │
│ • PLC Driver    │                     │ • Qt GUI         │
│ • Data Poll     │                     │ • User Input     │
│ • Broadcast     │────── Sensor ──────→│ • Display Data   │
│ • Command Exec  │       Data           │                  │
│                 │←──── Control ───────│ • Send Commands  │
└─────────────────┘       Cmd            └──────────────────┘
        △
        │
        │ Snap7 (C API)
        │
    S7-1200 PLC
```

## 📊 消息格式

### 消息头 (8字节)

```
┌─────┬──────┬────────┬────────┬────────┐
│Magic│ Type │Payload │Sequence│Station │
│ 1B  │ 1B  │  2B    │  2B    │  1B    │
│ 0xA5│Type │ Length │ Number │  ID    │
└─────┴──────┴────────┴────────┴────────┘
```

### 支持的消息类型

- `STATION_REGISTER` (0x01) - 站点注册
- `STATION_HEARTBEAT` (0x02) - 保活
- `DATA_UPDATE` (0x10) - 数据更新 (Terminal→Station)
- `COMMAND_REQUEST` (0x11) - 控制请求 (Station→Terminal)
- `ERROR_MESSAGE` (0xFE) - 错误通知
- `ACK` (0xFF) - 确认

## 🚀 使用示例

### Terminal端(main.cpp改造示意)

```cpp
auto deviceManager = std::make_shared<DeviceManager>();
auto server = std::make_unique<TerminalServer>(deviceManager);
server->startServer(5555);  // 启动在5555端口

// 定期轮询和广播
while (server->isRunning()) {
    SensorData data = readFromPLC();  // 从PLC读取
    server->broadcastSensorData(data);  // 广播给所有站点
    QThread::msleep(100);
}
```

### Station端(Client应用)

```cpp
auto client = std::make_unique<StationClient>(1, "操作站点1");
client->connectToTerminal("192.168.1.100", 5555);

// 接收数据更新
connect(client.get(), &StationClient::dataUpdated, [&](const SensorData& data) {
    updateUI(data);  // 更新界面
});

// 发送控制命令
ControlCommand cmd;
cmd.command_type = 0;  // Relay
cmd.index = 1;
cmd.action = 1;        // On
client->sendCommand(cmd);
```

## ✅ 编译状态

- ✅ NetworkProtocol 编译成功
- ✅ TerminalServer 编译成功
- ✅ StationClient 编译成功
- ✅ WaterTestSystem.exe 编译成功
- ⚠️ Snap7库暂时使用stub实现

## 🔜 后续工作

1. **完整测试**
   - Terminal-Station TCP连接测试
   - 数据传输测试
   - 命令执行测试

2. **UI集成**
   - 在MainWindow中集成TerminalServer或StationClient
   - 更新GUI显示

3. **Snap7恢复**
   - 安装Snap7库后恢复完整功能
   - 替换S7PLCClient_stub.cpp

4. **部署工具**
   - 创建Station应用启动器
   - 网络配置向导

## 📈 架构优势

✅ **分布式**: 支持多个操作台同时工作  
✅ **集中管理**: 中央Terminal管理所有数据和控制  
✅ **数据一致**: 所有操作台获取相同的PLC数据  
✅ **易于扩展**: 支持快速增加操作台数量  
✅ **故障隔离**: 单个操作台故障不影响其他操作台  
✅ **远程部署**: 操作台可部署在网络中任何位置  

## 🔗 相关文档

- [详细架构设计](docs/ARCHITECTURE_TERMINAL_STATION.md)
- 网络协议规范: NetworkProtocol.h 中的详细注释
- Terminal API: TerminalServer.h
- Station API: StationClient.h

---
**编译日期**: 2026-02-03  
**版本**: 1.1.0  
**状态**: ✅ 编译成功，待测试
