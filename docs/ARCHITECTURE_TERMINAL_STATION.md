# 新架构设计：一个终端托四个操作台

## 架构概述

从原来的**单一PC结构**升级为**终端-操作台(Terminal-Station)模式**：

```
┌─────────────────────────────────────────────────┐
│         Terminal Server                         │
│     (中央终端/中央控制器)                        │
│  ┌─────────────────────────────────────────┐   │
│  │  • 连接 Siemens S7-1200 PLC            │   │
│  │  • 管理数据采集                         │   │
│  │  • 分发传感器数据到四个操作台          │   │
│  │  • 接收并执行来自操作台的控制命令      │   │
│  │  • TCP服务器 (端口 5555)              │   │
│  └─────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
         △           △            △          △
         │           │            │          │
        TCP连接    TCP连接      TCP连接    TCP连接
         │           │            │          │
    ┌────┴─┐    ┌────┴─┐    ┌────┴─┐    ┌──┴──┐
    │站点1 │    │站点2 │    │站点3 │    │站点4 │
    │(Station│    │(Station│    │(Station│    │(Station
    │Client)│    │Client)│    │Client)│    │Client)
    │GUI1   │    │GUI2   │    │GUI3   │    │GUI4   │
    └───────┘    └───────┘    └───────┘    └───────┘
```

## 核心组件

### 1. NetworkProtocol (网络协议层)

- **文件**: `include/NetworkProtocol.h`, `src/NetworkProtocol.cpp`
- **作用**: 定义Terminal和Station之间的通信协议
- **核心结构**:
  - `MessageType`: 8种消息类型 (注册、心跳、数据更新、控制命令等)
  - `MessageHeader`: 固定8字节头 (魔数、类型、长度、序号、站点ID)
  - `SensorData`: 传感器数据 (4个压力、4个温度、流量、时间戳)
  - `ControlCommand`: 控制命令 (继电器、泵、阀门)
  - `NetworkMessage`: 消息包装类

### 2. TerminalServer (中央终端服务器)

- **文件**: `include/TerminalServer.h`, `src/TerminalServer.cpp`
- **作用**: 运行在中央PC上，管理PLC和四个操作台
- **核心功能**:
  - 启动TCP服务器 (`startServer(port)`)
  - 管理四个Station连接
  - 轮询PLC传感器数据
  - 广播数据到所有连接的操作台 (`broadcastSensorData()`)
  - 接收并执行操作台的控制命令
  - 心跳管理和连接检测

### 3. StationClient (操作台客户端)

- **文件**: `include/StationClient.h`, `src/StationClient.cpp`
- **作用**: 运行在每个操作台PC上，连接到Terminal
- **核心功能**:
  - 连接到Terminal服务器 (`connectToTerminal()`)
  - 注册站点信息
  - 接收实时传感器数据 (信号: `dataUpdated`)
  - 发送控制命令到Terminal (`sendCommand()`)
  - 自动心跳保活

## 通信协议详解

### 消息类型 (MessageType)

| 类型 | 值 | 方向 | 说明 |
|------|----|----|------|
| STATION_REGISTER | 0x01 | S→T | 站点向终端注册 |
| STATION_HEARTBEAT | 0x02 | S→T | 心跳保活 |
| STATION_DISCONNECT | 0x03 | S→T | 站点断开连接 |
| DATA_UPDATE | 0x10 | T→S | 终端向站点发送数据更新 |
| COMMAND_REQUEST | 0x11 | S→T | 站点请求执行命令 |
| COMMAND_RESPONSE | 0x12 | T→S | 终端返回命令执行结果 |
| RELAY_CONTROL | 0x20 | T→S | 控制继电器 |
| PUMP_CONTROL | 0x21 | T→S | 控制泵 |
| VALVE_CONTROL | 0x22 | T→S | 控制阀门 |
| START_TEST | 0x30 | T→S | 启动测试 |
| STOP_TEST | 0x31 | T→S | 停止测试 |
| EMERGENCY_STOP | 0x32 | T→S | 紧急停止 |
| ERROR_MESSAGE | 0xFE | T→S | 错误通知 |
| ACK | 0xFF | 双向 | 确认/应答 |

