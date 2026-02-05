# Terminal主控台技术文档

## 概览

Terminal主控台是一个专为Terminal Server设计的管理中心，负责：

- PLC连接管理和数据采集
- 4个Station客户端的连接管理
- 实时数据展示和历史数据查询
- 系统日志和告警
- 配置管理和远程诊断

## 架构

```
┌─────────────────────────────────────────────────────────┐
│                   MainWindow                            │
│  (支持Terminal/Station双模式切换)                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Terminal Mode ◄──── setWindowMode()                   │
│       ├─ DashboardWidget                               │
│       ├─ PLCConnectionWidget                           │
│       ├─ StationManagerWidget                          │
│       ├─ DataMonitorWidget                             │
│       ├─ SystemLogsWidget                              │
│       └─ SettingsWidget                                │
│                                                         │
│  Station Mode ◄──── setWindowMode()                    │
│       ├─ PreparationPanel                              │
│       ├─ MonitorPanel                                  │
│       ├─ TestPanel                                     │
│       └─ AutoTestPanel                                 │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│        TerminalServer                StationClient      │
│        (Main Process)              (Remote Client)      │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 类设计

### MainWindow 类

**位置**：`include/gui/MainWindow.h` / `src/gui/MainWindow.cpp`

**主要成员变量**：

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
    
private:
    // 模式管理
    WindowMode m_mode;                          // STATION_MODE / TERMINAL_MODE
    
    // 服务器/客户端实例
    std::shared_ptr<TerminalServer> m_terminalServer;
    std::shared_ptr<StationClient> m_stationClient;
    std::shared_ptr<DeviceManager> m_deviceManager;
    
    // 主UI组件
    QTabWidget *m_tabWidget;
    
    // Station模式Widget
    PreparationPanel *m_preparationPanel;
    MonitorPanel *m_monitorPanel;
    TestPanel *m_testPanel;
    AutoTestPanel *m_autoTestPanel;
    
    // Terminal模式Widget
    DashboardWidget *m_dashboardWidget;
    PLCConnectionWidget *m_plcConnectionWidget;
    StationManagerWidget *m_stationManagerWidget;
    DataMonitorWidget *m_dataMonitorWidget;
    SystemLogsWidget *m_systemLogsWidget;
    SettingsWidget *m_settingsWidget;
    
    // 连接控制
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QLabel *m_connectionStatusLabel;
    bool m_isConnected;
    
    // Terminal状态指示
    QLabel *m_terminalStatusLabel;
    QLabel *m_plcStatusLabel;
    QLabel *m_stationStatusLabel;
    QLabel *m_dataStatsLabel;
};
```

**主要方法**：

```cpp
// 模式切换
void setWindowMode(WindowMode mode);

// 服务器/客户端关联
void setTerminalServer(std::shared_ptr<TerminalServer> server);
void setStationClient(std::shared_ptr<StationClient> client);

// UI创建
void createTabWidgetStation();   // Station模式4个标签页
void createTabWidgetTerminal();  // Terminal模式6个标签页

// 样式
void applyDarkTheme();
void createStatusBar();
void createStatusBarTerminal();

// 事件处理
void closeEvent(QCloseEvent *event) override;
```

**信号/槽**：

| 槽函数 | 触发源 | 功能 |
|--------|--------|------|
| `onConnect()` | 菜单/按钮 | 连接PLC/Terminal |
| `onDisconnect()` | 菜单/按钮 | 断开连接 |
| `onConfig()` | 菜单 | 打开配置对话框 |
| `onAbout()` | 菜单 | 显示关于窗口 |
| `onPLCConnected()` | TerminalServer | PLC已连接 |
| `onPLCDisconnected()` | TerminalServer | PLC已断开 |
| `onPLCError()` | TerminalServer | PLC错误 |
| `onStationConnected()` | TerminalServer | Station已连接 |
| `onStationDisconnected()` | TerminalServer | Station已断开 |
| `onDataReceived()` | StationClient | 数据接收更新 |
| `onTabChanged()` | QTabWidget | 标签页切换 |

## 窗口模式

### 1. Terminal Mode (主控台)

```
启用情况：运行在主服务器PC上
特点：
  - 深色主题设计（工业级）
  - 6个功能标签页
  - 支持PLC连接管理
  - 支持Station连接管理
  - 实时数据汇总展示
  - 系统日志查询
```

