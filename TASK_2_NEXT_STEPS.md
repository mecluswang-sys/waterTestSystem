# 下一步指南 - Task 2: Dashboard仪表板实现

## 概览

Task 2是实现Terminal主控台的首个核心功能标签页 - **Dashboard仪表板**。

完成时间预估：**2-3天**

## 成功标准

✅ 完成以下功能：

- [ ] PLC连接状态卡片（在线/离线，连接时长，心跳延迟）
- [ ] 4个Station连接状态卡片（在线/离线，IP地址，连接时长）
- [ ] 实时数据汇总显示（4个压力值、4个温度值、1个流量值）
- [ ] 数据采样统计（采样帧数、总帧数、运行时间、丢包率）
- [ ] 与TerminalServer信号关联
- [ ] 编译通过且无运行时错误
- [ ] 符合深色主题设计规范

## 技术要求

### 1. 创建DashboardWidget类

**文件位置**：

- `include/gui/DashboardWidget.h`
- `src/gui/DashboardWidget.cpp`

**基本结构**：

```cpp
// include/gui/DashboardWidget.h
#ifndef DASHBOARD_WIDGET_H
#define DASHBOARD_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <memory>

namespace WaterTest {
    struct SensorData;
    
    class DashboardWidget : public QWidget {
        Q_OBJECT
        
    public:
        explicit DashboardWidget(QWidget *parent = nullptr);
        ~DashboardWidget();
        
    public slots:
        void onPLCConnected();
        void onPLCDisconnected();
        void onStationConnected(uint8_t stationId);
        void onStationDisconnected(uint8_t stationId);
        void onDataReceived(const SensorData &data);
        void onDataStats(int frameCount, int totalFrames, 
                        qint64 runTimeMs, float packetLossRate);
        
    private:
        void setupUI();
        void applyStyles();
        
        // 状态指示卡片
        QWidget *createStatusCard(const QString &title);
        QWidget *createDataSummaryCard();
        QWidget *createStatsCard();
        
        // 成员变量
        QLabel *m_plcStatusLabel;
        QLabel *m_plcTimeLabel;
        QLabel *m_plcHeartbeatLabel;
        
        QLabel *m_stationStatusLabels[4];
        QLabel *m_stationIPLabels[4];
        QLabel *m_stationTimeLabels[4];
        
        QLabel *m_pressureLabels[4];
        QLabel *m_temperatureLabels[4];
        QLabel *m_flowLabel;
        
        QLabel *m_framecountLabel;
        QLabel *m_packetLossLabel;
        QLabel *m_runtimeLabel;
    };
}

#endif
```

### 2. 实现关键方法

```cpp
// src/gui/DashboardWidget.cpp

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent) {
    setupUI();
    applyStyles();
}

void DashboardWidget::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);
    
    // 顶部：标题
    auto *titleLabel = new QLabel("系统概览 Dashboard");
    titleLabel->setStyleSheet("QLabel { font-size: 18pt; font-weight: bold; color: #4CAF50; }");
    mainLayout->addWidget(titleLabel);
    
    // 中间：状态卡片（2x3网格）
    auto *gridLayout = new QGridLayout();
    gridLayout->setSpacing(10);
    
    // 创建PLC和4个Station的状态卡片
    m_plcStatusLabel = new QLabel("● 未连接");
    // ... 添加到grid ...
    
    mainLayout->addLayout(gridLayout);
    
    // 数据汇总卡片
    mainLayout->addWidget(createDataSummaryCard());
    
    // 统计卡片
    mainLayout->addWidget(createStatsCard());
    
    mainLayout->addStretch();
}

void DashboardWidget::onDataReceived(const SensorData &data) {
    // 更新4个压力值
    for (int i = 0; i < 4; ++i) {
        m_pressureLabels[i]->setText(
            QString("P%1: %2 bar").arg(i+1).arg(data.pressure[i], 0, 'f', 2));
    }
    
    // 更新4个温度值
    for (int i = 0; i < 4; ++i) {
        m_temperatureLabels[i]->setText(
            QString("T%1: %2°C").arg(i+1).arg(data.temperature[i], 0, 'f', 1));
    }
    
    // 更新流量
    m_flowLabel->setText(
        QString("流量: %1 L/s").arg(data.flowRate, 0, 'f', 2));
}

void DashboardWidget::applyStyles() {
    // 应用深色主题样式表
    setStyleSheet(R"(
        QWidget {
            background-color: #2c3e50;
            color: #ffffff;
        }
        QLabel {
            background-color: transparent;
        }
        /* 卡片样式 */
        QFrame {
            background-color: #34495e;
            border: 1px solid #1a252f;
            border-radius: 4px;
            padding: 10px;
        }
    )");
}
```

