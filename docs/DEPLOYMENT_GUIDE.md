# 现有硬件配置下的部署指南

## 📋 现有条件

- ✅ 1台PC（主控机）
- ✅ 1台PLC（Siemens S7-1200）  
- ✅ 网络基础设施（网线、交换机等）

---

## 🎯 核心概念

在新的 **Terminal-Station 架构** 下，物理上的一台PC可以同时运行：

1. **Terminal Server** - 中央服务器进程
   - 直接连接PLC
   - 轮询PLC数据(每100ms)
   - 通过TCP将数据广播给所有Stations
   - 执行来自Stations的控制命令

2. **Station Clients** - 操作面板进程（1-4个）
   - 连接到Terminal Server (localhost:5555)
   - 接收传感器数据并显示在GUI上
   - 接收用户输入并发送控制命令

### 优势

- ✅ **单PC即可运行完整系统** - 无需多台电脑
- ✅ **低成本** - 充分利用现有硬件
- ✅ **易于扩展** - 将来可以在不改代码的情况下部署到多个PC

---

## 🔌 物理接线

### 网络拓扑

```
┌────────────────────────────────────────────┐
│  Main PC (192.168.33.100)                  │
│                                            │
│  ┌──────────────────────────────────────┐  │
│  │ Terminal Server (localhost:5555)     │  │
│  │ • PLC Driver + Data Polling         │  │
│  │ • TCP Server for Stations           │  │
│  └────────────────┬─────────────────────┘  │
│                   │                        │
│  ┌────────────────┴────────────────────┐   │
│  │                                     │   │
│  ▼                                     ▼   │
│ ┌────────────┐  ┌────────────┐ ... 3&4    │
│ │ Station 1  │  │ Station 2  │            │
│ │ (GUI)      │  │ (GUI)      │            │
│ └────────────┘  └────────────┘            │
└────────────────────────────────────────────┘
        │
        │ Ethernet (RJ45网线)
        │ 192.168.33.100 ←→ 192.168.33.1
        │
    ┌───▼────────────────────────┐
    │ Siemens S7-1200 PLC        │
    │ IP: 192.168.33.1           │
    │ Port: 102 (S7 Protocol)    │
    │                            │
    │ • Pressure Sensors (4)     │
    │ • Temperature Sensors (4)  │
    │ • Flow Meter (1)           │
    │ • Control Valves (relays)  │
    │ • Pump Control             │
    └────────────────────────────┘
```

### 网络配置

| 组件 | IP地址 | 子网掩码 | 说明 |
|------|--------|--------|------|
| PLC | 192.168.33.1 | 255.255.255.0 | 已配置 |
| PC | 192.168.33.100 | 255.255.255.0 | **需配置** |
| Terminal Server | localhost:5555 | - | 进程内通信 |
| Stations | localhost:5555 (client) | - | 进程内通信 |

---

## ⚙️ 配置步骤

### 第1步：配置PC网卡IP地址

**Windows 11/10:**

1. 右键点击 **任务栏网络图标** → **网络和Internet设置**
2. 点击 **以太网**
3. 点击 **编辑** (IP设置)
4. 选择 **手动**，打开 **IPv4** 切换
5. 填入：

   ```
   IP地址: 192.168.33.100
   子网掩码: 255.255.255.0
   网关: 192.168.33.1
   DNS: 8.8.8.8
   ```

6. 点击 **保存**

**验证配置:**

```powershell
# 打开PowerShell，运行：
ping 192.168.33.1

# 预期输出（成功）：
# Pinging 192.168.33.1 with 32 bytes of data:
# Reply from 192.168.33.1: bytes=32 time=1ms TTL=64
# Reply from 192.168.33.1: bytes=32 time=1ms TTL=64
```

### 第2步：更新config文件

编辑 `config/system.conf`:

