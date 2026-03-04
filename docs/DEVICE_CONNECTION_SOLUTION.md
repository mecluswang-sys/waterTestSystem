# 物理设备连接方案总结

## 📍 问题

**现有条件**：一台PC、一台PLC，如何在新的Terminal-Station架构下组织物理连接？

## ✅ 答案

**在新架构下，一台PC就足够了！**

---

## 🏗️ 最优方案：单PC多应用模式

### 硬件配置

```
PC (192.168.33.100)
├─ Terminal Server 进程 → S7协议 → PLC (192.168.33.1)
├─ Station 1 GUI (GUI界面)
├─ Station 2 GUI (GUI界面)
├─ Station 3 GUI (GUI界面)
└─ Station 4 GUI (GUI界面)
```

### 网络配置

| 组件 | 地址 | 说明 |
|------|------|------|
| PC | 192.168.33.100 | 需配置 |
| PLC | 192.168.33.1 | 既有 |
| Terminal | localhost:5555 | 进程内 |
| Stations | localhost:5555 | 进程内 |

---

## 🔌 物理接线（3步）

### 第1步：网线连接

```
PC网卡 ←[RJ45网线]→ PLC以太网口
```

### 第2步：配置PC网卡

```
IP地址: 192.168.33.100
子网掩码: 255.255.255.0
网关: 192.168.33.1
```

### 第3步：验证连接

```
ping 192.168.33.1  ✓ Reply
```

---

## 🚀 启动方式

### 方式1：一键启动（最简单）

```bash
双击 launch_system.bat
选择 3) 启动完整系统
↓
自动启动 Terminal + 4个Stations
```

### 方式2：命令行启动

```bash
# 终端1：启动Terminal
WaterTestSystem.exe --mode terminal

# 终端2：启动Station 1
WaterTestSystem.exe --mode station --id 1

# 终端3：启动Station 2
WaterTestSystem.exe --mode station --id 2
# ... 依此类推
```

### 方式3：PowerShell脚本

```bash
PowerShell -ExecutionPolicy Bypass -File .\launch_full_system.ps1
```

---

## 📊 工作流程

```
Terminal Server (每100ms)
  ↓ 读取PLC传感器数据
PLC (S7协议 Port 102)
  ↓ 返回实时数据
Terminal Server 广播
  ↓ TCP localhost:5555
Station 1,2,3,4 (四个GUI)
  ↓ 显示数据
用户操作 (点击按钮)
  ↓ 发送控制命令
Terminal Server 执行
  ↓ 写入PLC
硬件动作 (电磁阀、泵等)
```

---

## 📈 性能

- **采集周期**: 100ms (10Hz)
- **网络延迟**: <1ms (本地TCP)
- **总延迟**: ~110ms
- **支持Station数**: 4个

---

## 🔧 关键配置文件

### config/system.conf

```properties
plc.ip = 192.168.33.1        # PLC地址
data.collection_interval = 100  # 采集周期(ms)

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

### src/main.cpp

支持两种启动模式：

- `--mode terminal` - 启动中央服务器
- `--mode station --id N` - 启动操作台N

### launch_system.bat

自动化启动脚本，支持三种模式：

1. 仅启动Terminal
2. 启动单个Station
3. 启动Terminal + 4个Stations

---

## 💡 优势

✅ **成本低** - 充分利用现有硬件，无需购置额外PC  
✅ **易维护** - 单PC的故障诊断和维修更简单  
✅ **易开发** - 本地调试效率高，无网络延迟干扰  
✅ **可扩展** - 将来需要时可轻松部署到多个PC  
✅ **可靠性** - 各Station相互独立，单个故障不影响整体  

---

## 🔄 将来多PC扩展

当硬件升级后，只需改参数，无需改代码：

```bash
# 中央PC (Terminal)
WaterTestSystem.exe --mode terminal --bind 0.0.0.0

# 远程PC (Station)  
WaterTestSystem.exe --mode station --id 1 --host 192.168.1.100
```

新的网络拓扑：

```
中央PC (Terminal) 192.168.1.100:5555
├─ 远程PC1 (Station 1) 192.168.1.101
├─ 远程PC2 (Station 2) 192.168.1.102
├─ 远程PC3 (Station 3) 192.168.1.103
└─ 远程PC4 (Station 4) 192.168.1.104
```

---

## 📋 快速部署步骤

### 第1次配置（5分钟）

1. **配置网卡**
   - Windows设置 → 以太网 → 编辑 →
   - IP: 192.168.33.100, 掩码: 255.255.255.0, 网关: 192.168.33.1

2. **验证**

   ```bash
   ping 192.168.33.1
   ```

3. **编译**

   ```bash
   cd build
   cmake --build . --config Release
   ```

4. **启动**

   ```bash
   双击 launch_system.bat → 选择 3
   ```

### 日常使用（1分钟）

```bash
双击 launch_system.bat → 选择 3 → 一键启动所有
```

---

## ✨ 总结

| 指标 | 单PC方案 | 多PC方案 |
|------|--------|--------|
| **成本** | ✅ 最低 | 需多个PC |
| **复杂度** | ✅ 最简 | 需网络管理 |
| **当前适用** | ✅ 完全满足 | 暂不需要 |
| **将来扩展** | ✅ 支持 | 改参数即可 |
| **易维护** | ✅ 最易 | 需远程管理 |

**结论**: 现有硬件配置下，使用单PC多应用模式是最优解。

---

## 📚 相关文档

- [部署指南](DEPLOYMENT_GUIDE.md) - 详细部署步骤
- [物理连接](PHYSICAL_SETUP_GUIDE.md) - 网络和硬件配置
- [架构设计](ARCHITECTURE_TERMINAL_STATION.md) - 系统架构
- [快速开始](../QUICK_START.md) - 5分钟快速配置

---

**推荐**: 现在就开始！按照快速部署步骤配置网卡，然后运行 `launch_system.bat`。
