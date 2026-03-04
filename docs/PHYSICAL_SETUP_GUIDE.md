# 物理设备连接方案（Terminal-Station架构）

## 📋 现有硬件

- ✓ 1台PC（主控机）
- ✓ 1台PLC（Siemens S7-1200）
- ✓ 网络设备（网线、交换机等）

## 🏗️ 物理连接方案

### 方案1：单PC多应用模式（推荐 - 最简单）

```
┌─────────────────────────────────────┐
│        Main PC (192.168.1.100)      │
│                                     │
│  ┌──────────────────────────────┐  │
│  │ Terminal Server Process      │  │
│  │ • PLC 驱动                   │  │
│  │ • 数据采集 (100ms周期)       │  │
│  │ • TCP Server :5555           │  │
│  │ • 广播数据给Stations        │  │
│  └────────┬─────────────────────┘  │
│           │ TCP LocalHost:5555      │
│  ┌────────┴────┬────────┬────────┐  │
│  │             │        │        │  │
│  ▼             ▼        ▼        ▼  │
│ ┌─────┐ ┌────┐ ┌────┐ ┌────┐      │
│ │GUI1 │ │GUI2│ │GUI3│ │GUI4│      │
│ │操作台1│ │操作台2│ │操作台3│ │操作台4│   │
│ │Qt窗口│ │Qt窗口│ │Qt窗口│ │Qt窗口│   │
│ └─────┘ └────┘ └────┘ └────┘      │
└─────────────────────────────────────┘
        │ Ethernet (网线)
        │ S7协议 (Port 102)
        │
    ┌───▼──────────────┐
    │  S7-1200 PLC     │
    │ 192.168.33.1     │
    │                  │
    │ • 压力传感器信号  │
    │ • 温度传感器信号  │
    │ • 流量计信号      │
    │ • 电磁阀状态      │
    └──────────────────┘
```

### 设备连接清单

#### 1. PC → PLC 网络连接

```
PC网卡              →  网线(RJ45)  →  PLC以太网口
192.168.1.100              同一网段        192.168.33.1
(需配置IP在33段)                        (根据config)
```

#### 2. PC 内部进程通信

```
Terminal Server          Station Clients
(主进程)                 (子进程/线程)
:5555                    localhost:5555
(TCP SERVER)             (TCP CLIENTS)
↓
接收PLC数据
↓
广播给所有Stations
```

#### 3. 传感器/执行器 → PLC (既有)

```
压力传感器(4个)  →  PLC 模拟输入
温度传感器(4个)  →  PLC 模拟输入
流量计          →  PLC 数字/模拟输入
电磁阀          →  PLC 数字输出
泵              →  PLC 数字输出
```

## 🔧 网络配置步骤

### Step 1: 配置PC网卡IP地址

**Windows中：**

```powershell
# 设置网卡IP为192.168.33.xxx（与PLC同网段）
# 例如：192.168.33.100

# 1. 打开网络设置
# 2. 选择以太网适配器
# 3. 点击 "编辑"
# 4. IPv4设置：
#    - IP地址：192.168.33.100
#    - 子网掩码：255.255.255.0
#    - 网关：192.168.33.1（PLC地址）
# 5. 应用保存
```

### Step 2: 验证PC-PLC连接

```powershell
# 测试网络连接
ping 192.168.33.1

# 预期输出：
# Pinging 192.168.33.1 with 32 bytes of data:
# Reply from 192.168.33.1: bytes=32 time=1ms TTL=64
```

### Step 3: 更新config/system.conf

```properties
# 确认PLC连接参数
plc.ip = 192.168.33.1
plc.rack = 0
plc.slot = 1

# 数据采集间隔（100ms = 10Hz）
data.collection_interval = 100

# 压力读取（DB6结构体）
db.sensor.number = 6
db.sensor.base_offset = 0
db.sensor.item_size = 0
db.sensor.main_value_real.offset = 4
db.sensor.main_decimal.offset = 20
db.pressure.scale = 1000

# 单传感器调试
pressure.count = 1
temp.count = 1
```

## 💻 应用部署方案

### 方案A：单一可执行文件 + 命令行参数

```bash
# Terminal模式（启动服务器）
WaterTestSystem.exe --mode terminal

# Station模式（启动客户端，ID 1-4）
WaterTestSystem.exe --mode station --id 1 --name "操作台1"
WaterTestSystem.exe --mode station --id 2 --name "操作台2"
WaterTestSystem.exe --mode station --id 3 --name "操作台3"
WaterTestSystem.exe --mode station --id 4 --name "操作台4"
```

