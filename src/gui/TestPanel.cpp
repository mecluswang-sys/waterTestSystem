/**
 * @file TestPanel.cpp
 * @brief 测试区面板实现（简化版）
 */

#include "gui/TestPanel.h"
#include "DeviceManager.h"
#include "StationClient.h"
#include "ConfigManager.h"
#include <QMessageBox>
#include <QScrollBar>
#include <QHeaderView>
#include <QDateTime>

namespace WaterTest
{

    namespace
    {
        QString flowUnitText(const FlowMeter &meter)
        {
            if (!meter.unitLabel.empty() && meter.unitLabel != "unknown")
                return QString::fromStdString(meter.unitLabel);
            return "L/min";
        }

        QString flowAlarmText(const FlowMeter &meter)
        {
            QStringList alarms;
            if (meter.emptyPipeAlarm != 0)
                alarms << "空管";
            if (meter.excitationAlarm != 0)
                alarms << "励磁";
            return alarms.isEmpty() ? "正常" : alarms.join("/");
        }

        int pressureDisplayDecimals(const PressureSensor &sensor, int fallbackDecimals)
        {
            if (sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6)
                return sensor.displayDecimals;
            return fallbackDecimals;
        }

        QString fmtKPa(const PressureSensor &sensor, int fallbackDecimals = 2)
        {
            return QString::number(sensor.pressure, 'f', pressureDisplayDecimals(sensor, fallbackDecimals)) + " kPa";
        }
    }