### 3. 主要UI组件

**PLC连接状态卡片**

```
┌──────────────────────┐
│   PLC 连接状态       │
├──────────────────────┤
│ ● 已连接             │
│                      │
│ 连接时长: 02:30:45   │
│ 最后心跳: 42ms       │
│ 版本: S7-1200 V3.0   │
└──────────────────────┘
```

**Station状态卡片** (4张)

```
┌──────────────────────┐
│ Station 1            │
├──────────────────────┤
│ ● 已连接             │
│ IP: 192.168.33.101   │
│ 连接时长: 01:15:30   │
│ 数据: 1245/1250帧    │
│ 丢包: 0%             │
└──────────────────────┘
```

**实时数据汇总卡片**

```
┌────────────────────────────────────────────────┐
│ 实时数据                                       │
├──────────┬──────────┬──────────┬──────────┤
│ P1: 1.5  │ P2: 2.1  │ P3: 1.8  │ P4: 2.3  │
│ T1: 25.3 │ T2: 24.8 │ T3: 26.1 │ T4: 25.6 │
│          │ 流量: 5.2 L/s       │          │
├──────────┴──────────┴──────────┴──────────┤
└────────────────────────────────────────────────┘
```

**采样统计卡片**

```
┌────────────────────────────────────────────────┐
│ 采样统计                                       │
├────────────────────────────────────────────────┤
│ 采样帧数: 1250 | 总帧数: 1250 | 丢包: 0%     │
│ 运行时间: 00:25:30                            │
└────────────────────────────────────────────────┘
```

## 集成步骤

### 1. 在MainWindow中添加包含

```cpp
// src/gui/MainWindow.cpp
#include "gui/DashboardWidget.h"
```

### 2. 在createTerminalTabs()中创建

```cpp
void MainWindow::createTerminalTabs() {
    // 创建Dashboard
    m_dashboardWidget = new DashboardWidget(this);
    m_tabWidget->addTab(m_dashboardWidget, "⊞ 仪表板");
    
    // ... 其他标签页 ...
}
```

### 3. 在setupTerminalConnections()中连接信号

```cpp
void MainWindow::setupTerminalConnections() {
    if (!m_terminalServer)
        return;
        
    // 连接PLC信号
    connect(m_terminalServer.get(), &TerminalServer::plcConnected,
            m_dashboardWidget, &DashboardWidget::onPLCConnected);
    connect(m_terminalServer.get(), &TerminalServer::plcDisconnected,
            m_dashboardWidget, &DashboardWidget::onPLCDisconnected);
    
    // 连接Station信号
    connect(m_terminalServer.get(), &TerminalServer::stationConnected,
            m_dashboardWidget, &DashboardWidget::onStationConnected);
    connect(m_terminalServer.get(), &TerminalServer::stationDisconnected,
            m_dashboardWidget, &DashboardWidget::onStationDisconnected);
    
    // 连接数据信号
    connect(m_terminalServer.get(), &TerminalServer::dataUpdated,
            m_dashboardWidget, &DashboardWidget::onDataReceived);
}
```

## 样式参考

参考文件：`docs/UI_DESIGN_REFERENCE.md`

关键颜色：

- 背景：#2c3e50
- 卡片：#34495e
- 文本：#ffffff
- OK状态：#4CAF50 (绿)
- 错误状态：#F44336 (红)
- 运行状态：#FFC107 (黄)

## 测试清单

- [ ] 编译无错误
- [ ] Window启动可见
- [ ] PLC连接状态动态更新
- [ ] 4个Station状态卡片显示
- [ ] 实时数据更新正常
- [ ] 统计数据显示准确
- [ ] 深色主题样式正确应用
- [ ] 窗口缩放时布局自适应
- [ ] 无内存泄漏

## 文件清单

需要创建的文件：

1. `include/gui/DashboardWidget.h`
2. `src/gui/DashboardWidget.cpp`

需要修改的文件：

1. `include/gui/MainWindow.h` - 检查声明
2. `src/gui/MainWindow.cpp` - 创建和连接Widget
3. `CMakeLists.txt` - 添加新文件到编译

## 后续任务

Task 2完成后，可以进行：

1. Task 3: PLC Connection标签页
2. Task 4: Station Manager标签页
3. ... 其他标签页

这些标签页可以独立并行开发。

## 参考资源

- Qt文档：<https://doc.qt.io/>
- 项目文档：`TERMINAL_GUI_DESIGN.md`
- UI设计：`UI_DESIGN_REFERENCE.md`
- 数据结构：`include/DataPipeline.h`
- Terminal Server API：`TERMINAL_MAINWINDOW_TECHNICAL_GUIDE.md`

---

**准备好开始？**

下一步：创建 `include/gui/DashboardWidget.h`