### 消息格式

```
[Header (8字节)]      [Payload (可变长)]
┌─────────────────┬──────────────────┐
│ Magic │Type│Len│Seq│StID│ Payload  │
│ 1B    │1B  │2B │2B │1B  │ N Bytes  │
└─────────────────┴──────────────────┘
  0xA5  MessageType PayloadLen Sequence StationID
```

## 使用示例

### Terminal侧启动 (main.cpp 改造)

```cpp
#include "TerminalServer.h"
#include "DeviceManager.h"

auto deviceManager = std::make_shared<DeviceManager>();
auto server = std::make_unique<TerminalServer>(deviceManager);
server->startServer(5555);

// 定期轮询PLC数据并广播
while (true) {
    SensorData data = readFromPLC();
    server->broadcastSensorData(data);
    sleep(100);  // 100ms轮询周期
}
```

### Station侧启动 (operation_station.cpp)

```cpp
#include "StationClient.h"
#include "gui/StationMainWindow.h"

auto client = std::make_unique<StationClient>(1, "操作站点1");
client->connectToTerminal("192.168.1.100", 5555);  // 连接到Terminal

// UI获取数据
connect(client.get(), &StationClient::dataUpdated, [&](const SensorData& data) {
    updateUI(data);  // 更新GUI显示
});

// 用户操作时发送命令
ControlCommand cmd;
cmd.command_type = 0;  // Relay
cmd.index = 1;
cmd.action = 1;  // On
client->sendCommand(cmd);
```

## 项目文件变动

### 新增文件

- `include/NetworkProtocol.h` - 网络协议定义
- `include/TerminalServer.h` - 终端服务器
- `include/StationClient.h` - 操作台客户端
- `src/NetworkProtocol.cpp` - 网络协议实现
- `src/TerminalServer.cpp` - 终端服务器实现
- `src/StationClient.cpp` - 操作台客户端实现

### 需要改造的文件

- `CMakeLists.txt` - 添加新源文件
- `src/main.cpp` - 改为Terminal服务器启动
- `src/gui/MainWindow.cpp` - 改为Station客户端连接
- `src/gui/TestPanel.cpp` - 接收Terminal数据而不是直接连接PLC

## 部署架构

```
Central PC (终端)          Operation PC 1-4 (操作台)
┌──────────────┐          ┌──────────────┐
│ Terminal App │◄──────────┤ Station App1 │
│ (Server)     │          └──────────────┘
│ :5555        │          ┌──────────────┐
│              │◄──────────┤ Station App2 │
│  PLC Driver  │          └──────────────┘
│  Data Poll   │          ┌──────────────┐
│  Broadcast   │◄──────────┤ Station App3 │
└──────────────┘          └──────────────┘
       △                  ┌──────────────┐
       │                  │ Station App4 │
       └──────────────────┤              │
         S7-1200 PLC      └──────────────┘
```

## 下一步实现计划

1. **修改CMakeLists.txt** - 添加新的源文件编译
2. **改造main.cpp** - 创建Terminal服务器启动逻辑
3. **创建operation_station.cpp** - 操作台应用主程序
4. **改造MainWindow.cpp** - 接入StationClient而非DeviceManager直连
5. **改造TestPanel.cpp** - 数据来自StationClient的dataUpdated信号
6. **完善通信机制** - 完整测试Terminal-Station通信

## 优势

✅ **分布式架构**: 支持多个操作台同时工作  
✅ **集中管理**: 中央Terminal管理所有数据和控制  
✅ **数据一致**: 所有操作台获取相同的PLC数据  
✅ **易于扩展**: 支持增加更多操作台  
✅ **故障隔离**: 单个操作台故障不影响其他操作台  
✅ **远程部署**: 操作台可部署在网络中任何位置  