#### Terminal Mode 标签页

| # | 标签页 | 类名 | 描述 |
|---|--------|------|------|
| 1 | ⊞仪表板 | `DashboardWidget` | PLC+4个Station的综合状态展示 |
| 2 | ⚙PLC连接 | `PLCConnectionWidget` | PLC连接参数设置和状态监控 |
| 3 | 📡Station管理 | `StationManagerWidget` | 4个Station的连接管理 |
| 4 | 📊数据监控 | `DataMonitorWidget` | 压力/温度/流量趋势曲线 |
| 5 | 📋系统日志 | `SystemLogsWidget` | 系统事件日志查看 |
| 6 | ⚙配置 | `SettingsWidget` | 系统参数配置 |

### 2. Station Mode (操作台)

```
启用情况：运行在操作终端（平板/PC）上
特点：
  - 浅色主题设计（易于长时间操作）
  - 4个功能标签页
  - 显示来自Terminal的数据
  - 发送控制命令给Terminal
  - 本地测试控制界面
```

#### Station Mode 标签页

| # | 标签页 | 类名 | 描述 |
|---|--------|------|------|
| 1 | ① 测试准备区 | `PreparationPanel` | 试样/参数准备 |
| 2 | ② 实时监控 | `MonitorPanel` | 实时数据和状态 |
| 3 | ③ 测试区 | `TestPanel` | 测试控制和结果 |
| 4 | ④ 自动测试配置 | `AutoTestPanel` | 自动测试方案配置 |

## 菜单结构

### 文件菜单

```cpp
QMenu *fileMenu = menuBar()->addMenu("文件(&F)");

// 连接/断开
m_connectAction = fileMenu->addAction("连接(&C)");      // Ctrl+O
m_disconnectAction = fileMenu->addAction("断开(&D)");   // Ctrl+D

// 配置
m_settingsAction = fileMenu->addAction("配置(&S)");     // Ctrl+,

// 退出
m_exitAction = fileMenu->addAction("退出(&X)");         // Ctrl+Q
```

### 查看菜单

```cpp
QMenu *viewMenu = menuBar()->addMenu("查看(&V)");
m_viewLogsAction = viewMenu->addAction("查看日志(&L)"); // Ctrl+L
```

### 帮助菜单

```cpp
QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
m_helpAction = helpMenu->addAction("帮助文档(&H)");     // F1
m_aboutAction = helpMenu->addAction("关于(&A)");
```

## 状态栏

### Terminal Mode 状态栏

```
┌───────────────────────────────────────────────┐
│ 状态: 就绪  采样: 0帧 | 丢包: 0%  2025-01-15 │
└───────────────────────────────────────────────┘
```

## 数据流

### Terminal Mode 数据流向

```
TerminalServer (PLC数据采集 + 广播)
    ↓
MainWindow (接收Terminal信号)
    ├─ onPLCConnected()      ──→ m_plcStatusLabel 更新
    ├─ onStationConnected()  ──→ m_stationStatusLabel 更新
    ├─ onDataReceived()      ──→ DashboardWidget 更新
    └─ onPLCError()          ──→ 显示错误对话框

MainWindow 中各Widget
    ├─ DashboardWidget       ←─ Terminal 数据
    ├─ DataMonitorWidget     ←─ Terminal 数据
    ├─ SystemLogsWidget      ←─ 系统日志
    └─ SettingsWidget        →─ 配置更改
```

### Station Mode 数据流向

```
StationClient (连接到Terminal)
    ↓
MainWindow (接收Station信号)
    ├─ onConnect()           ──→ 启动各Panel数据接收
    ├─ onDisconnect()        ──→ 停止各Panel更新
    └─ onDataReceived()      ──→ 分发数据到各Panel

各Panel Widget
    ├─ PreparationPanel      ←─ Station 数据
    ├─ MonitorPanel          ←─ Station 数据
    ├─ TestPanel            ←─ Station 数据 + 发送命令
    └─ AutoTestPanel        ←─ Station 数据
```

## 初始化流程

### Station Mode 初始化

```cpp
// main.cpp Station分支
MainWindow window;
window.setWindowMode(WindowMode::STATION_MODE);

auto stationClient = std::make_shared<StationClient>();
window.setStationClient(stationClient);

// 连接Terminal
stationClient->connect("localhost", 5555);

window.show();
```

