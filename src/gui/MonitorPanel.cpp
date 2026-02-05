/**
 * @file MonitorPanel.cpp
 * @brief Monitor Panel Implementation - Simplified Version (Fixed)
 */

#include "gui/MonitorPanel.h"
#include "DeviceManager.h"
#include "DeviceTypes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QGroupBox>
#include <QGridLayout>
#include <algorithm>

namespace WaterTest
{

    MonitorPanel::MonitorPanel(std::shared_ptr<DeviceManager> deviceMgr, QWidget *parent)
        : QWidget(parent), m_deviceManager(deviceMgr), m_updateTimer(nullptr)
    {
        setupUI();

        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &MonitorPanel::updateDisplay);
    }

    MonitorPanel::~MonitorPanel()
    {
        if (m_updateTimer && m_updateTimer->isActive())
        {
            m_updateTimer->stop();
        }
    }

    void MonitorPanel::setupUI()
    {
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
        m_sensorTable->setHorizontalHeaderLabels({"编号", "名称", "压力 (MPa)", "温度 (℃)", "状态"});
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
        m_flowTable->setHorizontalHeaderLabels({"编号", "名称", "流量 (L/min)", "状态"});
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

        m_valveTable = new QTableWidget(0, 4, this);
        m_valveTable->setHorizontalHeaderLabels({"编号", "名称", "开度 (%)", "状态"});
        m_valveTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_valveTable->horizontalHeader()->setMinimumSectionSize(70);
        m_valveTable->horizontalHeader()->setDefaultSectionSize(100);
        m_valveTable->setColumnWidth(1, 150);
        // 让“状态”列填充剩余空间
        m_valveTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
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
        if (m_updateTimer)
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
            return;

        auto pSensors = m_deviceManager->getAllPressureSensors();
        auto tSensors = m_deviceManager->getAllTemperatureSensors();
        size_t rows = (pSensors.size() > tSensors.size()) ? pSensors.size() : tSensors.size();

        m_sensorTable->setRowCount(static_cast<int>(rows));

        for (size_t i = 0; i < rows; ++i)
        {
            // 编号与名称以 1 开始并行展示
            int id = static_cast<int>(i + 1);
            m_sensorTable->setItem(i, 0, new QTableWidgetItem(QString::number(id)));
            m_sensorTable->setItem(i, 1, new QTableWidgetItem(QString("传感器 %1").arg(id)));

            // 压力显示（Pa→MPa），若不存在则留空
            if (i < pSensors.size())
            {
                double mpa = static_cast<double>(pSensors[i].pressure) / 1000000.0;
                m_sensorTable->setItem(i, 2, new QTableWidgetItem(QString::number(mpa, 'f', 3)));
            }
            else
            {
                m_sensorTable->setItem(i, 2, new QTableWidgetItem(""));
            }

            // 温度显示（℃），若不存在则留空
            if (i < tSensors.size())
            {
                m_sensorTable->setItem(i, 3, new QTableWidgetItem(QString::number(tSensors[i].temperature, 'f', 2)));
            }
            else
            {
                m_sensorTable->setItem(i, 3, new QTableWidgetItem(""));
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

            m_flowTable->setItem(i, 0, new QTableWidgetItem(QString::number(meter.id)));
            m_flowTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(meter.name)));
            m_flowTable->setItem(i, 2, new QTableWidgetItem(QString::number(meter.flowRate, 'f', 2)));

            QString statusText;
            QColor bgColor(200, 200, 200);

            if (meter.status == DeviceStatus::ONLINE)
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
        m_valveTable->setRowCount(valves.size());

        for (size_t i = 0; i < valves.size(); ++i)
        {
            const auto &valve = valves[i];

            m_valveTable->setItem(i, 0, new QTableWidgetItem(QString::number(valve.id)));
            m_valveTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(valve.name)));
            m_valveTable->setItem(i, 2, new QTableWidgetItem(QString::number(valve.openingDegree)));

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
            m_valveTable->setItem(i, 3, statusItem);
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
