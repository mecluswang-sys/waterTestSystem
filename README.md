# 水介质测试系统 - PC上位机

## 项目简介

这是一套用于水介质电磁阀测试的PC上位机软件，通过Snap7库与西门子S7-1200 PLC进行通信，实现实时监控和控制。

## 系统架构

具体的线路是：PS3--电磁阀1--PS4--电磁阀2--PS5--电动调压阀1--流量计--PS6--待测阀--PS7--电磁阀4--电动调压阀2--PS8--电磁阀5

```
PC上位机 (C++) <--> Snap7库 <--> Ethernet <--> 西门子S7-1200 PLC
                                                       |
                                                       |
                                    传感器/阀门/泵等现场设备
```

## 功能特性

- ✅ 实时监控11个压力传感器
- ✅ 监控4个流量计数据（瞬时流量、累计流量、温度）
- ✅ 控制11个电动阀（开/关、开度调节）
- ✅ 控制2个变频泵（启停、频率调节）
- ✅ 支持4条测试管路切换（DN30/DN50/DN100/DN150）
- ✅ 多种运行模式（手动/自动/测试）
- ✅ 报警管理系统
- ✅ 数据日志记录
- ✅ 紧急停止功能

## 技术栈

- **语言**: C++17
- **构建工具**: CMake 3.15+
- **PLC通信库**: Snap7
- **PLC型号**: 西门子 S7-1200
- **通信协议**: S7 Protocol (ISO-TSAP)

## 依赖库

### Snap7库安装

#### Windows

1. 下载Snap7: <https://sourceforge.net/projects/snap7/>
2. 解压到 `C:\snap7`
3. 目录结构应为:

   ```
   C:\snap7\
   ├── include\
   │   └── snap7.h
   └── lib\
       └── snap7.lib (或 snap7.dll)
   ```

#### Linux

```bash
# 下载并编译Snap7
wget https://sourceforge.net/projects/snap7/files/latest/download -O snap7.zip
unzip snap7.zip
cd snap7-full-1.4.2/build/unix
make -f x86_64_linux.mk
sudo make -f x86_64_linux.mk install
```

## 编译和运行

### Windows (使用Visual Studio)

```powershell
# 创建构建目录
mkdir build
cd build

# 生成VS项目
cmake .. -G "Visual Studio 16 2019"

# 编译
cmake --build . --config Release

# 运行
.\bin\Release\WaterTestSystem.exe
```

### Windows (使用MinGW)

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
.\bin\WaterTestSystem.exe
```

### Linux

```bash
mkdir build && cd build
cmake ..
make -j4
./bin/WaterTestSystem
```

## 配置说明

编辑 `config/system.conf` 配置文件：

```ini
# PLC连接配置
plc.ip = 192.168.0.1        # PLC的IP地址
plc.rack = 0                 # 机架号
plc.slot = 1                 # 槽号（S7-1200通常为1）

# 数据采集间隔（毫秒）
data.collection_interval = 1000

# 压力传感器（DB6 UDT数组读取）
db.sensor.number = 6
db.sensor.base_offset = 0
db.sensor.item_size = 108             # UDT数组单元素步长
db.sensor.main_value_real.offset = 46 # MainValue_Real
db.sensor.main_decimal.offset = -1    # 当前不启用小数位换算
db.pressure.scale = 1                 # 当前 PLC MainValue_Real 已是工程值

