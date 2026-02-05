# 快速参考卡片 - Terminal-Station 架构

## 🎯 一句话总结

**一台PC + 一台PLC → 可以运行1个Terminal + 4个Station的完整系统**

---

## 🔧 快速配置（5分钟）

### 1️⃣ 配置PC网卡IP

```
IP: 192.168.33.100
掩码: 255.255.255.0
网关: 192.168.33.1 (PLC)
```

### 2️⃣ 验证连接

```bash
ping 192.168.33.1
# 预期: Reply from 192.168.33.1
```

### 3️⃣ 编译

```bash
cd build
cmake --build . --config Release
```

### 4️⃣ 启动

```bash
# 双击运行此脚本
launch_system.bat

# 或者用命令行
WaterTestSystem.exe --mode terminal
```

---

## 📊 系统架构（一图看懂）

```
┌─────────────────────────────────────┐
│  单台PC (192.168.33.100)            │
│                                     │
│  Terminal Server (主进程)           │  ← 连接PLC，采集数据，广播
│        ↓ TCP localhost:5555         │
│  ┌─────┬────────┬────────┐          │
│  │     │        │        │          │
│  GUI1 GUI2 GUI3 GUI4     │          │  ← 4个操作界面
└──────────────────────────────────────┘
        │ 网线
        ↓
    S7-1200 PLC
```

---

## 🚀 启动命令速查

| 场景 | 命令 |
|------|------|
| **启动Terminal** | `WaterTestSystem.exe --mode terminal` |
| **启动Station 1** | `WaterTestSystem.exe --mode station --id 1` |
| **启动Station 2** | `WaterTestSystem.exe --mode station --id 2` |
| **启动Station 3** | `WaterTestSystem.exe --mode station --id 3` |
| **启动Station 4** | `WaterTestSystem.exe --mode station --id 4` |
| **查看帮助** | `WaterTestSystem.exe --help` |

### 🎬 一键启动所有 (推荐)

```bash
launch_system.bat
# 选择 3) 启动完整系统
```

---

## 📡 数据流

```
Terminal Server
  ↓ 每100ms
读取PLC数据 (S7协议)
  ↓
广播给Stations
  ↓
Station 1,2,3,4 显示
  ↓
用户操作
  ↓
发送控制命令
  ↓
Terminal执行
  ↓
写入PLC
  ↓
硬件动作 (电磁阀/泵/等)
```

---

## 🔍 故障自检

| 问题 | 检查 |
|------|------|
| **无法连接PLC** | `ping 192.168.33.1` |
| **Station连不上Terminal** | `netstat -an \| find ":5555"` |
| **数据不更新** | 检查Terminal是否运行 |
| **控制命令无效** | 检查PLC是否在线 |

---

## 📈 性能指标

- **采集频率**: 10Hz (100ms)
- **延迟**: ~110ms
- **支持Station数**: 4个
- **最大吞吐**: 40条消息/秒

---

## 💾 重要文件位置

| 文件 | 位置 | 说明 |
|------|------|------|
| 可执行文件 | `build/bin/Release/WaterTestSystem.exe` | 主程序 |
| 启动脚本 | `launch_system.bat` | 一键启动 |
| 配置文件 | `config/system.conf` | PLC参数 |
| 源代码 | `src/*.cpp` | C++实现 |
| 头文件 | `include/*.h` | 接口定义 |
| 文档 | `docs/*.md` | 详细说明 |

---

## 🎓 关键概念

| 名词 | 说明 |
|------|------|
| **Terminal** | 中央服务器，连接PLC，管理所有Station |
| **Station** | 操作台客户端，显示GUI，接收用户输入 |
| **localhost** | 本机，进程间通信用127.0.0.1 |
| **:5555** | Terminal监听的TCP端口 |
| **SensorData** | 包含压力、温度、流量等的数据结构 |
| **ControlCommand** | 包含电磁阀、泵等的控制命令 |

---

## 🔗 将来扩展

**当要在远程PC上运行Station时：**

中央PC:

```bash
WaterTestSystem.exe --mode terminal --bind 0.0.0.0
# ^^^^^^^^^^^^^^^^^^^^
# 改这里绑定到所有网卡
```

远程PC:

```bash
WaterTestSystem.exe --mode station --id 1 --host 192.168.1.100
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
# 改这里指向中央PC的IP
```

---

## ❓ 常见问题

**Q: 现有只有1台PC，能运行所有系统吗？**  
A: 可以！Terminal和4个Station都在1台PC上，通过localhost:5555通信。

**Q: 启动顺序重要吗？**  
A: 很重要！必须先启动Terminal，再启动Stations。

**Q: Station之间能通信吗？**  
A: 不能。所有通信都通过Terminal中转。

**Q: 能运行超过4个Stations吗？**  
A: 可以（代码允许），但架构设计为4个。

**Q: 单个Station崩溃会影响其他吗？**  
A: 不会。Terminal继续运行，其他Stations继续工作。

---

## 🛠️ 自定义参数

编辑 `src/TerminalServer.cpp` 第80行调整采集频率：

```cpp
// 当前配置（100ms）
m_pollTimer->start(100);

// 改为:
m_pollTimer->start(50);   // 20Hz - 更快
m_pollTimer->start(200);  // 5Hz - 更慢
```

编辑 `include/NetworkProtocol.h` 调整其他参数。

---

## ✅ 部署检查清单

- [ ] PC网卡IP: 192.168.33.100
- [ ] PLC IP: 192.168.33.1
- [ ] ping 192.168.33.1 成功
- [ ] config/system.conf 正确
- [ ] 编译成功
- [ ] Terminal启动无错误
- [ ] Stations连接成功
- [ ] GUI显示更新数据
- [ ] 控制命令有效

---

## 📞 技术支持命令

```bash
# 查看所有进程
tasklist | find "WaterTestSystem"

# 查看5555端口
netstat -an | find ":5555"

# 查看网络接口
ipconfig

# 杀死所有WaterTestSystem
taskkill /IM WaterTestSystem.exe /F

# 查看详细帮助
WaterTestSystem.exe -h
```

---

## 🎯 下一步

1. ✅ **立即配置网卡** - 设置PC IP为192.168.33.100
2. ✅ **验证连接** - ping 192.168.33.1
3. ✅ **编译项目** - cmake --build build --config Release
4. ✅ **启动系统** - 运行 launch_system.bat
5. 🔧 **测试功能** - 验证数据流和控制命令
6. 📝 **查看详细文档** - docs/DEPLOYMENT_GUIDE.md

---

**建议第一次使用按照这个顺序操作！**
