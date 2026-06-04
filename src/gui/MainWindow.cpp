/**
 * @file MainWindow.cpp
 * @brief Main Window Implementation - Terminal Server & Station Client GUI
 */

#include "gui/MainWindow.h"
#include "gui/MonitorPanel.h"
#include "gui/PreparationPanel.h"
#include "gui/TestPanel.h"
#include "gui/AutoTestPanel.h"
#include "gui/Station1Panel.h"
#include "gui/ConfigDialog.h"
#include "PumpWidget.h"
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
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QStyle>
#include <QTime>
#include <QDateTime>
#include <QThread>

namespace WaterTest
{
    namespace
    {
        constexpr double kKPaPerKgfCm2 = 98.0665;

        static double kPaToKgfCm2(double kpa)
        {
            return kpa / kKPaPerKgfCm2;
        }

        static double kgfCm2ToKPa(double kgfCm2)
        {
            return kgfCm2 * kKPaPerKgfCm2;
        }
    }

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
          m_station1Panel(nullptr),
          m_connectBtn(nullptr),
          m_disconnectBtn(nullptr),
          m_connectionStatusLabel(nullptr),
          m_isConnected(false),
          m_plcStatusBtn(nullptr),
          m_plcStatusTimer(nullptr),
          m_terminalStatusTimer(nullptr),
          m_autoConnectTimer(nullptr),
          m_autoConnectEnabled(false),
          m_autoConnecting(false),
          m_diagVirtualConnected(false),
          m_dashboardWidget(nullptr),
          m_plcConnectionWidget(nullptr),
          m_stationManagerWidget(nullptr),
          m_dataMonitorWidget(nullptr),
          m_systemLogsWidget(nullptr),
          m_settingsWidget(nullptr),
          m_terminalStatusLabel(nullptr),
          m_plcStatusLabel(nullptr),
          m_stationStatusLabel(nullptr),
          m_dataStatsLabel(nullptr),
          m_dataCollectionProgressBar(nullptr),
          m_connectAction(nullptr),
          m_disconnectAction(nullptr),
          m_exitAction(nullptr),
          m_settingsAction(nullptr),
          m_viewLogsAction(nullptr),
          m_helpAction(nullptr),
          m_aboutAction(nullptr)
    {
        setWindowTitle("水质测试系统");
        setMinimumSize(1400, 900);

        // Apply app theme (graphite/light/ocean)
        applyAppTheme();

        // createMenuBar();

        // Default to station mode
        // Ensure setWindowMode creates the UI even if m_mode default equals STATION_MODE
        m_mode = WindowMode::TERMINAL_MODE; // force change so setWindowMode will rebuild UI
        setWindowMode(WindowMode::STATION_MODE);

        statusBar()->showMessage("Ready");
    }

    MainWindow::~MainWindow()
    {
        // 先停UI定时器，避免析构期间继续触发槽函�?
        if (m_plcStatusTimer)
            m_plcStatusTimer->stop();
        if (m_autoConnectTimer)
            m_autoConnectTimer->stop();
        if (m_terminalStatusTimer)
            m_terminalStatusTimer->stop();

        // 再停各页面刷�?
        if (m_preparationPanel)
            m_preparationPanel->stopUpdate();
        if (m_monitorPanel)
            m_monitorPanel->stopUpdate();
        if (m_testPanel)
            m_testPanel->stopUpdate();
        if (m_autoTestPanel)
            m_autoTestPanel->stopUpdate();

        // 最后停采集线程
        if (m_deviceManager)
        {
            m_deviceManager->stopDataCollection();
        }
    }

    void MainWindow::setWindowMode(WindowMode mode)
    {
        if (m_mode == mode)
            return;

        if (m_terminalStatusTimer && m_terminalStatusTimer->isActive())
        {
            m_terminalStatusTimer->stop();
        }

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
        if (m_terminalServer)
        {
            setupTerminalConnections();
        }
    }

