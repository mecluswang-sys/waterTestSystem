/**
 * @file MonitorPanel.cpp
 * @brief Monitor Panel Implementation - Simplified Version (Fixed)
 */

#include "gui/MonitorPanel.h"
#include "DeviceManager.h"
#include "DeviceTypes.h"
#include "ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QGroupBox>
#include <QGridLayout>
#include <QDateTime>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace WaterTest
{

    namespace
    {
        int pressureDisplayDecimals(const PressureSensor &sensor, int fallbackDecimals = 2)
        {
            if (sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6)
                return sensor.displayDecimals;
            return fallbackDecimals;
        }

        void appendPressureUiDebugLog(const std::string &message)
        {
            std::filesystem::create_directories("deploy/logs");

            std::ofstream logFile("deploy/logs/pressure_ui_debug.log", std::ios::app);
            if (!logFile.is_open())
            {
                return;
            }

            logFile << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString()
                    << ' ' << message << '\n';
        }
    }

    MonitorPanel::MonitorPanel(std::shared_ptr<DeviceManager> deviceMgr, QWidget *parent)
        : QWidget(parent), m_deviceManager(deviceMgr), m_updateTimer(nullptr)
    {
        setupUI();

        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &MonitorPanel::updateDisplay);
    }

    MonitorPanel::~MonitorPanel()
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

    void MonitorPanel::setupUI()
    {
        auto &cfg = ConfigManager::getInstance();
        const bool showTemperatureColumn = cfg.getInt("temp.count", 1) > 0;

        QGridLayout *mainLayout = new QGridLayout(this);
        // 统一主布局的边距与间距，使整体更紧凑
        mainLayout->setContentsMargins(8, 8, 8, 8);
        mainLayout->setSpacing(8);

        // 传感器监控（压力+温度合并为同一行块）
        QGroupBox *sensorGroup = new QGroupBox("压力/温度传感器", this);
        sensorGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *sensorLayout = new QVBoxLayout(sensorGroup);
        sensorLayout->setContentsMargins(4, 4, 4, 4);
        sensorLayout->setSpacing(4);

        m_sensorTable = new QTableWidget(0, 5, this);
        m_sensorTable->setHorizontalHeaderLabels({"编号", "名称", "压力 (kPa)", "温度 (℃)", "状态"});
        m_sensorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_sensorTable->horizontalHeader()->setMinimumSectionSize(70);
        m_sensorTable->horizontalHeader()->setDefaultSectionSize(100);
        m_sensorTable->setColumnWidth(1, 150);
        // 让“状态”列填充剩余空间，减少表内空白
        m_sensorTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        m_sensorTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_sensorTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_sensorTable->verticalHeader()->setVisible(false);
        m_sensorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_sensorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        if (!showTemperatureColumn)
        {
            m_sensorTable->setColumnHidden(3, true);
        }
        sensorLayout->addWidget(m_sensorTable);

        // 放置到网格：第0行第0列
        mainLayout->addWidget(sensorGroup, 0, 0);

        // 流量计组
        QGroupBox *flowGroup = new QGroupBox("流量计", this);
        flowGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *flowLayout = new QVBoxLayout(flowGroup);
        flowLayout->setContentsMargins(4, 4, 4, 4);
        flowLayout->setSpacing(4);

        m_flowTable = new QTableWidget(0, 4, this);
        m_flowTable->setHorizontalHeaderLabels({"编号", "名称", "流量", "状态"});
        m_flowTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_flowTable->horizontalHeader()->setMinimumSectionSize(70);
        m_flowTable->horizontalHeader()->setDefaultSectionSize(100);
        m_flowTable->setColumnWidth(1, 150);
        // 让“状态”列填充剩余空间
        m_flowTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        m_flowTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_flowTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_flowTable->verticalHeader()->setVisible(false);
        m_flowTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_flowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        flowLayout->addWidget(m_flowTable);

        // 放置到网格：第0行第1列
        mainLayout->addWidget(flowGroup, 0, 1);

        // 电动阀组
        QGroupBox *valveGroup = new QGroupBox("电动阀", this);
        valveGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *valveLayout = new QVBoxLayout(valveGroup);
        valveLayout->setContentsMargins(4, 4, 4, 4);
        valveLayout->setSpacing(4);

        m_valveTable = new QTableWidget(0, 6, this);
        m_valveTable->setHorizontalHeaderLabels({"编号", "名称", "开度 (%)", "AO(raw)", "控制电流 (mA)", "状态"});
        m_valveTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_valveTable->horizontalHeader()->setMinimumSectionSize(70);
        m_valveTable->horizontalHeader()->setDefaultSectionSize(100);
        m_valveTable->setColumnWidth(1, 150);
        m_valveTable->setColumnWidth(3, 90);
        m_valveTable->setColumnWidth(4, 110);
        // 让“状态”列填充剩余空间
        m_valveTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
        m_valveTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_valveTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_valveTable->verticalHeader()->setVisible(false);
        m_valveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_valveTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        valveLayout->addWidget(m_valveTable);

        mainLayout->addWidget(valveGroup, 0, 2);

        // 变频泵组
        QGroupBox *pumpGroup = new QGroupBox("变频泵", this);
        pumpGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *pumpLayout = new QVBoxLayout(pumpGroup);
        pumpLayout->setContentsMargins(4, 4, 4, 4);
        pumpLayout->setSpacing(4);

        m_pumpTable = new QTableWidget(0, 4, this);
        m_pumpTable->setHorizontalHeaderLabels({"编号", "名称", "频率 (Hz)", "状态"});
        m_pumpTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_pumpTable->horizontalHeader()->setMinimumSectionSize(70);
        m_pumpTable->horizontalHeader()->setDefaultSectionSize(100);
        m_pumpTable->setColumnWidth(1, 150);
        // 让“状态”列填充剩余空间
        m_pumpTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        m_pumpTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_pumpTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_pumpTable->verticalHeader()->setVisible(false);
        m_pumpTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pumpTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        pumpLayout->addWidget(m_pumpTable);

        mainLayout->addWidget(pumpGroup, 1, 0);

        // 列/行拉伸，使两列均分宽度
        mainLayout->setColumnStretch(0, 1);
        mainLayout->setColumnStretch(1, 1);
        mainLayout->setRowStretch(0, 1);
        mainLayout->setRowStretch(1, 1);

        setLayout(mainLayout);
    }

    void MonitorPanel::startUpdate()
    {
        if (m_updateTimer && !m_updateTimer->isActive())
        {
            m_updateTimer->start(1000); // Update every second
        }
    }

    void MonitorPanel::stopUpdate()
    {
        if (m_updateTimer)
        {
            m_updateTimer->stop();
        }
    }

    void MonitorPanel::updateDisplay()
    {
        updateSensorTable();
        updateFlowTable();
        updateValveTable();
        updatePumpTable();
    }

    void MonitorPanel::updateSensorTable()
    {
        if (!m_deviceManager)
        {
            appendPressureUiDebugLog("[UI][PRESSURE] skipped: device manager not ready");
            return;
        }

        auto pSensors = m_deviceManager->getAllPressureSensors();
        auto tSensors = m_deviceManager->getAllTemperatureSensors();
        auto &cfg = ConfigManager::getInstance();
        const bool showTemperatureColumn = cfg.getInt("temp.count", 1) > 0;
        size_t rows = showTemperatureColumn ? ((pSensors.size() > tSensors.size()) ? pSensors.size() : tSensors.size()) : pSensors.size();

        {
            std::ostringstream oss;
            oss << "[UI][PRESSURE] refresh rows=" << rows
                << " pressureSensors=" << pSensors.size()
                << " temperatureSensors=" << tSensors.size();
            appendPressureUiDebugLog(oss.str());
        }

        m_sensorTable->setRowCount(static_cast<int>(rows));

        for (size_t i = 0; i < rows; ++i)
        {
            // 编号与名称以 1 开始并行展示
            int id = static_cast<int>(i + 1);
            m_sensorTable->setItem(i, 0, new QTableWidgetItem(QString::number(id)));
            m_sensorTable->setItem(i, 1, new QTableWidgetItem(QString("传感器 %1").arg(id)));

            // 压力显示（当前内部单位即 kPa），若不存在则留空
            if (i < pSensors.size())
            {
                double kPa = static_cast<double>(pSensors[i].pressure);
                m_sensorTable->setItem(i, 2, new QTableWidgetItem(QString::number(kPa, 'f', pressureDisplayDecimals(pSensors[i]))));

                std::ostringstream oss;
                oss << "[UI][PRESSURE] sensor=" << pSensors[i].id
                    << " pressureKPa=" << pSensors[i].pressure
                    << " displayDecimals=" << pSensors[i].displayDecimals
                    << " displayKPa=" << kPa
                    << " status=" << static_cast<int>(pSensors[i].status);
                appendPressureUiDebugLog(oss.str());
            }
            else
            {
                m_sensorTable->setItem(i, 2, new QTableWidgetItem(""));
                std::ostringstream oss;
                oss << "[UI][PRESSURE] sensor=" << id << " missing-pressure-data";
                appendPressureUiDebugLog(oss.str());
            }

            // 温度显示（℃），若配置禁用则保持为空并隐藏该列
            if (showTemperatureColumn)
            {
                if (i < tSensors.size())
                {
                    m_sensorTable->setItem(i, 3, new QTableWidgetItem(QString::number(tSensors[i].temperature, 'f', 2)));
                }
                else
                {
                    m_sensorTable->setItem(i, 3, new QTableWidgetItem(""));
                }
            }

            // 合并状态：Fault优先，其次Maintenance，其次Online，否则Offline/Unknown
            QString statusText = "未知";
            QColor bgColor(200, 200, 200);

            auto pick = [&](DeviceStatus s)
            {
                if (s == DeviceStatus::FAULT)
                {
                    statusText = "故障";
                    bgColor = QColor(255, 100, 100);
                    return true; // 最高优先
                }
                if (s == DeviceStatus::MAINTENANCE)
                {
                    statusText = "维护";
                    bgColor = QColor(255, 255, 100);
                }
                else if (s == DeviceStatus::ONLINE)
                {
                    statusText = "在线";
                    bgColor = QColor(100, 255, 100);
                }
                else if (s == DeviceStatus::OFFLINE)
                {
                    statusText = "离线";
                    bgColor = QColor(150, 150, 150);
                }
                return false;
            };

            bool faulted = false;
            if (i < pSensors.size())
            {
                faulted = pick(pSensors[i].status);
            }
            if (!faulted && i < tSensors.size())
            {
                pick(tSensors[i].status);
            }

            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setBackground(bgColor);
            m_sensorTable->setItem(i, 4, statusItem);
        }
    }

    void MonitorPanel::updateFlowTable()
    {
        if (!m_deviceManager)
            return;

        auto meters = m_deviceManager->getAllFlowMeters();
        m_flowTable->setRowCount(meters.size());

        for (size_t i = 0; i < meters.size(); ++i)
        {
            const auto &meter = meters[i];
            const QString flowUnit = (!meter.unitLabel.empty() && meter.unitLabel != "unknown")
                                         ? QString::fromStdString(meter.unitLabel)
                                         : "L/min";
            const QString flowText = QString("%1 %2").arg(QString::number(meter.flowRate, 'f', 2)).arg(flowUnit);
            const bool hasEmptyPipeAlarm = (meter.emptyPipeAlarm != 0);
            const bool hasExcitationAlarm = (meter.excitationAlarm != 0);

            m_flowTable->setItem(i, 0, new QTableWidgetItem(QString::number(meter.id)));
            m_flowTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(meter.name)));
            m_flowTable->setItem(i, 2, new QTableWidgetItem(flowText));

            QString statusText;
            QColor bgColor(200, 200, 200);

            if (hasEmptyPipeAlarm || hasExcitationAlarm)
            {
                statusText = "报警";
                if (hasEmptyPipeAlarm && hasExcitationAlarm)
                    statusText += "(空管/励磁)";
                else if (hasEmptyPipeAlarm)
                    statusText += "(空管)";
                else
                    statusText += "(励磁)";
                bgColor = QColor(255, 120, 120);
            }
            else if (meter.status == DeviceStatus::ONLINE)
            {
                statusText = "在线";
                bgColor = QColor(100, 255, 100);
            }
            else if (meter.status == DeviceStatus::FAULT)
            {
                statusText = "故障";
                bgColor = QColor(255, 100, 100);
            }
            else if (meter.status == DeviceStatus::MAINTENANCE)
            {
                statusText = "维护";
                bgColor = QColor(255, 255, 100);
            }
            else if (meter.status == DeviceStatus::OFFLINE)
            {
                statusText = "离线";
                bgColor = QColor(150, 150, 150);
            }
            else
            {
                statusText = "未知";
            }

            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setBackground(bgColor);
            m_flowTable->setItem(i, 3, statusItem);
        }
    }

    void MonitorPanel::updateValveTable()
    {
        if (!m_deviceManager)
            return;

        auto valves = m_deviceManager->getAllValves();
        auto regValves = m_deviceManager->getAllRegulatingValves();
        m_valveTable->setRowCount(static_cast<int>(valves.size() + regValves.size()));

        auto percentToAoRaw = [](float percent) -> int
        {
            if (percent < 0.0f)
                percent = 0.0f;
            else if (percent > 100.0f)
                percent = 100.0f;
            const double raw = 5530.0 + (27648.0 - 5530.0) * (static_cast<double>(percent) / 100.0);
            return static_cast<int>(raw + 0.5);
        };

        auto percentToMilliAmp = [](float percent) -> double
        {
            if (percent < 0.0f)
                percent = 0.0f;
            else if (percent > 100.0f)
                percent = 100.0f;
            return 4.0 + 16.0 * (static_cast<double>(percent) / 100.0);
        };

        for (size_t i = 0; i < valves.size(); ++i)
        {
            const auto &valve = valves[i];

            m_valveTable->setItem(i, 0, new QTableWidgetItem(QString::number(valve.id)));
            m_valveTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(valve.name)));
            m_valveTable->setItem(i, 2, new QTableWidgetItem(QString::number(valve.openingDegree)));
            m_valveTable->setItem(i, 3, new QTableWidgetItem("-"));
            m_valveTable->setItem(i, 4, new QTableWidgetItem("-"));

            QString statusText;
            QColor bgColor(200, 200, 200);

            if (valve.status == ValveStatus::OPEN)
            {
                statusText = "开";
                bgColor = QColor(100, 255, 100);
            }
            else if (valve.status == ValveStatus::CLOSED)
            {
                statusText = "关";
                bgColor = QColor(200, 200, 200);
            }
            else if (valve.status == ValveStatus::OPENING)
            {
                statusText = "开启中";
                bgColor = QColor(255, 255, 100);
            }
            else if (valve.status == ValveStatus::CLOSING)
            {
                statusText = "关闭中";
                bgColor = QColor(255, 255, 100);
            }
            else if (valve.status == ValveStatus::FAULT)
            {
                statusText = "故障";
                bgColor = QColor(255, 100, 100);
            }
            else
            {
                statusText = "未知";
            }

            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setBackground(bgColor);
            m_valveTable->setItem(i, 5, statusItem);
        }

        for (size_t i = 0; i < regValves.size(); ++i)
        {
            const auto &valve = regValves[i];
            const int row = static_cast<int>(valves.size() + i);

            const int aoRaw = percentToAoRaw(valve.openingSetpoint);
            const double ma = percentToMilliAmp(valve.openingSetpoint);

            m_valveTable->setItem(row, 0, new QTableWidgetItem(QString("R%1").arg(valve.id)));
            m_valveTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(valve.name) + " (调压阀)"));
            m_valveTable->setItem(row, 2, new QTableWidgetItem(QString("%1 / %2")
                                                                .arg(QString::number(valve.openingSetpoint, 'f', 1))
                                                                .arg(QString::number(valve.openingPercent, 'f', 1))));
            m_valveTable->setItem(row, 3, new QTableWidgetItem(QString::number(aoRaw)));
            m_valveTable->setItem(row, 4, new QTableWidgetItem(QString::number(ma, 'f', 2)));

            QString statusText;
            QColor bgColor(200, 200, 200);

            if (valve.alarmActive || valve.deviceStatus == DeviceStatus::FAULT)
            {
                statusText = "故障";
                bgColor = QColor(255, 100, 100);
            }
            else if (valve.status == ValveStatus::OPEN)
            {
                statusText = "开";
                bgColor = QColor(100, 255, 100);
            }
            else if (valve.status == ValveStatus::CLOSED)
            {
                statusText = "关";
                bgColor = QColor(200, 200, 200);
            }
            else if (valve.status == ValveStatus::OPENING || valve.status == ValveStatus::CLOSING)
            {
                statusText = "动作中";
                bgColor = QColor(255, 255, 100);
            }
            else
            {
                statusText = "未知";
            }

            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setBackground(bgColor);
            m_valveTable->setItem(row, 5, statusItem);
        }
    }

    void MonitorPanel::updatePumpTable()
    {
        if (!m_deviceManager)
            return;

        auto pumps = m_deviceManager->getAllPumps();
        m_pumpTable->setRowCount(pumps.size());

        for (size_t i = 0; i < pumps.size(); ++i)
        {
            const auto &pump = pumps[i];

            m_pumpTable->setItem(i, 0, new QTableWidgetItem(QString::number(pump.id)));
            m_pumpTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pump.name)));
            m_pumpTable->setItem(i, 2, new QTableWidgetItem(QString::number(pump.frequency, 'f', 1)));

            QString statusText;
            QColor bgColor(200, 200, 200);

            if (pump.isRunning)
            {
                statusText = "运行";
                bgColor = QColor(100, 255, 100);
            }
            else
            {
                statusText = "停止";
                bgColor = QColor(200, 200, 200);
            }

            // Check device status
            if (pump.status == DeviceStatus::FAULT)
            {
                statusText = "故障";
                bgColor = QColor(255, 100, 100);
            }
            else if (pump.status == DeviceStatus::MAINTENANCE)
            {
                statusText = "维护";
                bgColor = QColor(255, 255, 100);
            }

            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setBackground(bgColor);
            m_pumpTable->setItem(i, 3, statusItem);
        }
    }

    // 原温度/压力独立更新函数已合并，不再使用

} // namespace WaterTest