### Terminal Mode 初始化

```cpp
// main.cpp Terminal分支
MainWindow window;
window.setWindowMode(WindowMode::TERMINAL_MODE);

auto terminalServer = std::make_shared<TerminalServer>();
window.setTerminalServer(terminalServer);

// 启动Terminal Server
terminalServer->start("0.0.0.0", 5555);

window.show();
```

## 主题系统

### 深色主题 (Terminal Mode)

```css
/* 核心颜色 */
background-color: #2c3e50;      /* 深灰背景 */
border: 1px solid #1a252f;      /* 深边框 */
color: #ffffff;                 /* 白字 */

/* 按钮 */
QPushButton {
    background-color: #4CAF50;   /* 绿色 */
    color: white;
}
QPushButton:hover {
    background-color: #45a049;   /* 稍深绿 */
}

/* 状态指示 */
status-ok: #4CAF50;              /* 绿色 OK */
status-running: #FFC107;         /* 黄色 运行 */
status-error: #F44336;           /* 红色 错误 */
```

### 浅色主题 (Station Mode)

```css
/* 核心颜色 */
background-color: #ffffff;      /* 白背景 */
border: 1px solid #ddd;         /* 浅边框 */
color: #333333;                 /* 深灰字 */

/* 按钮 */
QPushButton {
    background-color: #1976d2;   /* 蓝色 */
    color: white;
}
```

## 扩展点

### 添加新的Terminal标签页

1. **定义Widget类**

   ```cpp
   // include/gui/NewTerminalWidget.h
   class NewTerminalWidget : public QWidget {
       Q_OBJECT
   public:
       explicit NewTerminalWidget(QWidget *parent = nullptr);
   private slots:
       void onDataReceived(const SensorData &data);
   };
   ```

2. **在MainWindow中添加**

   ```cpp
   // include/gui/MainWindow.h
   class MainWindow : public QMainWindow {
   private:
       NewTerminalWidget *m_newWidget;
   };
   
   // src/gui/MainWindow.cpp
   void MainWindow::createTerminalTabs() {
       m_newWidget = new NewTerminalWidget();
       m_tabWidget->addTab(m_newWidget, "新标签页");
   }
   ```

3. **连接信号**

   ```cpp
   void MainWindow::setupTerminalConnections() {
       if (m_terminalServer) {
           connect(m_terminalServer.get(), &TerminalServer::dataUpdated,
                   m_newWidget, &NewTerminalWidget::onDataReceived);
       }
   }
   ```

## 编译和运行

### 编译

```bash
cd build
cmake ..
cmake --build . --config Release
```

### 运行

**Terminal主控台**

```bash
WaterTestSystem.exe terminal
```

**Station操作台（单个）**

```bash
WaterTestSystem.exe station 1
```

**批量启动（Windows）**

```bash
.\launch_system.bat
```

## 调试技巧

### 启用调试信息

```cpp
// 在main.cpp中
#define DEBUG_LOGGING 1

#ifdef DEBUG_LOGGING
#define DEBUG_LOG qDebug()
#else
#define DEBUG_LOG if(0) qDebug()
#endif
```

### 查看日志

```bash
# Windows
type logs/application.log | tail -f

# Linux
tail -f logs/application.log
```

## 性能指标

| 指标 | 目标值 | 实现方式 |
|------|--------|---------|
| 数据延迟 | <100ms | 局域网TCP |
| GUI响应 | <50ms | Qt事件循环 |
| 内存占用 | <100MB | 高效数据结构 |
| CPU占用 | <5% | 事件驱动 |
| 最大Station数 | 4个 | 架构设计 |

## 常见问题

### Q: 如何在Terminal和Station模式间切换？

A: 调用 `mainWindow.setWindowMode(WindowMode::TERMINAL_MODE)` 或 `STATION_MODE`

### Q: 如何连接Terminal Server？

A: 在Station模式下，StationClient会自动尝试连接 `localhost:5555`

### Q: 如何自定义主题颜色？

A: 修改 `applyDarkTheme()` 中的样式表代码

### Q: 如何添加新的菜单项？

A: 在 `createMenuBar()` 中添加新的 `QAction`

---

**最后更新**：Task 1 完成后
**版本**：v2.0.0
**架构**：Terminal-Station 分布式
