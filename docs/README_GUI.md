# Water Test System - GUI版本说明

## 项目概述

水介质电磁阀测试系统 - PC上位机，带Qt图形界面

## 系统组成

### 硬件

- PLC: 西门子 S7-1200
- 压力传感器: 11个
- 流量计: 4个
- 电磁阀: 11个
- 变频泵: 2个
- 测试管路: DN30, DN50, DN100, DN150

### 软件架构

- 核心通信: Snap7库
- GUI框架: Qt6 (Widgets + Charts)
- 编程语言: C++17
- 构建工具: CMake 3.15+
- 包管理: vcpkg

## GUI界面

### 主窗口功能

1. **监控面板** (MonitorPanel)
   - 实时显示压力传感器数据（表格+趋势图）
   - 实时显示流量计数据（表格+趋势图）
   - 实时显示阀门状态
   - 实时显示泵状态

2. **控制面板** (ControlPanel)
   - 系统模式选择（手动/自动/测试）
   - 测试管路选择（DN30/50/100/150）
   - 阀门控制（开/关）
   - 泵控制（启动/停止/频率设置）
   - 紧急停止按钮

3. **报警面板** (AlarmPanel)
   - 报警列表显示
   - 报警级别过滤
   - 报警确认
   - 报警历史

### 菜单栏

- 文件: 退出
- PLC: 连接/断开, 查看状态
- 数据: 开始/停止采集
- 帮助: 关于

## 编译步骤

### 1. 安装依赖

```powershell
cd c:\Users\mickey\workspace\4_cpp\1-huanjiang
vcpkg install
```

### 2. 配置CMake

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

### 3. 编译

```powershell
cmake --build build --config Release
```

### 4. 运行

```powershell
.\build\bin\Release\WaterTestSystem.exe
```

## 配置文件

配置文件位于 `config/system.conf`:

```ini
# PLC连接
plc.ip = 192.168.33.1
plc.rack = 0
plc.slot = 1

# 采集周期（毫秒）
data.collection_interval = 100

# 压力读取（DB6结构体）
db.sensor.number = 6
db.sensor.base_offset = 0
db.sensor.item_size = 0
db.sensor.main_value_real.offset = 4
db.sensor.main_decimal.offset = 20
db.pressure.scale = 1000

pressure.count = 1
temp.count = 1
```

## 使用说明

### 首次运行

1. 启动程序
2. 点击 PLC → 连接
3. 连接成功后，点击 数据 → 开始采集
4. 在监控面板查看实时数据
5. 使用控制面板操作设备

### 紧急情况

点击控制面板的红色 "EMERGENCY STOP" 按钮将立即关闭所有阀门和泵。

## 技术细节

### PLC通信

- DB6: 压力传感器结构体（读取 MainValue_Real + MainDecimal）
- DB2: 流量计数据（4个流量计，每个12字节）
- DB3: 阀门控制（11个阀门，每个3字节）
- DB4: 泵控制（2个泵，每个9字节）
- DB5: 系统状态

### 数据采集

- 默认采集间隔: 1000ms
- 趋势图最大数据点: 100
- 数据日志格式: CSV
- 报警日志格式: 文本

## 故障排查

### PLC连接失败

1. 检查IP地址配置
2. 检查网络连接
3. 检查PLC是否在运行
4. 确认Snap7库已正确安装

### 编译错误

1. 确认Qt6已通过vcpkg安装
2. 确认CMake版本≥3.15
3. 确认MSVC编译器已安装
4. 检查vcpkg toolchain路径

## 版本信息

- 版本: 1.0.0
- 日期: 2025-12-18
- 编译器: MSVC 19.44.35215.0
- Qt版本: 6.8.3
- Snap7版本: 1.4.2