### 方案B：多进程启动脚本

**launch_system.bat** (启动所有应用)

```batch
@echo off
REM 启动Terminal Server
start "Terminal Server" WaterTestSystem.exe --mode terminal

REM 等待Terminal启动
timeout /t 2

REM 启动4个Station Clients
start "Station 1" WaterTestSystem.exe --mode station --id 1
start "Station 2" WaterTestSystem.exe --mode station --id 2
start "Station 3" WaterTestSystem.exe --mode station --id 3
start "Station 4" WaterTestSystem.exe --mode station --id 4

echo All applications started!
```

## 📊 Terminal Server 工作流

```
Timer (100ms)
    ↓
[Terminal Server]
    ↓
1. 读取PLC数据
   └→ S7协议 (102端口) → PLC
   └→ 读取DB块数据
   └→ 获取: 压力、温度、流量、阀门状态
    ↓
2. 将数据组织为SensorData结构体
    ↓
3. 广播到所有connected Stations
   ├→ Station 1 (localhost:5555)
   ├→ Station 2 (localhost:5555)
   ├→ Station 3 (localhost:5555)
   └→ Station 4 (localhost:5555)
    ↓
4. 接收来自Stations的控制命令
   ├→ 电磁阀控制
   ├→ 泵频率调节
   ├→ 阀门开度控制
    ↓
5. 执行命令 → 写入PLC → 硬件动作
```

## 🔗 网络端口分配

| 组件 | 地址 | 端口 | 说明 |
|------|------|------|------|
| PLC | 192.168.33.1 | 102 | S7通信 |
| Terminal Server | localhost | 5555 | TCP服务器 |
| Station 1 | localhost | (客户端) | 连接到:5555 |
| Station 2 | localhost | (客户端) | 连接到:5555 |
| Station 3 | localhost | (客户端) | 连接到:5555 |
| Station 4 | localhost | (客户端) | 连接到:5555 |

## 🚀 将来扩展方案

当需要多台PC时的方案：

```
┌─────────────────┐
│  Central PC     │
│ Terminal Server │ 192.168.1.100
│ :5555           │
└────────┬────────┘
         │ Ethernet
    ┌────┼────┬──────┬──────┐
    │    │    │      │      │
┌───▼┐┌──▼──┐┌──────┐┌────┐
│Op1 ││Op2  ││Op3   ││Op4 │
│PC1 ││PC2  ││PC3   ││PC4 │
│    ││     ││      ││    │
└────┘└─────┘└──────┘└────┘
(192.168.1.101-104)
```

**优势**：

- 无需改代码，只需改连接参数
- Terminal Server IP改为 Central PC IP
- Station Clients 连接到该IP:5555

## ⚡ 性能指标

**当前配置(单PC)：**

- 数据采集周期: 100ms (10Hz)
- 网络延迟: <1ms (本地TCP)
- 总系统延迟: ~100-110ms
- 吞吐量: 4个Station × 100Hz = 无压力

**关键参数可调**：

```cpp
// TerminalServer.cpp
m_pollTimer->start(100);  // 改这里调整采集频率
                          // 最小建议50ms (20Hz)
                          // 最大可100ms或更长
```

## 📝 启动检查清单

- [ ] PC网卡IP配置为192.168.33.xxx
- [ ] 网线连接PC和PLC
- [ ] 验证ping 192.168.33.1成功
- [ ] config/system.conf配置正确
- [ ] 编译WaterTestSystem.exe
- [ ] 启动Terminal Server
- [ ] 启动4个Station Clients
- [ ] 验证数据流动
- [ ] 测试控制命令

## 🔍 故障排查

### 问题1: 无法连接PLC

```
原因：网络配置错误或PLC地址错误
解决：
1. ping 192.168.33.1
2. 检查config中的plc.ip配置
3. 检查网线是否连接
4. 检查PC防火墙是否阻止
```

### 问题2: Station无法连接Terminal

```
原因：Terminal未启动或监听失败
解决：
1. 确认Terminal Server process正在运行
2. netstat -an | grep 5555 验证端口监听
3. 检查防火墙规则
```

### 问题3: 数据更新缓慢

```
原因：PLC响应慢或采集间隔过长
解决：
1. 检查PLC负载
2. 减小data.collection_interval
3. 检查网络状况
```

---

**推荐方案**: 方案A（单PC多应用），操作简单，无需额外硬件，完全利用现有条件。