    void MainWindow::setStationClient(std::shared_ptr<StationClient> client)
    {
        m_stationClient = client;
        if (m_mode == WindowMode::STATION_MODE && m_stationClient)
        {
            const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);

            connect(m_stationClient.get(), &StationClient::connected,
                    this, [this]()
                    {
                        m_isConnected = true;
                        statusBar()->showMessage("已连接到主控台", 3000);
                        setPlcStatusState("connected", "远程模式：数据来自主控台");
                    });
            connect(m_stationClient.get(), &StationClient::disconnected,
                    this, [this]()
                    {
                        m_isConnected = false;
                        statusBar()->showMessage("与主控台连接已断开", 3000);
                        setPlcStatusState("disconnected", "远程模式：等待主控台连接");
                    });
            connect(m_stationClient.get(), &StationClient::errorOccurred,
                    this, [this](const QString &error)
                    {
                        statusBar()->showMessage(QString("主控台通信错误: %1").arg(error), 5000);
                    });
            connect(m_stationClient.get(), &StationClient::dataUpdated,
                    this, &MainWindow::onDataReceived);

            if (m_testPanel)
            {
                m_testPanel->setStationClient(m_stationClient);
            }
            if (m_preparationPanel)
            {
                m_preparationPanel->setStationClient(m_stationClient);
            }
            if (m_station1Panel)
            {
                m_station1Panel->setStationClient(m_stationClient);
            }
            if (m_autoTestPanel)
            {
                m_autoTestPanel->setStationClient(m_stationClient);
            }

            if (strictRemoteMode)
            {
                m_autoConnectEnabled = false;
                if (m_autoConnectTimer)
                    m_autoConnectTimer->stop();
                if (m_connectBtn)
                    m_connectBtn->setEnabled(false);
                if (m_disconnectBtn)
                    m_disconnectBtn->setEnabled(false);
                if (m_connectAction)
                    m_connectAction->setEnabled(false);
                if (m_disconnectAction)
                    m_disconnectAction->setEnabled(false);

                setPlcStatusState(m_stationClient->isConnected() ? "connected" : "disconnected",
                                  m_stationClient->isConnected() ? "远程模式：已连接主控台" : "远程模式：等待主控台连接");
            }
        }
    }

    void MainWindow::applyAppTheme()
    {
        auto &config = ConfigManager::getInstance();
        const QString uiTheme = QString::fromStdString(config.getString("ui.theme", "")).trimmed().toLower();
        const QString hmiTheme = QString::fromStdString(config.getString("ui.hmi.theme", "")).trimmed().toLower();
        const QString theme = !uiTheme.isEmpty() ? uiTheme : (!hmiTheme.isEmpty() ? hmiTheme : QString("graphite"));

        QString qssPath = ":/styles/industrial_10inch_graphite.qss";
        if (theme == "light" || theme == "graywhite" || theme == "greywhite")
            qssPath = ":/styles/industrial_10inch_light.qss";
        else if (theme == "ocean" || theme == "aqua" || theme == "teal")
            qssPath = ":/styles/industrial_10inch_ocean.qss";

        QFile styleFile(qssPath);
        if (styleFile.open(QFile::ReadOnly))
        {
            QString style = QLatin1String(styleFile.readAll());
            qApp->setStyle("Fusion");
            qApp->setStyleSheet(style);
            styleFile.close();
            qInfo().noquote() << QString("[UI主题] apply theme=%1 (ui.theme=%2 ui.hmi.theme=%3)").arg(theme, uiTheme, hmiTheme);
        }
        else
        {
            // Fallback theme if resource not available
            qApp->setStyle("Fusion");
            qWarning().noquote() << QString("[UI主题] load qss fail: %1").arg(qssPath);
        }
    }

    void MainWindow::createTabWidgetStation()
    {
        auto *centralWidget = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 创建顶部连接控制�?
        auto *topBar = new QWidget(this);
        topBar->setObjectName("topBar");
        auto *topBarLayout = new QHBoxLayout(topBar);
        // 压缩顶部栏高度（PLC 那行更协调）
        topBarLayout->setContentsMargins(15, 4, 15, 4);
        topBarLayout->setSpacing(10);

        // 左侧：标题
        auto *titleLabel = new QLabel("水介质电磁阀测试系统", this);
        titleLabel->setObjectName("topTitle");
        topBarLayout->addWidget(titleLabel);

        topBarLayout->addStretch();

        // 右侧：设置按钮 + 连接/断开
        m_plcStatusBtn = new QPushButton("设置", this);
        m_plcStatusBtn->setObjectName("plcStatusButton");
        m_plcStatusBtn->setProperty("role", "settings");
        m_plcStatusBtn->setToolTip("设置测试参数");
        connect(m_plcStatusBtn, &QPushButton::clicked, this, &MainWindow::onConfig);
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

        // Tab 5: 1号操作台（流程图）
        // 说明：该页面包含高频图元/动画刷新；默认关闭以提升启动稳定性。
        // 如需启用，可在 config/system.conf 中设置：ui.enable_station1_panel = true
        {
            auto &config = ConfigManager::getInstance();
            const bool enableStation1Panel = config.getBool("ui.enable_station1_panel", false);
            if (enableStation1Panel)
            {
                m_station1Panel = new Station1Panel(m_deviceManager, this);
                m_tabWidget->addTab(m_station1Panel, "⑤ 1号操作台");
            }
            else
            {
                auto *disabled = new QWidget(this);
                auto *disabledLayout = new QVBoxLayout(disabled);
                auto *msg = new QLabel("1号操作台页面已关闭（可在 config/system.conf 设置 ui.enable_station1_panel = true 启用）", disabled);
                msg->setWordWrap(true);
                msg->setAlignment(Qt::AlignCenter);
                disabledLayout->addStretch();
                disabledLayout->addWidget(msg);
                disabledLayout->addStretch();
                m_tabWidget->addTab(disabled, "⑤ 1号操作台");
            }
        }

        // Tab 6: 泵绘制预览
        {
            auto *pumpPreviewPage = new QWidget(this);
            auto *pumpPreviewLayout = new QVBoxLayout(pumpPreviewPage);
            pumpPreviewLayout->setContentsMargins(12, 12, 12, 12);
            pumpPreviewLayout->setSpacing(10);

            auto *pumpPreviewTitle = new QLabel("泵绘制预览", pumpPreviewPage);
            pumpPreviewTitle->setObjectName("pageTitle");
            pumpPreviewLayout->addWidget(pumpPreviewTitle);

            auto *pumpPreviewHint = new QLabel("用于查看 PumpWidget 的动态渲染效果。", pumpPreviewPage);
            pumpPreviewHint->setWordWrap(true);
            pumpPreviewLayout->addWidget(pumpPreviewHint);

            auto *pumpWidget = new PumpWidget(pumpPreviewPage);
            pumpWidget->setMinimumSize(700, 480);
            pumpWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            pumpPreviewLayout->addWidget(pumpWidget, 1);

            m_tabWidget->addTab(pumpPreviewPage, "⑥ 泵绘制预览");
        }

        mainLayout->addWidget(m_tabWidget);
        setCentralWidget(centralWidget);

        // 默认关闭开机自动连接，避免在网络/PLC异常时进入重复重连路径。
        // 需要自动连接可在配置中启用：plc.auto_connect_on_start = true
        {
            auto &config = ConfigManager::getInstance();
            m_autoConnectEnabled = config.getBool("plc.auto_connect_on_start", false);
        }

        // 启动 PLC 状态刷新与自动连接（仅 Station 模式�?
        startAutoConnect();
    }

    void MainWindow::createTabWidgetTerminal()
    {
        // 创建主容�?
        auto *centralWidget = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 创建顶部状态栏
        auto *topBar = new QWidget(this);
        topBar->setObjectName("topBar");
        auto *topBarLayout = new QHBoxLayout(topBar);
        topBarLayout->setContentsMargins(15, 10, 15, 10);

        // 左侧：标题
        auto *titleLabel = new QLabel("主控台 - Terminal Server", this);
        titleLabel->setObjectName("topTitle");
        topBarLayout->addWidget(titleLabel);

        topBarLayout->addSpacing(20);

        // 中间：状态指示
        m_terminalStatusLabel = new QLabel("● 待启动", this);
        m_terminalStatusLabel->setObjectName("terminalStatus");
        m_terminalStatusLabel->setProperty("tone", "warn");
        topBarLayout->addWidget(m_terminalStatusLabel);

        m_plcStatusLabel = new QLabel("● PLC 已连接", this);
        m_plcStatusLabel->setObjectName("plcStatus");
        m_plcStatusLabel->setProperty("tone", "good");
        topBarLayout->addWidget(m_plcStatusLabel);

        m_stationStatusLabel = new QLabel("● Station 0/4", this);
        m_stationStatusLabel->setObjectName("stationStatus");
        m_stationStatusLabel->setProperty("tone", "bad");
        topBarLayout->addWidget(m_stationStatusLabel);

        topBarLayout->addStretch();

        // 右侧：时间显示
        auto *timeLabel = new QLabel(QTime::currentTime().toString("HH:mm:ss"), this);
        timeLabel->setObjectName("topTime");
        topBarLayout->addWidget(timeLabel);
        auto *timeTimer = new QTimer(timeLabel);
        timeTimer->setInterval(1000);
        connect(timeTimer, &QTimer::timeout, timeLabel, [timeLabel]()
            { timeLabel->setText(QTime::currentTime().toString("HH:mm:ss")); });
        timeTimer->start();

        mainLayout->addWidget(topBar);

        // 创建标签页容器
        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setTabPosition(QTabWidget::North);
        m_tabWidget->setObjectName("mainTabs");

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

        connect(m_terminalServer.get(), &TerminalServer::stationConnected,
                this, [this](uint8_t stationId, const QString &)
                {
                    onStationConnected(stationId);
                });
        connect(m_terminalServer.get(), &TerminalServer::stationDisconnected,
                this, &MainWindow::onStationDisconnected);
        connect(m_terminalServer.get(), &TerminalServer::dataReceived,
                this, &MainWindow::onDataReceived);
        connect(m_terminalServer.get(), &TerminalServer::errorOccurred,
                this, &MainWindow::onPLCError);

        if (m_terminalStatusLabel)
        {
            m_terminalStatusLabel->setText("● 主控服务已连接");
            m_terminalStatusLabel->setProperty("tone", "good");
            m_terminalStatusLabel->style()->unpolish(m_terminalStatusLabel);
            m_terminalStatusLabel->style()->polish(m_terminalStatusLabel);
        }

        if (!m_terminalStatusTimer)
        {
            m_terminalStatusTimer = new QTimer(this);
            m_terminalStatusTimer->setInterval(1000);
            connect(m_terminalStatusTimer, &QTimer::timeout, this, &MainWindow::updateTerminalStatusUi);
        }
        if (!m_terminalStatusTimer->isActive())
        {
            m_terminalStatusTimer->start();
        }
        updateTerminalStatusUi();
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

        m_exitAction = fileMenu->addAction("退�?&X)");
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

        // 左侧：当前状�?
        auto *stateLabel = new QLabel("状�? 就绪");
        statusLayout->addWidget(stateLabel);

        statusLayout->addSpacing(20);

        // 中间：采样统�?
        m_dataStatsLabel = new QLabel("采样: 0�?| 丢包: 0%");
        m_dataStatsLabel->setStyleSheet("QLabel { color: #4CAF50; }");
        statusLayout->addWidget(m_dataStatsLabel);

        statusLayout->addStretch();

        // 右侧：时�?
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
        if (m_stationClient && ConfigManager::getInstance().getBool("station.strict_remote_mode", true))
        {
            statusBar()->showMessage("远程模式已启用：操作台不允许本地连接PLC", 4000);
            setPlcStatusState(m_stationClient->isConnected() ? "connected" : "disconnected",
                              m_stationClient->isConnected() ? "远程模式：已连接主控台" : "远程模式：等待主控台连接");
            return;
        }

        m_autoConnectEnabled = true; // 用户主动点击“连接”后允许自动重试
        connectPlc(true);
    }

    void MainWindow::onDisconnect()
    {
        // 用户手动断开：停止自动重连，避免“刚断开又自动连上�?
        m_autoConnectEnabled = false;
        disconnectPlc();
    }

    bool MainWindow::connectPlc(bool interactive)
    {
        auto syncStationPanelsOnce = [this]() {
            if (m_station1Panel)
                m_station1Panel->syncVisualStateOnce();
        };

        auto &config = ConfigManager::getInstance();
        const bool strictRemoteMode = config.getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            const bool terminalConnected = m_stationClient->isConnected();
            m_isConnected = terminalConnected;
            m_diagVirtualConnected = false;

            if (terminalConnected)
            {
                statusBar()->showMessage("远程模式：已连接主控台，数据由主控统一下发", 3000);
                setPlcStatusState("connected", "远程模式：数据来自主控台");
            }
            else
            {
                if (interactive)
                {
                    statusBar()->showMessage("远程模式：请先连接主控台", 3000);
                }
                setPlcStatusState("disconnected", "远程模式：等待主控台连接");
            }
            if (terminalConnected)
                syncStationPanelsOnce();
            return terminalConnected;
        }

        if (m_isConnected)
        {
            if (m_diagVirtualConnected)
            {
                statusBar()->showMessage("诊断模式已连接（未实际连接PLC）", 2000);
                setPlcStatusState("connected", "诊断模式：未实际连接PLC");
            }
            else
            {
                statusBar()->showMessage("已连接到系统", 2000);
                setPlcStatusState("connected", "PLC 已连接");
            }
            syncStationPanelsOnce();
            return true;
        }

        if (m_mode != WindowMode::STATION_MODE)
            return false;

        if (!m_deviceManager)
            setupDeviceManager();

        // Load configuration
        if (!config.loadConfig("config/system.conf"))
        {
            config.setString("plc.ip", "192.168.33.1");
            config.setInt("plc.rack", 0);
            config.setInt("plc.slot", 1);
        }

        const bool skipDataCollection = config.getBool("diag.skip_data_collection_on_connect", false);
        const bool skipPanelUpdates = config.getBool("diag.skip_panel_updates_on_connect", false);
        const bool skipPlcConnect = config.getBool("diag.skip_plc_connect_on_connect", (skipDataCollection && skipPanelUpdates));
        const bool enablePreparationPanelUpdate = config.getBool("ui.enable_preparation_live_update", false);
        const bool enableAutoTestPanelUpdate = config.getBool("ui.enable_auto_test_live_update", false);

        if (skipPlcConnect)
        {
            m_diagVirtualConnected = true;
            m_isConnected = true;
            if (m_connectAction)
                m_connectAction->setEnabled(false);
            if (m_disconnectAction)
                m_disconnectAction->setEnabled(true);
            if (m_connectBtn)
                m_connectBtn->setEnabled(false);
            if (m_disconnectBtn)
                m_disconnectBtn->setEnabled(true);

            if (m_autoConnectTimer)
                m_autoConnectTimer->stop();

            statusBar()->showMessage("诊断模式连接成功（已跳过PLC实连/采集/页面刷新）", 3000);
            setPlcStatusState("connected", "诊断模式：已跳过PLC实连");
            syncStationPanelsOnce();
            return true;
        }

        m_diagVirtualConnected = false;

        auto plcClient = std::make_shared<S7PLCClient>();

        S7PLCClient::ConnectionParams params;
        params.ipAddress = config.getString("plc.ip", "192.168.33.1");
        params.rack = config.getInt("plc.rack", 0);
        params.slot = config.getInt("plc.slot", 1);
        params.timeout = interactive ? 5000 : 2000; // 自动连接时缩短阻塞时�?

        statusBar()->showMessage("正在连接PLC: " + QString::fromStdString(params.ipAddress) + "...");
        setPlcStatusState("connecting", "PLC 正在连接...");

        bool plcConnected = false;
        std::string lastConnectError;
        constexpr int kConnectAttempts = 3;
        for (int attempt = 1; attempt <= kConnectAttempts; ++attempt)
        {
            if (attempt > 1)
            {
                statusBar()->showMessage(QString("正在重试连接PLC (%1/%2): ").arg(attempt).arg(kConnectAttempts) +
                                         QString::fromStdString(params.ipAddress));
            }

            if (plcClient->connect(params))
            {
                plcConnected = true;
                break;
            }

            lastConnectError = plcClient->getLastError();
            if (attempt < kConnectAttempts)
            {
                QThread::msleep(350);
            }
        }

        if (!plcConnected)
        {
            const QString errorMsg = QString("连接PLC失败(重试%1次): %2")
                                         .arg(kConnectAttempts)
                                         .arg(QString::fromStdString(lastConnectError));
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

        // 初始化数据保存系�?
        if (!m_deviceManager->initializeDataLogging("deploy/logs"))
        {
            const QString errorMsg = "数据保存系统初始化失败";
            if (interactive)
                QMessageBox::warning(this, "警告", errorMsg);
            // 不中断连接流程，数据保存失败不影响系统运�?
        }
        else
        {
            // 启用数据保存
            m_deviceManager->setDataLoggingEnabled(true);
        }

        if (!skipDataCollection)
        {
            int interval = config.getInt("data.collection_interval", 1000);
            if (interval < 50)
                interval = 50;
            else if (interval > 5000)
                interval = 5000;
            if (!m_deviceManager->startDataCollection(interval))
            {
                const QString errorMsg = "启动数据采集失败";
                if (interactive)
                    QMessageBox::critical(this, "错误", errorMsg);
                plcClient->disconnect();
                setPlcStatusState("disconnected", errorMsg);
                return false;
            }
        }

        if (!skipPanelUpdates)
        {
            if (m_monitorPanel)
                m_monitorPanel->startUpdate();

            // 为提升连接稳定性，复杂页面默认不启动高频定时刷新；按需通过配置逐步打开�?
            if (enablePreparationPanelUpdate && m_preparationPanel)
                m_preparationPanel->startUpdate();
            // 测试区实时刷新在代码层强制关闭，避免与站1控制链路冲突。
            if (enableAutoTestPanelUpdate && m_autoTestPanel)
                m_autoTestPanel->startUpdate();
        }

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
        syncStationPanelsOnce();
        return true;
    }

    void MainWindow::disconnectPlc()
    {
        if (!m_isConnected)
        {
            setPlcStatusState("disconnected", "PLC 未连接");
            return;
        }

        if (m_stationClient && ConfigManager::getInstance().getBool("station.strict_remote_mode", true))
        {
            m_stationClient->disconnectFromTerminal();
            m_isConnected = false;
            statusBar()->showMessage("已与主控台断开", 3000);
            setPlcStatusState("disconnected", "远程模式：等待主控台连接");
            return;
        }

        m_diagVirtualConnected = false;

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
            {
                m_deviceManager->stopDataCollection();
                
                // 关闭并刷新数据保�?
                if (m_deviceManager->isDataLoggingEnabled())
                {
                    m_deviceManager->setDataLoggingEnabled(false);
                    m_deviceManager->flushDataLogging();
                }
            }
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
        if (m_plcStatusBtn->property("role").toString() == "settings")
            return;
        m_plcStatusBtn->setProperty("plcState", state);
        m_plcStatusBtn->setToolTip(toolTip);
        m_plcStatusBtn->style()->unpolish(m_plcStatusBtn);
        m_plcStatusBtn->style()->polish(m_plcStatusBtn);
        m_plcStatusBtn->update();
    }

    void MainWindow::startAutoConnect()
    {
        // 状态刷新定时器：用�?UI 指示更贴近真实连接状�?
        if (!m_plcStatusTimer)
        {
            m_plcStatusTimer = new QTimer(this);
            m_plcStatusTimer->setInterval(1000);
            connect(m_plcStatusTimer, &QTimer::timeout, this, &MainWindow::updatePlcStatusUi);
        }
        if (!m_plcStatusTimer->isActive())
            m_plcStatusTimer->start();

        if (!m_autoConnectEnabled)
        {
            if (m_autoConnectTimer)
                m_autoConnectTimer->stop();
            setPlcStatusState("disconnected", "PLC 未连接（点击连接）");
            return;
        }

        // 自动连接：启动后持续尝试，成功后自动�?
        if (!m_autoConnectTimer)
        {
            m_autoConnectTimer = new QTimer(this);
            m_autoConnectTimer->setInterval(5000);
            connect(m_autoConnectTimer, &QTimer::timeout, this, &MainWindow::onAutoConnectTick);
        }

        // 初始立即尝试一次（避免�?5 秒）
        QTimer::singleShot(300, this, &MainWindow::onAutoConnectTick);
        if (m_autoConnectEnabled && !m_isConnected && !m_autoConnectTimer->isActive())
            m_autoConnectTimer->start();
    }

    void MainWindow::onAutoConnectTick()
    {
        if (m_mode != WindowMode::STATION_MODE)
            return;

        if (m_stationClient && ConfigManager::getInstance().getBool("station.strict_remote_mode", true))
        {
            if (m_stationClient->isConnected())
            {
                m_isConnected = true;
                setPlcStatusState("connected", "远程模式：已连接主控台");
            }
            else
            {
                m_isConnected = false;
                setPlcStatusState("disconnected", "远程模式：等待主控台连接");
            }
            return;
        }

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

        if (m_stationClient && ConfigManager::getInstance().getBool("station.strict_remote_mode", true))
        {
            const bool terminalConnected = m_stationClient->isConnected();
            m_isConnected = terminalConnected;
            setPlcStatusState(terminalConnected ? "connected" : "disconnected",
                              terminalConnected ? "远程模式：数据来自主控台" : "远程模式：等待主控台连接");
            return;
        }

        if (m_diagVirtualConnected && m_isConnected)
        {
            setPlcStatusState("connected", "诊断模式：未实际连接PLC");
            return;
        }

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

    void MainWindow::updateTerminalStatusUi()
    {
        if (m_mode != WindowMode::TERMINAL_MODE)
            return;

        const bool serverRunning = (m_terminalServer && m_terminalServer->isRunning());

        if (m_terminalStatusLabel)
        {
            m_terminalStatusLabel->setText(serverRunning ? "● 主控服务运行中" : "● 主控服务未启动");
            m_terminalStatusLabel->setProperty("tone", serverRunning ? "good" : "bad");
            m_terminalStatusLabel->style()->unpolish(m_terminalStatusLabel);
            m_terminalStatusLabel->style()->polish(m_terminalStatusLabel);
        }

        bool plcHasClient = false;
        bool plcConnected = false;
        if (m_terminalServer)
        {
            auto deviceManager = m_terminalServer->getDeviceManager();
            if (deviceManager)
            {
                plcHasClient = deviceManager->hasPlcClient();
                plcConnected = deviceManager->isPlcConnected();
            }
        }

        if (m_plcStatusLabel)
        {
            if (plcConnected)
            {
                m_plcStatusLabel->setText("● PLC 已连接");
                m_plcStatusLabel->setProperty("tone", "good");
            }
            else if (plcHasClient)
            {
                m_plcStatusLabel->setText("● PLC 连接中断");
                m_plcStatusLabel->setProperty("tone", "bad");
            }
            else
            {
                m_plcStatusLabel->setText("● PLC 未初始化");
                m_plcStatusLabel->setProperty("tone", "warn");
            }
            m_plcStatusLabel->style()->unpolish(m_plcStatusLabel);
            m_plcStatusLabel->style()->polish(m_plcStatusLabel);
        }

        if (m_stationStatusLabel)
        {
            const int count = m_terminalServer ? m_terminalServer->getConnectedStationCount() : 0;
            m_stationStatusLabel->setText(QString("● Station %1/4").arg(count));
            m_stationStatusLabel->setProperty("tone", count > 0 ? "good" : "bad");
            m_stationStatusLabel->style()->unpolish(m_stationStatusLabel);
            m_stationStatusLabel->style()->polish(m_stationStatusLabel);
        }
    }

    void MainWindow::onConfig()
    {
        auto &config = ConfigManager::getInstance();
        config.loadConfig("config/system.conf");

        QDialog dialog(this);
        dialog.setWindowTitle("测试参数设置");
        dialog.setMinimumSize(560, 480);

        dialog.setStyleSheet(
            "QDialog { background: #0b1220; color: #e2e8f0; }"
            "QLabel { color: #e2e8f0; background: transparent; }"
            "QGroupBox {"
            "  background: #111a2c;"
            "  border: 1px solid #243349;"
            "  border-radius: 8px;"
            "  margin-top: 10px;"
            "  font-weight: 600;"
            "  color: #e2e8f0;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  padding: 0 4px;"
            "  color: #dbeafe;"
            "  background: #111a2c;"
            "}"
            "QLabel[role='unit'] { color: #93c5fd; min-width: 72px; }"
            "QLabel[role='hint'] { color: #93a4bd; font-size: 12px; }"
            "QAbstractSpinBox, QComboBox {"
            "  background: #0f172a;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  padding: 4px 8px;"
            "  min-height: 28px;"
            "  color: #e2e8f0;"
            "}"
            "QAbstractSpinBox:focus, QComboBox:focus {"
            "  border-color: #60a5fa;"
            "  background: #111c33;"
            "}"
            "QLabel[role='preview'] {"
            "  background: #0f172a;"
            "  color: #dbeafe;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  padding: 8px;"
            "  font-family: Consolas;"
            "}"
            "QPushButton {"
            "  min-height: 30px;"
            "  padding: 4px 14px;"
            "  background: #111a2c;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  color: #e2e8f0;"
            "}"
            "QPushButton:hover { background: #17243d; border-color: #60a5fa; }"
        );

        auto *mainLayout = new QVBoxLayout(&dialog);

        auto *titleLabel = new QLabel("测试参数配置", &dialog);
        titleLabel->setStyleSheet("font-size: 18px; font-weight: 700; color: #dbeafe; background: transparent;");
        auto *hintLabel = new QLabel("用于设置当前批次的目标压力、电压、阀门策略与分控台。", &dialog);
        hintLabel->setProperty("role", "hint");
        mainLayout->addWidget(titleLabel);
        mainLayout->addWidget(hintLabel);

        auto *targetGroup = new QGroupBox("目标参数", &dialog);
        auto *targetForm = new QFormLayout(targetGroup);

        auto makeInputWithUnit = [&](QWidget *input, const QString &unitText) {
            auto *rowWidget = new QWidget(&dialog);
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);
            rowLayout->addWidget(input);
            auto *unitLabel = new QLabel(unitText, rowWidget);
            unitLabel->setProperty("role", "unit");
            rowLayout->addWidget(unitLabel);
            rowLayout->addStretch();
            return rowWidget;
        };

        auto *targetPressure = new QDoubleSpinBox(&dialog);
        targetPressure->setRange(0.0, 20.0);
        targetPressure->setDecimals(2);
        targetPressure->setSingleStep(0.05);
        targetPressure->setValue(kPaToKgfCm2(config.getFloat("station.test.target_pressure_kpa", 100.0f)));
        targetForm->addRow("测试压力:", makeInputWithUnit(targetPressure, "kgf/cm^2"));

        auto *targetVoltage = new QDoubleSpinBox(&dialog);
        targetVoltage->setRange(0.0, 500.0);
        targetVoltage->setDecimals(1);
        targetVoltage->setSingleStep(0.5);
        targetVoltage->setValue(config.getFloat("station.test.target_voltage_v", 24.0f));
        targetForm->addRow("测试电压:", makeInputWithUnit(targetVoltage, "V"));

        mainLayout->addWidget(targetGroup);

        auto *strategyGroup = new QGroupBox("执行策略", &dialog);
        auto *strategyForm = new QFormLayout(strategyGroup);

        auto *valveMode = new QComboBox(&dialog);
        valveMode->addItem("常开 (NO)", "NO");
        valveMode->addItem("常闭 (NC)", "NC");
        const QString mode = QString::fromStdString(config.getString("station.test.valve_mode", "NO")).toUpper();
        valveMode->setCurrentIndex(mode == "NC" ? 1 : 0);
        strategyForm->addRow("阀门类型:", valveMode);

        auto *openCount = new QSpinBox(&dialog);
        openCount->setRange(1, 1000000);
        openCount->setValue(config.getInt("station.test.open_count", 10));
        strategyForm->addRow("开阀次数:", makeInputWithUnit(openCount, "次"));

        mainLayout->addWidget(strategyGroup);

        auto *stationGroup = new QGroupBox("分控台", &dialog);
        auto *stationForm = new QFormLayout(stationGroup);

        auto *subStation = new QComboBox(&dialog);
        subStation->addItem("1号分控台", 1);
        subStation->addItem("2号分控台", 2);
        subStation->addItem("3号分控台", 3);
        const int stationId = config.getInt("station.test.sub_station_id", 1);
        int stationIndex = stationId - 1;
        if (stationIndex < 0)
            stationIndex = 0;
        if (stationIndex > 2)
            stationIndex = 2;
        subStation->setCurrentIndex(stationIndex);
        stationForm->addRow("目标工位:", subStation);

        auto *previewLabel = new QLabel(&dialog);
        previewLabel->setProperty("role", "preview");
        previewLabel->setWordWrap(true);
        stationForm->addRow("参数预览:", previewLabel);

        auto updatePreview = [&]() {
            previewLabel->setText(
                QString("Pressure: %1 kgf/cm^2\nVoltage : %2 V\nValve   : %3\nCycles  : %4\nStation : %5")
                    .arg(targetPressure->value(), 0, 'f', 2)
                    .arg(targetVoltage->value(), 0, 'f', 1)
                    .arg(valveMode->currentText())
                    .arg(openCount->value())
                    .arg(subStation->currentText()));
        };

        connect(targetPressure, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [updatePreview](double) { updatePreview(); });
        connect(targetVoltage, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [updatePreview](double) { updatePreview(); });
        connect(openCount, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [updatePreview](int) { updatePreview(); });
        connect(valveMode, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [updatePreview](int) { updatePreview(); });
        connect(subStation, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [updatePreview](int) { updatePreview(); });

        updatePreview();
        mainLayout->addWidget(stationGroup);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        auto *resetBtn = buttons->addButton("恢复默认", QDialogButtonBox::ResetRole);
        buttons->button(QDialogButtonBox::Ok)->setText("保存设置");
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");

        connect(resetBtn, &QPushButton::clicked, &dialog, [=]() {
            targetPressure->setValue(kPaToKgfCm2(100.0));
            targetVoltage->setValue(24.0);
            valveMode->setCurrentIndex(0);
            openCount->setValue(10);
            subStation->setCurrentIndex(0);
        });

        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        mainLayout->addWidget(buttons);

        if (dialog.exec() == QDialog::Accepted)
        {
            config.setFloat("station.test.target_pressure_kpa", static_cast<float>(kgfCm2ToKPa(targetPressure->value())));
            config.setFloat("station.test.target_voltage_v", static_cast<float>(targetVoltage->value()));
            config.setString("station.test.valve_mode", valveMode->currentData().toString().toStdString());
            config.setInt("station.test.open_count", openCount->value());
            config.setInt("station.test.sub_station_id", subStation->currentData().toInt());

            if (config.saveConfig("config/system.conf"))
            {
                statusBar()->showMessage("测试参数已保存", 3000);
            }
            else
            {
                QMessageBox::warning(this, "保存失败", "无法写入 config/system.conf，请检查文件权限。");
            }
        }
    }

    void MainWindow::onAbout()
    {
        QMessageBox::about(this, "关于本软件",
                           "<h3>水质测试系统 v2.0</h3>"
                           "<p>Terminal-Station 分布式架构</p>"
                           "<p>与西门子 S7-1200 PLC 兼容</p>"
                           "<p>用于实时监控和控制水质测试设备</p>");
    }

    void MainWindow::onPLCConnected()
    {
        if (m_plcStatusLabel)
        {
            m_plcStatusLabel->setText("● PLC 已连接");
            m_plcStatusLabel->setProperty("tone", "good");
            m_plcStatusLabel->style()->unpolish(m_plcStatusLabel);
            m_plcStatusLabel->style()->polish(m_plcStatusLabel);
        }
    }

    void MainWindow::onPLCDisconnected()
    {
        if (m_plcStatusLabel)
        {
            m_plcStatusLabel->setText("�?PLC 断开");
            m_plcStatusLabel->setProperty("tone", "bad");
            m_plcStatusLabel->style()->unpolish(m_plcStatusLabel);
            m_plcStatusLabel->style()->polish(m_plcStatusLabel);
        }
    }

    void MainWindow::onPLCError(const QString &error)
    {
        if (m_mode == WindowMode::TERMINAL_MODE)
        {
            statusBar()->showMessage(QString("PLC错误: %1").arg(error), 5000);
            onPLCDisconnected();
            return;
        }

        QMessageBox::warning(this, "PLC错误", error);
    }

    void MainWindow::onStationConnected(uint8_t stationId)
    {
        Q_UNUSED(stationId);
        if (m_terminalServer && m_stationStatusLabel)
        {
            const int count = m_terminalServer->getConnectedStationCount();
            m_stationStatusLabel->setText(QString("�?Station %1/4").arg(count));
            m_stationStatusLabel->setProperty("tone", count > 0 ? "good" : "bad");
            m_stationStatusLabel->style()->unpolish(m_stationStatusLabel);
            m_stationStatusLabel->style()->polish(m_stationStatusLabel);
        }
    }

    void MainWindow::onStationDisconnected(uint8_t stationId)
    {
        Q_UNUSED(stationId);
        if (m_terminalServer && m_stationStatusLabel)
        {
            const int count = m_terminalServer->getConnectedStationCount();
            m_stationStatusLabel->setText(QString("�?Station %1/4").arg(count));
            m_stationStatusLabel->setProperty("tone", count > 0 ? "good" : "bad");
            m_stationStatusLabel->style()->unpolish(m_stationStatusLabel);
            m_stationStatusLabel->style()->polish(m_stationStatusLabel);
        }
    }

    void MainWindow::onDataReceived(const struct SensorData &data)
    {
        if (m_dataStatsLabel)
        {
            m_dataStatsLabel->setText(
                QString("采样: P1=%1 kgf/cm^2, F=%2 | Ts=%3")
                    .arg(kPaToKgfCm2(data.pressure[0]), 0, 'f', 2)
                    .arg(data.flow_rate, 0, 'f', 2)
                    .arg(data.timestamp));
        }
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
