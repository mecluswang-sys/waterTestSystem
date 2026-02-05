/**
 * @file MainWindow.cpp
 * @brief Main Window Implementation - Terminal Server & Station Client GUI
 */

#include "gui/MainWindow.h"
#include "gui/MonitorPanel.h"
#include "gui/PreparationPanel.h"
#include "gui/TestPanel.h"
#include "gui/AutoTestPanel.h"
#include "gui/ConfigDialog.h"
#include "DeviceManager.h"
#include "ConfigManager.h"
#include "S7PLCClient.h"
#include "TerminalServer.h"
#include "StationClient.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTabWidget>
#include <QProgressBar>
#include <QApplication>
#include <QFile>
#include <QStyle>
#include <QTime>
#include <QDateTime>

namespace WaterTest
{
    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent),
          m_mode(WindowMode::STATION_MODE),
          m_deviceManager(nullptr),
          m_terminalServer(nullptr),
          m_stationClient(nullptr),
          m_tabWidget(nullptr),
          m_preparationPanel(nullptr),
          m_monitorPanel(nullptr),
          m_testPanel(nullptr),
          m_autoTestPanel(nullptr),
          m_connectBtn(nullptr),
          m_disconnectBtn(nullptr),
          m_connectionStatusLabel(nullptr),
          m_isConnected(false),
          m_plcStatusBtn(nullptr),
          m_plcStatusTimer(nullptr),
          m_autoConnectTimer(nullptr),
          m_autoConnectEnabled(true),
          m_autoConnecting(false),
          m_dashboardWidget(nullptr),
          m_plcConnectionWidget(nullptr),
          m_stationManagerWidget(nullptr),
          m_dataMonitorWidget(nullptr),
          m_systemLogsWidget(nullptr),
          m_settingsWidget(nullptr)
    {
        setWindowTitle("水质测试系统");
        setMinimumSize(1400, 900);

        // Apply dark theme
        applyDarkTheme();

        // createMenuBar();

        // Default to station mode
        // Ensure setWindowMode creates the UI even if m_mode default equals STATION_MODE
        m_mode = WindowMode::TERMINAL_MODE; // force change so setWindowMode will rebuild UI
        setWindowMode(WindowMode::STATION_MODE);

        statusBar()->showMessage("Ready");
    }

    MainWindow::~MainWindow()
    {
        if (m_preparationPanel)
            m_preparationPanel->stopUpdate();
        if (m_monitorPanel)
            m_monitorPanel->stopUpdate();
        if (m_testPanel)
            m_testPanel->stopUpdate();
        if (m_autoTestPanel)
            m_autoTestPanel->stopUpdate();
    }

    void MainWindow::setWindowMode(WindowMode mode)
    {
        if (m_mode == mode)
            return;

        m_mode = mode;

        // Clear existing central widget
        if (centralWidget())
        {
            centralWidget()->deleteLater();
        }

        // Create appropriate UI based on mode
        if (mode == WindowMode::STATION_MODE)
        {
            setWindowTitle("水质测试系统 - 操作台");
            createTabWidgetStation();
        }
        else // TERMINAL_MODE
        {
            setWindowTitle("水质测试系统 - 主控台");
            createTabWidgetTerminal();
        }

        createStatusBar();
    }

    void MainWindow::setTerminalServer(std::shared_ptr<TerminalServer> server)
    {
        m_terminalServer = server;
        if (m_mode == WindowMode::TERMINAL_MODE)
        {
            setupTerminalConnections();
        }
    }

    void MainWindow::setStationClient(std::shared_ptr<StationClient> client)
    {
        m_stationClient = client;
        if (m_mode == WindowMode::STATION_MODE)
        {
            // Station mode setup if needed
        }
    }

    void MainWindow::applyDarkTheme()
    {
        QFile styleFile(":/styles/industrial_10inch.qss");
        if (styleFile.open(QFile::ReadOnly))
        {
            QString style = QLatin1String(styleFile.readAll());
            qApp->setStyle("Fusion");
            qApp->setStyleSheet(style);
            styleFile.close();
        }
        else
        {
            // Fallback theme if resource not available
            qApp->setStyle("Fusion");
        }
    }

    void MainWindow::createTabWidgetStation()
    {
        // 创建主容器
        auto *centralWidget = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 创建顶部连接控制栏
        auto *topBar = new QWidget(this);
        topBar->setObjectName("topBar");
        auto *topBarLayout = new QHBoxLayout(topBar);
        // 压缩顶部栏高度（PLC 那行更协调）
        topBarLayout->setContentsMargins(15, 4, 15, 4);
        topBarLayout->setSpacing(10);

        // 左侧：标题
        auto *titleLabel = new QLabel("水质测试系统 - 操作台", this);
        titleLabel->setObjectName("topTitle");
        topBarLayout->addWidget(titleLabel);

        topBarLayout->addStretch();

        // 右侧：PLC 状态按钮 + 连接/断开（仍保留手动控制）
        m_plcStatusBtn = new QPushButton("PLC", this);
        m_plcStatusBtn->setObjectName("plcStatusButton");
        m_plcStatusBtn->setEnabled(false); // 指示用
        m_plcStatusBtn->setProperty("plcState", "disconnected");
        m_plcStatusBtn->setToolTip("PLC 未连接");
        topBarLayout->addWidget(m_plcStatusBtn);

        m_connectBtn = new QPushButton("连接", this);
        m_connectBtn->setObjectName("connectButton");
        connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
        topBarLayout->addWidget(m_connectBtn);

        m_disconnectBtn = new QPushButton("断开", this);
        m_disconnectBtn->setEnabled(false);
        m_disconnectBtn->setObjectName("disconnectButton");
        connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
        topBarLayout->addWidget(m_disconnectBtn);

        // 右上角：时间显示（放在 PLC 连接按钮右侧）
        topBarLayout->addSpacing(12);
        auto *timeLabel = new QLabel(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), this);
        timeLabel->setObjectName("topTime");
        timeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
        topBarLayout->addWidget(timeLabel);
        auto *timeTimer = new QTimer(timeLabel);
        timeTimer->setInterval(1000);
        connect(timeTimer, &QTimer::timeout, timeLabel, [timeLabel]()
                { timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")); });
        timeTimer->start();

        mainLayout->addWidget(topBar);

        // 创建标签页
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabPosition(QTabWidget::North);
        m_tabWidget->setObjectName("mainTabs");

        // 初始化设备管理器
        setupDeviceManager();

        // Tab 1: 测试准备区
        m_preparationPanel = new PreparationPanel(m_deviceManager, this);
        m_tabWidget->addTab(m_preparationPanel, "① 测试准备区");

        // Tab 2: 实时监控
        m_monitorPanel = new MonitorPanel(m_deviceManager, this);
        m_tabWidget->addTab(m_monitorPanel, "② 实时监控");

        // Tab 3: 测试区
        m_testPanel = new TestPanel(m_deviceManager, this);
        m_tabWidget->addTab(m_testPanel, "③ 测试区");

        // Tab 4: 自动测试配置
        m_autoTestPanel = new AutoTestPanel(m_deviceManager, this);
        m_tabWidget->addTab(m_autoTestPanel, "④ 自动测试配置");

        mainLayout->addWidget(m_tabWidget);
        setCentralWidget(centralWidget);

        // 启动 PLC 状态刷新与自动连接（仅 Station 模式）
        startAutoConnect();
    }

    void MainWindow::createTabWidgetTerminal()
    {
        // 创建主容器
        auto *centralWidget = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 创建顶部状态栏
        auto *topBar = new QWidget(this);
        topBar->setStyleSheet("QWidget { background-color: #2c3e50; border-bottom: 1px solid #1a252f; }");
        auto *topBarLayout = new QHBoxLayout(topBar);
        topBarLayout->setContentsMargins(15, 10, 15, 10);

        // 左侧：标题
        auto *titleLabel = new QLabel("主控台 - Terminal Server", this);
        titleLabel->setStyleSheet("QLabel { font-size: 16pt; font-weight: bold; color: #4CAF50; }");
        topBarLayout->addWidget(titleLabel);

        topBarLayout->addSpacing(20);

        // 中间：状态指示
        m_terminalStatusLabel = new QLabel("● 待启动", this);
        m_terminalStatusLabel->setStyleSheet("QLabel { font-size: 11pt; color: #FFC107; font-weight: bold; }");
        topBarLayout->addWidget(m_terminalStatusLabel);

        m_plcStatusLabel = new QLabel("● PLC 已连接", this);
        m_plcStatusLabel->setStyleSheet("QLabel { font-size: 11pt; color: #4CAF50; font-weight: bold; padding: 0 15px; }");
        topBarLayout->addWidget(m_plcStatusLabel);

        m_stationStatusLabel = new QLabel("● Station 0/4", this);
        m_stationStatusLabel->setStyleSheet("QLabel { font-size: 11pt; color: #F44336; font-weight: bold; }");
        topBarLayout->addWidget(m_stationStatusLabel);

        topBarLayout->addStretch();

        // 右侧：时间显示
        auto *timeLabel = new QLabel(QTime::currentTime().toString("HH:mm:ss"), this);
        timeLabel->setStyleSheet("QLabel { font-size: 11pt; color: #bbb; }");
        topBarLayout->addWidget(timeLabel);

        mainLayout->addWidget(topBar);

        // 创建标签页容器
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabPosition(QTabWidget::North);
        m_tabWidget->setStyleSheet(
            "QTabBar::tab { height: 40px; font-size: 12pt; padding: 8px 20px; }"
            "QTabBar { background-color: #3a3a3a; }"
            "QTabWidget::pane { border: none; }");

        createTerminalTabs();

        mainLayout->addWidget(m_tabWidget);
        setCentralWidget(centralWidget);
    }

    void MainWindow::createTerminalTabs()
    {
        // 创建6个标签页的占位符
        // 这些将在后续任务中实现实际的Widget

        // Tab 1: Dashboard (仪表板)
        auto *dashboardPlaceholder = new QWidget();
        auto *dashboardLayout = new QVBoxLayout(dashboardPlaceholder);
        dashboardLayout->addWidget(new QLabel("Dashboard - 仪表板 (待实现)", dashboardPlaceholder));
        m_tabWidget->addTab(dashboardPlaceholder, "⊞ 仪表板");

        // Tab 2: PLC Connection
        auto *plcPlaceholder = new QWidget();
        auto *plcLayout = new QVBoxLayout(plcPlaceholder);
        plcLayout->addWidget(new QLabel("PLC Connection - PLC连接 (待实现)", plcPlaceholder));
        m_tabWidget->addTab(plcPlaceholder, "⚙ PLC连接");

        // Tab 3: Station Manager
        auto *stationPlaceholder = new QWidget();
        auto *stationLayout = new QVBoxLayout(stationPlaceholder);
        stationLayout->addWidget(new QLabel("Station Manager - Station管理 (待实现)", stationPlaceholder));
        m_tabWidget->addTab(stationPlaceholder, "📡 Station管理");

        // Tab 4: Data Monitor
        auto *dataPlaceholder = new QWidget();
        auto *dataLayout = new QVBoxLayout(dataPlaceholder);
        dataLayout->addWidget(new QLabel("Data Monitor - 数据监控 (待实现)", dataPlaceholder));
        m_tabWidget->addTab(dataPlaceholder, "📊 数据监控");

        // Tab 5: System Logs
        auto *logsPlaceholder = new QWidget();
        auto *logsLayout = new QVBoxLayout(logsPlaceholder);
        logsLayout->addWidget(new QLabel("System Logs - 系统日志 (待实现)", logsPlaceholder));
        m_tabWidget->addTab(logsPlaceholder, "📋 系统日志");

        // Tab 6: Settings
        auto *settingsPlaceholder = new QWidget();
        auto *settingsLayout = new QVBoxLayout(settingsPlaceholder);
        settingsLayout->addWidget(new QLabel("Settings - 配置 (待实现)", settingsPlaceholder));
        m_tabWidget->addTab(settingsPlaceholder, "⚙ 配置");

        connect(m_tabWidget, QOverload<int>::of(&QTabWidget::currentChanged),
                this, &MainWindow::onTabChanged);
    }

    void MainWindow::setupTerminalConnections()
    {
        if (!m_terminalServer)
            return;

        // TODO: Connect Terminal Server signals to slots
        // This will be implemented in Task 8
    }

    void MainWindow::createMenuBar()
    {
        // 文件菜单
        QMenu *fileMenu = menuBar()->addMenu("文件(&F)");

        m_connectAction = fileMenu->addAction("连接(&C)");
        m_connectAction->setShortcut(QKeySequence("Ctrl+O"));
        connect(m_connectAction, &QAction::triggered, this, &MainWindow::onConnect);

        m_disconnectAction = fileMenu->addAction("断开(&D)");
        m_disconnectAction->setShortcut(QKeySequence("Ctrl+D"));
        m_disconnectAction->setEnabled(false);
        connect(m_disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnect);

        fileMenu->addSeparator();

        m_settingsAction = fileMenu->addAction("配置(&S)");
        m_settingsAction->setShortcut(QKeySequence("Ctrl+,"));
        connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onConfig);

        fileMenu->addSeparator();

        m_exitAction = fileMenu->addAction("退出(&X)");
        m_exitAction->setShortcut(QKeySequence("Ctrl+Q"));
        connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

        // 查看菜单
        QMenu *viewMenu = menuBar()->addMenu("查看(&V)");
        m_viewLogsAction = viewMenu->addAction("查看日志(&L)");
        m_viewLogsAction->setShortcut(QKeySequence("Ctrl+L"));

        // 帮助菜单
        QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
        m_helpAction = helpMenu->addAction("帮助文档(&H)");
        m_helpAction->setShortcut(QKeySequence::HelpContents);

        m_aboutAction = helpMenu->addAction("关于(&A)");
        connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    }

    void MainWindow::createStatusBar()
    {
        if (m_mode == WindowMode::TERMINAL_MODE)
        {
            createStatusBarTerminal();
        }
        else
        {
            statusBar()->showMessage("系统已初始化");
        }
    }

    void MainWindow::createStatusBarTerminal()
    {
        // Terminal状态栏
        auto *statusLayout = new QHBoxLayout();
        statusLayout->setContentsMargins(5, 2, 5, 2);

        // 左侧：当前状态
        auto *stateLabel = new QLabel("状态: 就绪");
        statusLayout->addWidget(stateLabel);

        statusLayout->addSpacing(20);

        // 中间：采样统计
        m_dataStatsLabel = new QLabel("采样: 0帧 | 丢包: 0%");
        m_dataStatsLabel->setStyleSheet("QLabel { color: #4CAF50; }");
        statusLayout->addWidget(m_dataStatsLabel);

        statusLayout->addStretch();

        // 右侧：时间
        auto *timeLabel = new QLabel(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        statusLayout->addWidget(timeLabel);

        auto *container = new QWidget();
        container->setLayout(statusLayout);
        statusBar()->addWidget(container);
    }

    void MainWindow::setupDeviceManager()
    {
        if (!m_deviceManager)
        {
            m_deviceManager = std::make_shared<DeviceManager>();
        }
    }

    void MainWindow::onConnect()
    {
        m_autoConnectEnabled = true; // 用户主动点击“连接”后允许自动重试
        connectPlc(true);
    }

    void MainWindow::onDisconnect()
    {
        // 用户手动断开：停止自动重连，避免“刚断开又自动连上”
        m_autoConnectEnabled = false;
        disconnectPlc();
    }

    bool MainWindow::connectPlc(bool interactive)
    {
        if (m_isConnected)
        {
            statusBar()->showMessage("已连接到系统", 2000);
            setPlcStatusState("connected", "PLC 已连接");
            return true;
        }

        if (m_mode != WindowMode::STATION_MODE)
            return false;

        if (!m_deviceManager)
            setupDeviceManager();

        // Load configuration
        auto &config = ConfigManager::getInstance();
        if (!config.loadConfig("config/system.conf"))
        {
            config.setString("plc.ip", "192.168.33.1");
            config.setInt("plc.rack", 0);
            config.setInt("plc.slot", 1);
        }

        auto plcClient = std::make_shared<S7PLCClient>();

        S7PLCClient::ConnectionParams params;
        params.ipAddress = config.getString("plc.ip", "192.168.33.1");
        params.rack = config.getInt("plc.rack", 0);
        params.slot = config.getInt("plc.slot", 1);
        params.timeout = interactive ? 5000 : 2000; // 自动连接时缩短阻塞时间

        statusBar()->showMessage("正在连接PLC: " + QString::fromStdString(params.ipAddress) + "...");
        setPlcStatusState("connecting", "PLC 正在连接...");

        if (!plcClient->connect(params))
        {
            const QString errorMsg = "连接PLC失败: " + QString::fromStdString(plcClient->getLastError());
            if (interactive)
                QMessageBox::critical(this, "连接错误", errorMsg);
            statusBar()->showMessage("PLC连接失败", 3000);
            setPlcStatusState("disconnected", errorMsg);
            return false;
        }

        if (!m_deviceManager->initialize(plcClient))
        {
            const QString errorMsg = "设备管理器初始化失败";
            if (interactive)
                QMessageBox::critical(this, "错误", errorMsg);
            plcClient->disconnect();
            setPlcStatusState("disconnected", errorMsg);
            return false;
        }

        int interval = config.getInt("data.collection_interval", 1000);
        if (!m_deviceManager->startDataCollection(interval))
        {
            const QString errorMsg = "启动数据采集失败";
            if (interactive)
                QMessageBox::critical(this, "错误", errorMsg);
            plcClient->disconnect();
            setPlcStatusState("disconnected", errorMsg);
            return false;
        }

        if (m_preparationPanel)
            m_preparationPanel->startUpdate();
        if (m_monitorPanel)
            m_monitorPanel->startUpdate();
        if (m_testPanel)
            m_testPanel->startUpdate();
        if (m_autoTestPanel)
            m_autoTestPanel->startUpdate();

        m_isConnected = true;
        if (m_connectAction)
            m_connectAction->setEnabled(false);
        if (m_disconnectAction)
            m_disconnectAction->setEnabled(true);
        if (m_connectBtn)
            m_connectBtn->setEnabled(false);
        if (m_disconnectBtn)
            m_disconnectBtn->setEnabled(true);

        statusBar()->showMessage("PLC已连接", 2000);
        setPlcStatusState("connected", "PLC 已连接");
        return true;
    }

    void MainWindow::disconnectPlc()
    {
        if (!m_isConnected)
        {
            setPlcStatusState("disconnected", "PLC 未连接");
            return;
        }

        if (m_mode == WindowMode::STATION_MODE)
        {
            if (m_preparationPanel)
                m_preparationPanel->stopUpdate();
            if (m_monitorPanel)
                m_monitorPanel->stopUpdate();
            if (m_testPanel)
                m_testPanel->stopUpdate();
            if (m_autoTestPanel)
                m_autoTestPanel->stopUpdate();
            if (m_deviceManager)
                m_deviceManager->stopDataCollection();
        }

        m_isConnected = false;
        if (m_connectAction)
            m_connectAction->setEnabled(true);
        if (m_disconnectAction)
            m_disconnectAction->setEnabled(false);
        if (m_connectBtn)
            m_connectBtn->setEnabled(true);
        if (m_disconnectBtn)
            m_disconnectBtn->setEnabled(false);

        statusBar()->showMessage("已断开连接", 3000);
        setPlcStatusState("disconnected", "PLC 未连接");
    }

    void MainWindow::setPlcStatusState(const char *state, const QString &toolTip)
    {
        if (!m_plcStatusBtn)
            return;
        m_plcStatusBtn->setProperty("plcState", state);
        m_plcStatusBtn->setToolTip(toolTip);
        m_plcStatusBtn->style()->unpolish(m_plcStatusBtn);
        m_plcStatusBtn->style()->polish(m_plcStatusBtn);
        m_plcStatusBtn->update();
    }

    void MainWindow::startAutoConnect()
    {
        // 状态刷新定时器：用于 UI 指示更贴近真实连接状态
        if (!m_plcStatusTimer)
        {
            m_plcStatusTimer = new QTimer(this);
            m_plcStatusTimer->setInterval(1000);
            connect(m_plcStatusTimer, &QTimer::timeout, this, &MainWindow::updatePlcStatusUi);
        }
        if (!m_plcStatusTimer->isActive())
            m_plcStatusTimer->start();

        // 自动连接：启动后持续尝试，成功后自动停
        if (!m_autoConnectTimer)
        {
            m_autoConnectTimer = new QTimer(this);
            m_autoConnectTimer->setInterval(5000);
            connect(m_autoConnectTimer, &QTimer::timeout, this, &MainWindow::onAutoConnectTick);
        }

        // 初始立即尝试一次（避免等 5 秒）
        QTimer::singleShot(300, this, &MainWindow::onAutoConnectTick);
        if (m_autoConnectEnabled && !m_isConnected && !m_autoConnectTimer->isActive())
            m_autoConnectTimer->start();
    }

    void MainWindow::onAutoConnectTick()
    {
        if (m_mode != WindowMode::STATION_MODE)
            return;

        if (!m_autoConnectEnabled)
        {
            if (m_autoConnectTimer)
                m_autoConnectTimer->stop();
            return;
        }

        if (m_isConnected)
        {
            if (m_autoConnectTimer)
                m_autoConnectTimer->stop();
            setPlcStatusState("connected", "PLC 已连接");
            return;
        }

        m_autoConnecting = true;
        setPlcStatusState("connecting", "PLC 正在自动连接...");
        const bool ok = connectPlc(false);
        m_autoConnecting = false;

        if (ok)
        {
            if (m_autoConnectTimer)
                m_autoConnectTimer->stop();
        }
        else
        {
            // 保持定时器继续重试（不弹窗）
            if (m_autoConnectTimer && !m_autoConnectTimer->isActive())
                m_autoConnectTimer->start();
            setPlcStatusState("disconnected", "PLC 未连接（自动重试中，可点击连接）");
        }
    }

    void MainWindow::updatePlcStatusUi()
    {
        if (m_mode != WindowMode::STATION_MODE)
            return;

        if (!m_deviceManager)
        {
            setPlcStatusState(m_autoConnecting ? "connecting" : "disconnected", "设备管理器未初始化");
            return;
        }

        const bool hasPlc = m_deviceManager->hasPlcClient();
        const bool plcConnected = hasPlc && m_deviceManager->isPlcConnected();

        if (plcConnected)
        {
            setPlcStatusState("connected", "PLC 已连接");
        }
        else if (m_autoConnecting)
        {
            setPlcStatusState("connecting", "PLC 正在连接...");
        }
        else
        {
            setPlcStatusState("disconnected", hasPlc ? "PLC 未连接" : "PLC 未初始化");
        }
    }

    void MainWindow::onConfig()
    {
        ConfigDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
        {
            statusBar()->showMessage("配置已保存", 3000);
        }
    }

    void MainWindow::onAbout()
    {
        QMessageBox::about(this, "关于本软件",
                           "<h3>水质测试系统 v2.0</h3>"
                           "<p>Terminal-Station 分布式架构</p>"
                           "<p>与西门子 S7-1200 PLC 兼容</p>"
                           "<p>用于实时监控和控制水质测试设备</p>"
                           "<p>Copyright © 2026 WaterTest</p>");
    }

    void MainWindow::onPLCConnected()
    {
        if (m_plcStatusLabel)
        {
            m_plcStatusLabel->setText("● PLC 已连接");
            m_plcStatusLabel->setStyleSheet("QLabel { font-size: 11pt; color: #4CAF50; font-weight: bold; }");
        }
    }

    void MainWindow::onPLCDisconnected()
    {
        if (m_plcStatusLabel)
        {
            m_plcStatusLabel->setText("● PLC 断开");
            m_plcStatusLabel->setStyleSheet("QLabel { font-size: 11pt; color: #F44336; font-weight: bold; }");
        }
    }

    void MainWindow::onPLCError(const QString &error)
    {
        QMessageBox::warning(this, "PLC错误", error);
    }

    void MainWindow::onStationConnected(uint8_t stationId)
    {
        // Update station count
        // This will be updated in Task 4
    }

    void MainWindow::onStationDisconnected(uint8_t stationId)
    {
        // Update station count
        // This will be updated in Task 4
    }

    void MainWindow::onDataReceived(const struct SensorData &data)
    {
        // Update data display
        // This will be updated in Task 2
    }

    void MainWindow::onTabChanged(int index)
    {
        // Handle tab changes if needed
    }

    void MainWindow::closeEvent(QCloseEvent *event)
    {
        if (m_isConnected)
        {
            int ret = QMessageBox::question(this, "确认退出",
                                            "系统仍处于连接状态。是否要断开连接并退出?",
                                            QMessageBox::Yes | QMessageBox::No);

            if (ret == QMessageBox::Yes)
            {
                onDisconnect();
                event->accept();
            }
            else
            {
                event->ignore();
            }
        }
        else
        {
            event->accept();
        }
    }

} // namespace WaterTest