```properties
# PLC连接配置 - 确认这些值
plc.ip = 192.168.33.1
plc.rack = 0
plc.slot = 1

# 数据采集周期（毫秒）
# 100 = 10Hz轮询
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

# 其他配置保持不变
...
```

### 第3步：编译项目

```bash
cd c:\Users\mickey\workspace\4_cpp\1-huanjiang
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

输出：`build\bin\Release\WaterTestSystem.exe`

---

## 🚀 启动系统

### 方式1：使用启动脚本（推荐）

```bash
# 双击运行
launch_system.bat
```

脚本会提示你选择：

1. **启动Terminal Server** - 只运行中央服务器
2. **启动Station Client** - 运行单个操作台
3. **启动完整系统** - 启动Terminal + 4个Stations

### 方式2：命令行启动

**启动Terminal Server:**

```bash
cd build\bin\Release
WaterTestSystem.exe --mode terminal
```

**启动Station Client #1:**

```bash
WaterTestSystem.exe --mode station --id 1 --name "操作台1"
```

**启动所有Station Clients:**

```bash
for /L %i in (1,1,4) do (
    start "" WaterTestSystem.exe --mode station --id %i
)
```

### 方式3：PowerShell脚本

创建 `launch_full_system.ps1`:

```powershell
$exe = ".\build\bin\Release\WaterTestSystem.exe"

# 启动Terminal
Write-Host "Starting Terminal Server..."
Start-Process -NoNewWindow -FilePath $exe -ArgumentList "--mode terminal"
Start-Sleep -Seconds 2