    TestPanel::TestPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent)
        : QWidget(parent), m_deviceManager(deviceManager), m_updateTimer(nullptr), m_isTesting(false), m_testTimeSeconds(0)
    {
        setupUI();

        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &TestPanel::onUpdateData);
    }

    void TestPanel::setStationClient(std::shared_ptr<StationClient> stationClient)
    {
        m_stationClient = stationClient;
    }

    TestPanel::~TestPanel()
    {
        // Disconnect all signals before destroying
        if (m_updateTimer)
        {
            disconnect(m_updateTimer, nullptr, this, nullptr);
            m_updateTimer->stop();
        }
        // Clear device manager reference to prevent accessing destroyed object
        m_deviceManager.reset();
    }

    void TestPanel::setupUI()
    {
        auto *mainLayout = new QVBoxLayout(this);

        // ========== 顶部总览块（动态数值） ==========
        m_topOverviewGroup = new QGroupBox("测试链路概要", this);
        auto *topLayout = new QHBoxLayout(m_topOverviewGroup);

        auto makeBlock = [&](const QString &title)
        {
            auto *lbl = new QLabel(title + "\n--", this);
            lbl->setMinimumWidth(160);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setProperty("role", "metricBlock");
            return lbl;
        };

        m_pumpBlock = makeBlock("增压泵");
        m_p1t1Block = makeBlock("P1/T1");
        m_valve1Block = makeBlock("阀1");
        m_p2t2Block = makeBlock("P2/T2");
        m_flowBlock = makeBlock("流量计");
        m_valve2Block = makeBlock("阀2");
        m_p3t3Block = makeBlock("P3/T3");
        m_dutValveBlock = makeBlock("待测阀");
        m_p4t4Block = makeBlock("P4/T4");

        for (auto *w : {m_pumpBlock, m_p1t1Block, m_valve1Block, m_p2t2Block, m_flowBlock, m_valve2Block, m_p3t3Block, m_dutValveBlock, m_p4t4Block})
            topLayout->addWidget(w);

        mainLayout->addWidget(m_topOverviewGroup);

        // ========== 流程图区域 ==========
        m_flowDiagramGroup = new QGroupBox("测试管路流程", this);
        auto *flowLayout = new QVBoxLayout(m_flowDiagramGroup);

        createFlowDiagram();

        flowLayout->addWidget(m_flowDiagramLabel);
        mainLayout->addWidget(m_flowDiagramGroup);

        // 设备状态区域已移除，避免出现空白区域

        // ========== 电磁阀控制(D0.0 / D0.1 / D0.2) ==========
        m_solenoidGroup = new QGroupBox("电磁阀状态（只读）(M100.0 / Q0.1 / Q0.2)", this);
        auto *solenoidLayout = new QHBoxLayout(m_solenoidGroup);

        auto styleBtn = [](QPushButton *btn, const QString &label, bool on)
        {
            if (!btn)
                return;
            btn->setText(QString("%1 (%2)").arg(label).arg(on ? "通" : "断"));
            btn->setProperty("tone", on ? "good" : "neutral");
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        };

        auto makeSolenoidBtn = [&](const QString &label, uint8_t index)
        {
            auto *btn = new QPushButton(QString("%1 (断)").arg(label), this);
            btn->setMinimumHeight(36);
            btn->setProperty("relayOn", false);
            btn->setToolTip(QString("%1仅用于显示实时状态，控制请在1号操作台执行").arg(label));
            btn->setEnabled(false);
            solenoidLayout->addWidget(btn);
            return btn;
        };

        m_solenoid0Btn = makeSolenoidBtn("阀1", 0);
        m_solenoid1Btn = makeSolenoidBtn("阀2", 1);
        m_solenoid2Btn = makeSolenoidBtn("待测阀", 2);

        mainLayout->addWidget(m_solenoidGroup);

        // ========== 控制按钮区 ==========
        auto *controlGroup = new QGroupBox("测试控制", this);
        auto *controlLayout = new QHBoxLayout(controlGroup);

        m_startTestBtn = new QPushButton("开始测试", this);
        m_startTestBtn->setProperty("tone", "good");
        m_startTestBtn->setProperty("size", "xl");
        m_startTestBtn->setMinimumHeight(50);
        connect(m_startTestBtn, &QPushButton::clicked, this, &TestPanel::onStartTest);
        controlLayout->addWidget(m_startTestBtn);

        m_stopTestBtn = new QPushButton("停止测试", this);
        m_stopTestBtn->setProperty("tone", "warn");
        m_stopTestBtn->setProperty("size", "xl");
        m_stopTestBtn->setMinimumHeight(50);
        m_stopTestBtn->setEnabled(false);
        connect(m_stopTestBtn, &QPushButton::clicked, this, &TestPanel::onStopTest);
        controlLayout->addWidget(m_stopTestBtn);

        m_emergencyStopBtn = new QPushButton("紧急停止", this);
        m_emergencyStopBtn->setProperty("tone", "bad");
        m_emergencyStopBtn->setProperty("size", "xl");
        m_emergencyStopBtn->setMinimumHeight(50);
        connect(m_emergencyStopBtn, &QPushButton::clicked, this, &TestPanel::onEmergencyStop);
        controlLayout->addWidget(m_emergencyStopBtn);

        mainLayout->addWidget(controlGroup);

        // ========== 状态信息 ==========
        auto *infoLayout = new QHBoxLayout();

        m_statusLabel = new QLabel("状态: 等待测试", this);
        m_statusLabel->setProperty("role", "statusBox");
        m_statusLabel->setProperty("tone", "muted");
        infoLayout->addWidget(m_statusLabel, 2);

        m_testTimeLabel = new QLabel("测试时间: 0秒", this);
        m_testTimeLabel->setProperty("role", "statusBox");
        m_testTimeLabel->setProperty("tone", "muted");
        infoLayout->addWidget(m_testTimeLabel, 1);

        mainLayout->addLayout(infoLayout);

        // （已移动）电磁阀控制已作为独立分组置于“测试控制”之上
        // ========== 数据保存控制 ==========
        auto *dataLogGroup = new QGroupBox("数据保存", this);
        auto *dataLogLayout = new QHBoxLayout(dataLogGroup);

        m_enableDataLogBtn = new QPushButton("启用数据保存", this);
        m_enableDataLogBtn->setProperty("tone", "good");
        m_enableDataLogBtn->setMinimumHeight(36);
        m_enableDataLogBtn->setCheckable(true);
        connect(m_enableDataLogBtn, &QPushButton::clicked, this, &TestPanel::onToggleDataLogging);
        dataLogLayout->addWidget(m_enableDataLogBtn);

        m_exportDataBtn = new QPushButton("导出数据为CSV", this);
        m_exportDataBtn->setProperty("tone", "neutral");
        m_exportDataBtn->setMinimumHeight(36);
        connect(m_exportDataBtn, &QPushButton::clicked, this, &TestPanel::onExportData);
        dataLogLayout->addWidget(m_exportDataBtn);

        m_recordCountLabel = new QLabel("已保存: 0 条记录", this);
        m_recordCountLabel->setProperty("role", "statusBox");
        m_recordCountLabel->setProperty("tone", "muted");
        m_recordCountLabel->setMinimumWidth(150);
        m_recordCountLabel->setAlignment(Qt::AlignCenter);
        dataLogLayout->addWidget(m_recordCountLabel);

        mainLayout->addWidget(dataLogGroup);
        // ========== 测试记录区域 ==========
        initTestRecordUI();
        mainLayout->addWidget(m_testRecordGroup);
    }

    void TestPanel::createFlowDiagram()
    {
        m_flowDiagramLabel = new QLabel(this);
        m_flowDiagramLabel->setWordWrap(true);
        m_flowDiagramLabel->setProperty("role", "flowDiagram");

        const QString arrow = " → ";
        QString flowText = QString(
                               "<div>"
                               "<b>测试管路流程图：</b><br/><br/>"
                               "分水罐%1电动阀2%1<i>消声止回阀</i>%1压力传感器2%1电动阀3%1流量计1<br/>"
                               "%1电动阀4%1<i>压力表</i>%1<i>消声止回阀</i>%1压力传感器3%1电动阀5<br/>"
                               "%1压力传感器4%1<b>电动三通切换阀</b>%1电动阀6%1调压阀1<br/>"
                               "%1压力传感器5%1<b>温度传感器</b>%1<b>待测试阀</b>%1压力传感器6<br/>"
                               "%1流量计2%1电动阀7%1调压阀2<br/><br/>"
                               "<i style='font-size: 9pt;'>注：斜体表示物理设备（不受PLC控制）</i>"
                               "</div>")
                               .arg(arrow);

        m_flowDiagramLabel->setText(flowText);
    }

    void TestPanel::startUpdate(int intervalMs)
    {
        Q_UNUSED(intervalMs);
        // 测试区实时刷新在代码层强制关闭，避免多界面轮询与状态竞争。
        if (m_updateTimer && m_updateTimer->isActive())
        {
            m_updateTimer->stop();
        }
    }

    void TestPanel::stopUpdate()
    {
        if (m_updateTimer && m_updateTimer->isActive())
        {
            m_updateTimer->stop();
        }
    }

    void TestPanel::onUpdateData()
    {
        if (!m_deviceManager)
        {
            return;
        }

        updateTopOverview();
        updateFlowDiagramDynamic();
        updateDeviceStatus();
        updateSolenoidStates();
        updateDataLogStatus();

        // 如果正在测试，更新测试时间
        if (m_isTesting)
        {
            m_testTimeSeconds++;
            m_testTimeLabel->setText(QString("测试时间: %1秒").arg(m_testTimeSeconds));
        }
    }

    static QString fmtC(double c)
    {
        return QString::number(c, 'f', 2) + " ℃";
    }

    static QString fmtValve(ValveStatus s)
    {
        if (s == ValveStatus::OPEN || s == ValveStatus::OPENING)
            return "开";
        if (s == ValveStatus::FAULT)
            return "故障";
        return "关";
    }

    void TestPanel::updateTopOverview()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            const SensorData net = m_stationClient->getLatestSensorData();
            m_pumpBlock->setText("增压泵\n远程模式");
            m_p1t1Block->setText(QString("P1/T1\n%1  --").arg(QString::number(net.pressure[0], 'f', 2) + " kPa"));
            m_valve1Block->setText("阀1\n远程控制");
            m_p2t2Block->setText(QString("P2/T2\n%1  --").arg(QString::number(net.pressure[1], 'f', 2) + " kPa"));
            m_flowBlock->setText(QString("流量计\n%1 L/min\n网络数据")
                                     .arg(QString::number(net.flow_rate, 'f', 2)));
            m_valve2Block->setText("阀2\n远程控制");
            m_p3t3Block->setText(QString("P3/T3\n%1  --").arg(QString::number(net.pressure[2], 'f', 2) + " kPa"));
            m_dutValveBlock->setText("待测阀\n远程控制");
            m_p4t4Block->setText(QString("P4/T4\n%1  --").arg(QString::number(net.pressure[3], 'f', 2) + " kPa"));
            return;
        }

        auto pump = m_deviceManager->getPump(1);
        auto v1 = m_deviceManager->getValve(1);
        auto v2 = m_deviceManager->getValve(2);
        auto vDut = m_deviceManager->getValve(3); // 待测试阀暂定ID=3，如有不同请调整
        auto fm = m_deviceManager->getFlowMeter(1);

        auto p1 = m_deviceManager->getPressureSensor(1);
        auto p2 = m_deviceManager->getPressureSensor(2);
        auto p3 = m_deviceManager->getPressureSensor(3);
        auto p4 = m_deviceManager->getPressureSensor(4);

        auto t1 = m_deviceManager->getTemperatureSensor(1);
        auto t2 = m_deviceManager->getTemperatureSensor(2);
        auto t3 = m_deviceManager->getTemperatureSensor(3);
        auto t4 = m_deviceManager->getTemperatureSensor(4);

        m_pumpBlock->setText(QString("增压泵\n%1  %2Hz")
                                 .arg(pump.isRunning ? "运行" : "停止")
                                 .arg(QString::number(pump.frequency, 'f', 1)));

        m_p1t1Block->setText(QString("P1/T1\n%1  %2")
                                 .arg(fmtKPa(p1))
                                 .arg(fmtC(t1.temperature)));
        bool d0 = false, d1 = false, d2 = false;
        m_deviceManager->getRelayState(0, d0);
        m_deviceManager->getRelayState(1, d1);
        m_deviceManager->getRelayState(2, d2);

        m_valve1Block->setText(QString("阀1\n%1  A:%2")
                                   .arg(fmtValve(v1.status))
                                   .arg(d0 ? "通" : "断"));
        m_p2t2Block->setText(QString("P2/T2\n%1  %2")
                                 .arg(fmtKPa(p2))
                                 .arg(fmtC(t2.temperature)));
        m_flowBlock->setText(QString("流量计\n%1 %2\n%3")
                     .arg(QString::number(fm.flowRate, 'f', 2))
                     .arg(flowUnitText(fm))
                     .arg(flowAlarmText(fm)));
        m_valve2Block->setText(QString("阀2\n%1  B:%2")
                                   .arg(fmtValve(v2.status))
                                   .arg(d1 ? "通" : "断"));
        m_p3t3Block->setText(QString("P3/T3\n%1  %2")
                                 .arg(fmtKPa(p3))
                                 .arg(fmtC(t3.temperature)));
        m_dutValveBlock->setText(QString("待测阀\n%1  C:%2")
                                     .arg(fmtValve(vDut.status))
                                     .arg(d2 ? "通" : "断"));
        m_p4t4Block->setText(QString("P4/T4\n%1  %2")
                                 .arg(fmtKPa(p4))
                                 .arg(fmtC(t4.temperature)));
    }

    void TestPanel::updateFlowDiagramDynamic()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            const SensorData net = m_stationClient->getLatestSensorData();
            const QString line1 = QString("远程模式：主控机统一采集/控制\nP1:%1  P2:%2")
                                      .arg(QString::number(net.pressure[0], 'f', 2) + " kPa")
                                      .arg(QString::number(net.pressure[1], 'f', 2) + " kPa");
            const QString line2 = QString("P3:%1  P4:%2  流量:%3 L/min")
                                      .arg(QString::number(net.pressure[2], 'f', 2) + " kPa")
                                      .arg(QString::number(net.pressure[3], 'f', 2) + " kPa")
                                      .arg(QString::number(net.flow_rate, 'f', 2));
            m_flowDiagramLabel->setText(line1 + "\n" + line2);
            return;
        }

        // 用与准备区一致的动态两行文本方案
        bool d0 = false, d1 = false, d2 = false;
        m_deviceManager->getRelayState(0, d0);
        m_deviceManager->getRelayState(1, d1);
        m_deviceManager->getRelayState(2, d2);
        auto p1 = m_deviceManager->getPressureSensor(1);
        auto p2 = m_deviceManager->getPressureSensor(2);
        auto p3 = m_deviceManager->getPressureSensor(3);
        auto p4 = m_deviceManager->getPressureSensor(4);
        auto t1 = m_deviceManager->getTemperatureSensor(1);
        auto t2 = m_deviceManager->getTemperatureSensor(2);
        auto t3 = m_deviceManager->getTemperatureSensor(3);
        auto t4 = m_deviceManager->getTemperatureSensor(4);
        auto v1 = m_deviceManager->getValve(1);
        auto v2 = m_deviceManager->getValve(2);
        auto vDUT = m_deviceManager->getValve(3);
        auto fm = m_deviceManager->getFlowMeter(1);
        auto pump = m_deviceManager->getPump(1);

        QString line1 = QString("增压泵[%1]  →  传感器1[P:%2  T:%3]  →  阀1[%4; A:%5]  →  传感器2[P:%6  T:%7]\n")
                            .arg(pump.isRunning ? "运行" : "停止")
                            .arg(fmtKPa(p1))
                            .arg(fmtC(t1.temperature))
                            .arg(fmtValve(v1.status))
                            .arg(d0 ? "通" : "断")
                            .arg(fmtKPa(p2))
                            .arg(fmtC(t2.temperature));

        QString line2 = QString("流量计[%1 %2, 报警:%3]  →  阀2[%4; B:%5]  →  传感器3[P:%6  T:%7]  →  待测阀[%8; C:%9]  →  传感器4[P:%10  T:%11]")
                    .arg(QString::number(fm.flowRate, 'f', 2))
                    .arg(flowUnitText(fm))
                    .arg(flowAlarmText(fm))
                            .arg(fmtValve(v2.status))
                            .arg(d1 ? "通" : "断")
                            .arg(fmtKPa(p3))
                            .arg(fmtC(t3.temperature))
                            .arg(fmtValve(vDUT.status))
                            .arg(d2 ? "通" : "断")
                            .arg(fmtKPa(p4))
                            .arg(fmtC(t4.temperature));

        m_flowDiagramLabel->setText(line1 + "\n" + line2);
    }

    void TestPanel::initTestRecordUI()
    {
        m_testRecordGroup = new QGroupBox("测试记录", this);
        m_testRecordGroup->setStyleSheet("QGroupBox { font-size: 12pt; font-weight: bold; }");
        auto *layout = new QVBoxLayout(m_testRecordGroup);

        m_testRecordTable = new QTableWidget(this);
        m_testRecordTable->setColumnCount(13);
        QStringList headers;
        headers << "时间" << "事件"
                << "P1(kPa)" << "T1(℃)"
                << "P2(kPa)" << "T2(℃)"
                << "P3(kPa)" << "T3(℃)"
                << "P4(kPa)" << "T4(℃)"
                << "阀1" << "阀2" << "待测阀";
        m_testRecordTable->setHorizontalHeaderLabels(headers);
        m_testRecordTable->horizontalHeader()->setStretchLastSection(true);
        m_testRecordTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);
        m_testRecordTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_testRecordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_testRecordTable->setMinimumHeight(220);
        m_testRecordTable->setStyleSheet("QTableWidget { font-size: 12pt; } QHeaderView::section { font-size: 12pt; font-weight: bold; }");

        layout->addWidget(m_testRecordTable);
        // 默认显示测试记录区
        m_testRecordGroup->setVisible(true);
    }

    void TestPanel::appendTestRecord(const QString &event)
    {
        if (!m_deviceManager || !m_testRecordTable)
            return;

        auto p1 = m_deviceManager->getPressureSensor(1);
        auto p2 = m_deviceManager->getPressureSensor(2);
        auto p3 = m_deviceManager->getPressureSensor(3);
        auto p4 = m_deviceManager->getPressureSensor(4);
        auto t1 = m_deviceManager->getTemperatureSensor(1);
        auto t2 = m_deviceManager->getTemperatureSensor(2);
        auto t3 = m_deviceManager->getTemperatureSensor(3);
        auto t4 = m_deviceManager->getTemperatureSensor(4);
        auto v1 = m_deviceManager->getValve(1);
        auto v2 = m_deviceManager->getValve(2);
        auto vDUT = m_deviceManager->getValve(3);

        int row = m_testRecordTable->rowCount();
        m_testRecordTable->insertRow(row);
        if (!m_testRecordGroup->isVisible())
        {
            m_testRecordGroup->setVisible(true);
        }

        auto set = [&](int col, const QString &text)
        {
            auto *item = new QTableWidgetItem(text);
            m_testRecordTable->setItem(row, col, item);
        };

        set(0, QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        set(1, event);
        set(2, fmtKPa(p1));
        set(3, fmtC(t1.temperature));
        set(4, fmtKPa(p2));
        set(5, fmtC(t2.temperature));
        set(6, fmtKPa(p3));
        set(7, fmtC(t3.temperature));
        set(8, fmtKPa(p4));
        set(9, fmtC(t4.temperature));
        set(10, fmtValve(v1.status));
        set(11, fmtValve(v2.status));
        set(12, fmtValve(vDUT.status));

        m_testRecordTable->scrollToBottom();
    }

    void TestPanel::updateDeviceStatus()
    {
        // 当前状态区控件已移除显示，避免解引用空/未初始化指针，这里保持空实现
        return;
    }

    void TestPanel::updateSolenoidStates()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            return;
        }

        if (!m_deviceManager)
            return;
        auto apply = [](QPushButton *btn, const QString &label, bool on)
        {
            if (!btn)
                return;
            btn->setText(QString("%1 (%2)").arg(label).arg(on ? "通" : "断"));
            btn->setProperty("relayOn", on);
            btn->setProperty("tone", on ? "good" : "neutral");
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        };
        bool on = false;
        if (m_deviceManager->getRelayState(0, on))
            apply(m_solenoid0Btn, "阀1", on);
        if (m_deviceManager->getRelayState(1, on))
            apply(m_solenoid1Btn, "阀2", on);
        if (m_deviceManager->getRelayState(2, on))
            apply(m_solenoid2Btn, "待测阀", on);
    }

    void TestPanel::updateValveStatus(int valveId, QLabel *statusLabel)
    {
        if (!statusLabel)
            return;

        auto valve = m_deviceManager->getValve(valveId);
        if (valve.status == ValveStatus::OPEN || valve.status == ValveStatus::OPENING)
        {
            statusLabel->setText("开启");
            statusLabel->setProperty("role", "valueBox");
            statusLabel->setProperty("tone", "good");
        }
        else
        {
            statusLabel->setText("关闭");
            statusLabel->setProperty("role", "valueBox");
            statusLabel->setProperty("tone", "muted");
        }

        statusLabel->style()->unpolish(statusLabel);
        statusLabel->style()->polish(statusLabel);
    }

    void TestPanel::updatePressureSensor(int sensorId, QLabel *valueLabel)
    {
        if (!valueLabel)
            return;

        auto sensor = m_deviceManager->getPressureSensor(sensorId);
        const double pressureKPa = static_cast<double>(sensor.pressure);
        valueLabel->setText(QString("%1 kPa").arg(pressureKPa, 0, 'f', pressureDisplayDecimals(sensor, 1)));

        // 根据压力值设置颜色
        if (pressureKPa > 800.0)
        {
            valueLabel->setProperty("role", "valueBox");
            valueLabel->setProperty("tone", "bad");
        }
        else if (pressureKPa > 10.0)
        {
            valueLabel->setProperty("role", "valueBox");
            valueLabel->setProperty("tone", "good");
        }
        else
        {
            valueLabel->setProperty("role", "valueBox");
            valueLabel->setProperty("tone", "muted");
        }

        valueLabel->style()->unpolish(valueLabel);
        valueLabel->style()->polish(valueLabel);
    }

    void TestPanel::onStartTest()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (!m_deviceManager && !(m_stationClient && strictRemoteMode))
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        auto reply = QMessageBox::question(this, "确认",
                                           "确定开始测试吗？\n\n这将打开测试管路的所有必要阀门",
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }

        m_isTesting = true;
        m_testTimeSeconds = 0;

        if (m_stationClient && strictRemoteMode)
        {
            ControlCommand cmd;
            cmd.command_type = 2; // valve
            cmd.index = 1;        // valve id 2 -> index 1
            cmd.action = 1;
            if (!m_stationClient->sendCommand(cmd))
            {
                QMessageBox::warning(this, "错误", "发送开始测试命令失败（主控通信异常）");
                m_isTesting = false;
                return;
            }
        }
        else
        {
            // 打开测试管路的关键阀门
            m_deviceManager->controlValve(2, true); // 分水罐出口阀
        }

        // 更新UI
        m_startTestBtn->setEnabled(false);
        m_stopTestBtn->setEnabled(true);
        m_statusLabel->setText("状态: 测试进行中...");
        m_statusLabel->setProperty("tone", "good");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);

        appendTestRecord("开始测试");
    }

    void TestPanel::onStopTest()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (!m_deviceManager && !(m_stationClient && strictRemoteMode))
        {
            return;
        }

        m_isTesting = false;

        if (m_stationClient && strictRemoteMode)
        {
            for (int i = 2; i <= 11; i++)
            {
                ControlCommand cmd;
                cmd.command_type = 2; // valve
                cmd.index = static_cast<uint8_t>(i - 1);
                cmd.action = 0;
                (void)m_stationClient->sendCommand(cmd);
            }
        }
        else
        {
            // 关闭所有测试阀门
            for (int i = 2; i <= 11; i++)
            {
                m_deviceManager->controlValve(i, false);
            }
        }

        // 更新UI
        m_startTestBtn->setEnabled(true);
        m_stopTestBtn->setEnabled(false);
        m_statusLabel->setText("状态: 测试已停止");
        m_statusLabel->setProperty("tone", "muted");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);

        appendTestRecord("停止测试");
    }

    void TestPanel::onEmergencyStop()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (!m_deviceManager && !(m_stationClient && strictRemoteMode))
        {
            return;
        }

        auto reply = QMessageBox::warning(this, "紧急停止",
                                          "确定执行紧急停止吗？\n这将立即停止所有设备！",
                                          QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }

        m_isTesting = false;

        if (m_stationClient && strictRemoteMode)
        {
            for (int i = 0; i <= 2; ++i)
            {
                ControlCommand relayCmd;
                relayCmd.command_type = 0;
                relayCmd.index = static_cast<uint8_t>(i);
                relayCmd.action = 0;
                (void)m_stationClient->sendCommand(relayCmd);
            }
            ControlCommand pumpCmd;
            pumpCmd.command_type = 1;
            pumpCmd.index = 0;
            pumpCmd.action = 0;
            (void)m_stationClient->sendCommand(pumpCmd);
        }
        else
        {
            // 紧急停止
            m_deviceManager->emergencyStop();
        }

        // 更新UI
        m_startTestBtn->setEnabled(true);
        m_stopTestBtn->setEnabled(false);
        m_statusLabel->setText("状态: 紧急停止");
        m_statusLabel->setProperty("tone", "bad");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);

        appendTestRecord("紧急停止");
    }

    void TestPanel::onToggleDataLogging()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        bool enabled = m_enableDataLogBtn->isChecked();
        m_deviceManager->setDataLoggingEnabled(enabled);

        if (enabled)
        {
            m_enableDataLogBtn->setText("禁用数据保存");
            m_enableDataLogBtn->setProperty("tone", "warn");
            appendTestRecord("数据保存已启用");
        }
        else
        {
            m_enableDataLogBtn->setText("启用数据保存");
            m_enableDataLogBtn->setProperty("tone", "good");
            appendTestRecord("数据保存已禁用");
        }

        m_enableDataLogBtn->style()->unpolish(m_enableDataLogBtn);
        m_enableDataLogBtn->style()->polish(m_enableDataLogBtn);
    }

    void TestPanel::onExportData()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        QString logDir = QString::fromStdString(m_deviceManager->getLogDirectory());
        if (logDir.isEmpty())
        {
            QMessageBox::warning(this, "错误", "日志目录为空，请先启用数据保存");
            return;
        }

        // 生成CSV文件名：sensor_data_YYYYMMDD_HHMMSS.csv
        QDateTime now = QDateTime::currentDateTime();
        QString csvFileName = logDir + "/sensor_data_" + now.toString("yyyyMMdd_hhmmss") + ".csv";

        if (m_deviceManager->exportDataToCSV(csvFileName.toStdString()))
        {
            QMessageBox::information(this, "导出成功", 
                                    "数据已导出到:\n" + csvFileName);
            appendTestRecord("导出数据到CSV: " + csvFileName);
        }
        else
        {
            QMessageBox::warning(this, "导出失败", "导出数据为CSV格式失败");
        }
    }

    void TestPanel::updateDataLogStatus()
    {
        if (!m_deviceManager)
        {
            m_recordCountLabel->setText("已保存: -- 条记录");
            return;
        }

        int recordCount = m_deviceManager->getDataRecordCount();
        m_recordCountLabel->setText(QString("已保存: %1 条记录").arg(recordCount));

        // 根据是否启用了数据保存来更新按钮状态
        if (m_deviceManager->isDataLoggingEnabled())
        {
            m_exportDataBtn->setEnabled(recordCount > 0);
        }
    }

} // namespace WaterTest