# 传感器数量
pressure.count = 3
temp.count = 3
```

## PLC程序要求

### DB块定义

程序需要在S7-1200 PLC中定义以下DB块：

1. **DB6 - 压力/温度传感器 UDT 数组（推荐）**
   - 压力读取字段：MainValue_Real(REAL)、MainDecimal(UINT)
   - 计算规则：工程值 = MainValue_Real / 10^MainDecimal，随后按 `db.pressure.scale` 转为内部Pa
   - 当前项目按 UDT 数组读取：`base_offset + (sensor_id - 1) * item_size + 46`
   - 当前现场配置：`db.sensor.number = 6`，`db.sensor.item_size = 108`

2. **DB2 - 流量计数据** (4个流量计 × 16字节 = 64字节)
   - 每个流量计: 状态(INT) + 流量(REAL) + 累计(REAL) + 温度(REAL)

3. **DB3 - 电动阀数据** (11个阀 × 20字节 = 220字节)
   - 每个阀: 阀状态(INT) + 开度(BYTE) + 设备状态(INT) + 其他

4. **DB4 - 变频泵数据** (2个泵 × 30字节 = 60字节)
   - 每个泵: 运行(BOOL) + 状态(INT) + 频率(REAL) + 电流(REAL) + 功率(REAL) + 转速(REAL)

5. **DB5 - 系统状态** (根据需要定义)
   - 系统模式(INT) + 测试管路(INT) + 运行状态(BOOL) + 其他

### TIA Portal配置

1. 打开TIA Portal，创建S7-1200项目
2. 配置PLC以太网接口IP地址（与配置文件一致）
3. 在PLC中创建上述DB块（压力传感器建议按DB6结构体方式）
4. 允许PUT/GET通信（在PLC属性中启用）

## 使用说明

### 启动流程

1. 确保PLC已上电并连接到网络
2. 检查配置文件中的IP地址是否正确
3. 运行程序
4. 程序自动连接PLC并开始数据采集

### 菜单操作

```
1. 显示所有压力传感器数据
2. 显示所有流量计数据
3. 显示所有电动阀状态
4. 控制电动阀 (开/关)
5. 控制变频泵 (启动/停止)
6. 切换测试管路
7. 设置系统模式
8. 显示活动报警
9. 紧急停止
0. 退出
```

### 数据日志

- 日志文件保存在 `logs/system.log`
- 记录所有系统事件、操作和报警
- 格式：时间戳 + 类型 + 详细信息

## 项目结构

```
1-huanjiang/
├── CMakeLists.txt          # CMake构建配置
├── README.md               # 本文件
├── config/
│   └── system.conf         # 系统配置文件
├── include/                # 头文件
│   ├── DeviceTypes.h       # 设备数据类型定义
│   ├── S7PLCClient.h       # PLC通信客户端
│   ├── DeviceManager.h     # 设备管理器
│   ├── ConfigManager.h     # 配置管理器
│   └── DataLogger.h        # 数据记录器
├── src/                    # 源文件
│   ├── main.cpp            # 主程序
│   ├── S7PLCClient.cpp
│   ├── DeviceManager.cpp
│   ├── ConfigManager.cpp
│   └── DataLogger.cpp
└── logs/                   # 日志文件（运行时创建）
```

## 网络配置

### PC网络设置

1. 将PC网络接口设置为与PLC同一网段
   - 例如: PLC IP为192.168.0.1，PC可设为192.168.0.100
   - 子网掩码: 255.255.255.0

### 防火墙设置

- Windows: 允许程序通过防火墙
- Linux: 确保端口102（S7通信端口）未被阻止

## 常见问题

### 1. 无法连接到PLC

- 检查网络连接和IP地址
- 确认PLC的PUT/GET通信已启用
- 检查防火墙设置
- 验证Snap7库是否正确安装

### 2. 编译错误：找不到snap7.h

```bash
# 设置SNAP7_ROOT环境变量
export SNAP7_ROOT=/path/to/snap7  # Linux
set SNAP7_ROOT=C:\snap7            # Windows
```

### 3. 数据读取错误

- 检查PLC DB块定义是否与程序匹配
- 验证数据类型和偏移量
- 查看日志文件了解详细错误

## 开发计划

- [ ] 添加图形界面（Qt）
- [ ] 实现趋势图显示
- [ ] 添加测试序列自动化
- [ ] 支持多PLC同时连接
- [ ] 增加数据库存储功能
- [ ] 远程访问和监控

## 安全注意事项

⚠️ **重要**:

- 本系统直接控制工业设备，使用前请充分测试
- 紧急停止功能必须经过验证
- 建议配置硬件紧急停止按钮
- 定期备份PLC程序和配置

## 许可证

MIT License

本项目本身继续采用 MIT License；如果发布包含 Qt 的 Windows 安装包，请同时确认：

- Qt 运行库采用动态链接方式随程序分发
- 安装包中包含 gpl.txt 和 lgpl.txt
- “关于”对话框中能看到 Qt 致谢和许可证提示
- 若后续引入新的 Qt 模块或静态链接方式，请重新核对对应许可义务

## 联系方式

如有问题或建议，请联系开发团队。

---

**注意**: 本程序仅供参考，实际使用需根据具体PLC程序调整DB块地址和数据结构。

# waterTestSystem