# 启动4个Stations
for ($i = 1; $i -le 4; $i++) {
    Write-Host "Starting Station $i..."
    Start-Process -FilePath $exe -ArgumentList "--mode station --id $i --name `"操作台$i`""
    Start-Sleep -Milliseconds 500
}

Write-Host "All applications started!"
```

运行：

```powershell
PowerShell -ExecutionPolicy Bypass -File .\launch_full_system.ps1
```

---

## 📊 工作流程验证

### 启动后应该看到

**Terminal Server 窗口:**

```
Terminal Server started successfully on port 5555
Waiting for Station connections...
Station connected: 操作台1 (ID: 1)
Station connected: 操作台2 (ID: 2)
Station connected: 操作台3 (ID: 3)
Station connected: 操作台4 (ID: 4)
```

**每个Station 窗口:**

```
Connected to Terminal Server
Receiving sensor data...
Pressure P1: 1023 kPa
Pressure P2: 1024 kPa
Temperature T1: 25.5°C
...
```

### 数据流动验证

1. **Terminal Server** 读取PLC → 获得传感器数据
2. **Terminal Server** 广播数据 → 所有Stations接收
3. **每个Station** 更新GUI显示
4. **用户操作** (点击按钮) → 发送命令给Terminal
5. **Terminal** 执行命令 → 写入PLC → 硬件动作

---

## 🔧 故障排查

### 问题1: "无法连接PLC"

**症状：** Terminal启动但立即报错连接失败

**检查清单：**

```powershell
# 1. 验证网络连接
ping 192.168.33.1
# 预期: Reply (成功)

# 2. 检查网卡配置
ipconfig
# 查看以太网适配器 IP是否为 192.168.33.x

# 3. 验证PLC是否在线
# - 检查PLC面板指示灯
# - 检查网线连接
```

**解决方案：**

- [ ] 确认PC网卡IP在192.168.33.x网段
- [ ] 检查网线是否牢固连接
- [ ] 重启网络适配器

### 问题2: "Station无法连接Terminal"

**症状：** Station启动后显示"连接失败"

**原因分析：**

1. Terminal未启动
2. localhost:5555被占用
3. 防火墙阻止

**解决方案：**

```powershell
# 检查Terminal是否运行
tasklist | grep WaterTestSystem

# 检查5555端口是否被监听
netstat -an | findstr :5555

# 重启Terminal
taskkill /IM WaterTestSystem.exe /F
# 再启动Terminal
```

### 问题3: 数据更新缓慢或卡顿

**可能原因：**

- PLC响应缓慢
- 网络状况不佳
- 采集周期设置过短

**调整方案：**

编辑 `src/TerminalServer.cpp` 第80行：

```cpp
m_pollTimer->start(100);  // 当前: 100ms

// 改为:
m_pollTimer->start(200);  // 200ms = 5Hz (更慢但更稳定)
// 或
m_pollTimer->start(50);   // 50ms = 20Hz (更快但需要更强的网络)
```

---

## 📈 性能指标（单PC配置）

| 指标 | 值 | 说明 |
|------|-----|------|
| **采集频率** | 10Hz | 100ms间隔 |
| **网络延迟** | <1ms | 本地TCP |
| **总系统延迟** | ~110ms | 采集+网络+处理 |
| **数据吞吐量** | 4×10Hz | 无压力 |
| **支持Station数** | 4个 | 架构限制 |

---

## 🔄 将来扩展到多PC

当需要操作台在远程PC上运行时，**无需改代码**，只需改参数：

**中央PC (Terminal Server):**

```bash
# 绑定到所有网卡 (0.0.0.0 表示任何网卡)
WaterTestSystem.exe --mode terminal --bind 0.0.0.0 --port 5555
```

**远程PC (Station Client):**

```bash
# 连接到中央PC的IP
WaterTestSystem.exe --mode station --id 1 --host 192.168.1.100 --port 5555
# 改这里 ^^^^^^^^^^^^^
```

**网络拓扑演变：**

```
现在 (单PC):
┌─ Terminal
├─ Station 1
├─ Station 2
├─ Station 3
└─ Station 4

将来 (多PC):
Central PC 192.168.1.100 ─ Terminal
       ├─ PC1 192.168.1.101 ─ Station 1
       ├─ PC2 192.168.1.102 ─ Station 2
       ├─ PC3 192.168.1.103 ─ Station 3
       └─ PC4 192.168.1.104 ─ Station 4
```

---

## 📋 启动检查清单

**部署前：**

- [ ] PC网卡IP配置为 192.168.33.100
- [ ] 网线连接PC和PLC
- [ ] `ping 192.168.33.1` 成功
- [ ] `config/system.conf` 配置正确
- [ ] 编译成功，生成 `WaterTestSystem.exe`

**启动时：**

- [ ] 先启动 Terminal Server
- [ ] 等待2秒后启动 Stations
- [ ] 观察连接日志

**验证：**

- [ ] Terminal显示"Waiting for Station connections"
- [ ] 每个Station显示"Connected successfully"
- [ ] GUI显示实时更新的传感器数据
- [ ] 控制命令能正常执行

---

## 💡 最佳实践

1. **始终先启动Terminal再启动Stations**
   - 否则Stations会连接失败

2. **使用启动脚本而不是手工启动**
   - 避免遗漏或启动顺序错误

3. **在生产环境使用Windows服务**
   - 自动重启
   - 开机自启

4. **定期监控PLC连接状态**
   - Terminal日志会显示连接问题
   - 及时处理PLC离线情况

5. **为Stations添加重连机制**
   - 当Terminal重启时能自动重新连接

---

## 📞 常用命令参考

```bash
# 查看所有可用选项
WaterTestSystem.exe --help

# 启动Terminal，监听所有网卡
WaterTestSystem.exe --mode terminal --bind 0.0.0.0

# 启动Station，连接到指定主机
WaterTestSystem.exe --mode station --id 1 --host 192.168.1.100 --port 5555

# 启动Station，使用自定义名称
WaterTestSystem.exe --mode station --id 2 --name "Pressure Test Panel"

# 后台运行（Windows）
start "" WaterTestSystem.exe --mode station --id 1
```

---

**当前配置推荐：** 使用 `launch_system.bat` 脚本，选择 "3) 启动完整系统"

这样可以一键启动 Terminal Server + 4个 Stations！
