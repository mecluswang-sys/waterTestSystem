/**
 * @file AutoTestPanel.cpp
 * @brief 自动化测试配置面板实现
 */

#include "gui/AutoTestPanel.h"
#include "DeviceManager.h"
#include "StationClient.h"
#include "ConfigManager.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>

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

    AutoTestPanel::AutoTestPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent)
        : QWidget(parent), m_deviceManager(deviceManager), m_stationClient(nullptr), m_updateTimer(nullptr), m_testValveOpen(false), m_currentPowerType(PowerType::DC), m_testValveVoltage(0.0f), m_testType(TestType::BY_COUNT), m_isAutoTesting(false), m_currentConditionIndex(0), m_currentCycleCount(0), m_autoTestElapsedSeconds(0)
    {
        setupUI();

        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &AutoTestPanel::onUpdateData);
    }

    void AutoTestPanel::setStationClient(std::shared_ptr<StationClient> stationClient)
    {
        m_stationClient = stationClient;
    }

    AutoTestPanel::~AutoTestPanel()
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

    void AutoTestPanel::setupUI()
    {
        auto *mainLayout = new QVBoxLayout(this);

        // ========== 待测试阀控制区 ==========
        m_testValveControlGroup = new QGroupBox("待测试阀控制", this);
        auto *testValveControlLayout = new QGridLayout(m_testValveControlGroup);

        // 供电类型选择
        testValveControlLayout->addWidget(new QLabel("供电类型:", this), 0, 0);
        auto *powerTypeLayout = new QHBoxLayout();
        m_powerTypeDC = new QRadioButton("DC直流", this);
        m_powerTypeDC->setChecked(true);
        connect(m_powerTypeDC, &QRadioButton::toggled, this, &AutoTestPanel::onPowerTypeChanged);
        powerTypeLayout->addWidget(m_powerTypeDC);

        m_powerTypeAC = new QRadioButton("AC交流", this);
        powerTypeLayout->addWidget(m_powerTypeAC);
        powerTypeLayout->addStretch();
        testValveControlLayout->addLayout(powerTypeLayout, 0, 1);

        testValveControlLayout->addWidget(new QLabel("设定电压:", this), 1, 0);
        m_testValveVoltageSpinBox = new QDoubleSpinBox(this);
        m_testValveVoltageSpinBox->setRange(0.0, 24.0);
        m_testValveVoltageSpinBox->setValue(12.0);
        m_testValveVoltageSpinBox->setSingleStep(0.5);
        m_testValveVoltageSpinBox->setSuffix(" V");
        connect(m_testValveVoltageSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &AutoTestPanel::onTestValveVoltageChanged);
        testValveControlLayout->addWidget(m_testValveVoltageSpinBox, 1, 1);

        testValveControlLayout->addWidget(new QLabel("当前电压:", this), 2, 0);
        m_testValveVoltageLabel = new QLabel("0.0 V", this);
        m_testValveVoltageLabel->setProperty("role", "valueBox");
        m_testValveVoltageLabel->setProperty("tone", "info");
        testValveControlLayout->addWidget(m_testValveVoltageLabel, 2, 1);

        testValveControlLayout->addWidget(new QLabel("阀门状态:", this), 3, 0);
        m_testValveStatusLabel = new QLabel("已关闭", this);
        m_testValveStatusLabel->setObjectName("badge");
        m_testValveStatusLabel->setProperty("tone", "muted");
        testValveControlLayout->addWidget(m_testValveStatusLabel, 3, 1);

        auto *valveButtonLayout = new QHBoxLayout();
        m_testValveOpenBtn = new QPushButton("开启阀门", this);
        m_testValveOpenBtn->setProperty("tone", "good");
        m_testValveOpenBtn->setMinimumHeight(45);
        connect(m_testValveOpenBtn, &QPushButton::clicked, this, &AutoTestPanel::onTestValveOpen);
        valveButtonLayout->addWidget(m_testValveOpenBtn);

        m_testValveCloseBtn = new QPushButton("关闭阀门", this);
        m_testValveCloseBtn->setProperty("tone", "bad");
        m_testValveCloseBtn->setMinimumHeight(45);
        m_testValveCloseBtn->setEnabled(false);
        connect(m_testValveCloseBtn, &QPushButton::clicked, this, &AutoTestPanel::onTestValveClose);
        valveButtonLayout->addWidget(m_testValveCloseBtn);

        testValveControlLayout->addLayout(valveButtonLayout, 4, 0, 1, 2);

        mainLayout->addWidget(m_testValveControlGroup);

        // ========== 测试条件配置区 ==========
        m_testConditionGroup = new QGroupBox("自动测试条件配置", this);
        auto *conditionLayout = new QVBoxLayout(m_testConditionGroup);

        // 测试类型选择
        auto *testTypeLayout = new QHBoxLayout();
        testTypeLayout->addWidget(new QLabel("测试类型:", this));

        m_testByCountRadio = new QRadioButton("按次数测试", this);
        m_testByCountRadio->setChecked(true);
        connect(m_testByCountRadio, &QRadioButton::toggled, this, &AutoTestPanel::onTestTypeChanged);
        testTypeLayout->addWidget(m_testByCountRadio);

        m_testByDurationRadio = new QRadioButton("按时长测试", this);
        testTypeLayout->addWidget(m_testByDurationRadio);

        testTypeLayout->addStretch();
        conditionLayout->addLayout(testTypeLayout);

        // 测试条件表格
        m_testConditionTable = new QTableWidget(this);
        m_testConditionTable->setColumnCount(8);
        m_testConditionTable->setHorizontalHeaderLabels({"条件名称", "供电类型", "目标压力(kPa)", "流量(L/min)",
                                                         "温度(°C)", "阀门电压(V)", "循环次数", "测试时长(秒)"});
        m_testConditionTable->horizontalHeader()->setStretchLastSection(true);
        m_testConditionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_testConditionTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_testConditionTable->setMinimumHeight(200);
        m_testConditionTable->setAlternatingRowColors(true);
        conditionLayout->addWidget(m_testConditionTable);

        // 条件管理按钮
        auto *conditionBtnLayout = new QHBoxLayout();

        m_addConditionBtn = new QPushButton("添加条件", this);
        m_addConditionBtn->setProperty("tone", "info");
        m_addConditionBtn->setMinimumHeight(40);
        connect(m_addConditionBtn, &QPushButton::clicked, this, &AutoTestPanel::onAddTestCondition);
        conditionBtnLayout->addWidget(m_addConditionBtn);

        m_editConditionBtn = new QPushButton("编辑条件", this);
        m_editConditionBtn->setProperty("tone", "warn");
        m_editConditionBtn->setMinimumHeight(40);
        connect(m_editConditionBtn, &QPushButton::clicked, this, &AutoTestPanel::onEditTestCondition);
        conditionBtnLayout->addWidget(m_editConditionBtn);

        m_deleteConditionBtn = new QPushButton("删除条件", this);
        m_deleteConditionBtn->setProperty("tone", "bad");
        m_deleteConditionBtn->setMinimumHeight(40);
        connect(m_deleteConditionBtn, &QPushButton::clicked, this, &AutoTestPanel::onDeleteTestCondition);
        conditionBtnLayout->addWidget(m_deleteConditionBtn);

        conditionBtnLayout->addStretch();
        conditionLayout->addLayout(conditionBtnLayout);

        // 自动测试控制
        auto *autoTestLayout = new QHBoxLayout();
        m_startAutoTestBtn = new QPushButton("开始自动测试", this);
        m_startAutoTestBtn->setProperty("tone", "info");
        m_startAutoTestBtn->setProperty("size", "lg");
        m_startAutoTestBtn->setMinimumHeight(50);
        connect(m_startAutoTestBtn, &QPushButton::clicked, this, &AutoTestPanel::onStartAutoTest);
        autoTestLayout->addWidget(m_startAutoTestBtn, 2);

        m_autoTestProgressLabel = new QLabel("就绪", this);
        m_autoTestProgressLabel->setProperty("role", "statusBox");
        m_autoTestProgressLabel->setProperty("tone", "muted");
        autoTestLayout->addWidget(m_autoTestProgressLabel, 1);

        conditionLayout->addLayout(autoTestLayout);

        mainLayout->addWidget(m_testConditionGroup);
        mainLayout->addStretch();
    }

    void AutoTestPanel::startUpdate(int intervalMs)
    {
        if (m_updateTimer && !m_updateTimer->isActive())
        {
            m_updateTimer->start(intervalMs);
        }
    }

    void AutoTestPanel::stopUpdate()
    {
        if (m_updateTimer && m_updateTimer->isActive())
        {
            m_updateTimer->stop();
        }
    }

    void AutoTestPanel::onPowerTypeChanged()
    {
        if (m_powerTypeDC->isChecked())
        {
            m_currentPowerType = PowerType::DC;
            // DC电压范围通常0-24V
            m_testValveVoltageSpinBox->setRange(0.0, 24.0);
            m_testValveVoltageSpinBox->setValue(12.0);
        }
        else
        {
            m_currentPowerType = PowerType::AC;
            // AC电压范围通常0-220V
            m_testValveVoltageSpinBox->setRange(0.0, 220.0);
            m_testValveVoltageSpinBox->setValue(24.0);
        }
    }

    void AutoTestPanel::onUpdateData()
    {
        if (!m_deviceManager)
        {
            return;
        }

        // 如果正在自动测试，更新进度
        if (m_isAutoTesting && m_currentConditionIndex < static_cast<int>(m_testConditions.size()))
        {
            const auto &condition = m_testConditions[m_currentConditionIndex];

            if (m_testType == TestType::BY_COUNT)
            {
                // 按次数测试：每秒计为一次
                m_currentCycleCount++;

                if (m_currentCycleCount >= condition.cycleCount)
                {
                    // 当前条件完成，进入下一个
                    m_currentConditionIndex++;
                    m_currentCycleCount = 0;
                    executeAutoTest();
                }
                else
                {
                    // 更新进度
                    m_autoTestProgressLabel->setText(
                        QString("条件 %1/%2: %3 - 第 %4/%5 次")
                            .arg(m_currentConditionIndex + 1)
                            .arg(m_testConditions.size())
                            .arg(condition.name)
                            .arg(m_currentCycleCount)
                            .arg(condition.cycleCount));
                }
            }
            else
            {
                // 按时长测试
                m_autoTestElapsedSeconds++;

                if (m_autoTestElapsedSeconds >= condition.durationSeconds)
                {
                    // 当前条件完成，进入下一个
                    m_currentConditionIndex++;
                    m_autoTestElapsedSeconds = 0;
                    executeAutoTest();
                }
                else
                {
                    // 更新进度
                    m_autoTestProgressLabel->setText(
                        QString("条件 %1/%2: %3 - 已用时 %4/%5 秒")
                            .arg(m_currentConditionIndex + 1)
                            .arg(m_testConditions.size())
                            .arg(condition.name)
                            .arg(m_autoTestElapsedSeconds)
                            .arg(condition.durationSeconds));
                }
            }
        }
    }

    void AutoTestPanel::onTestValveOpen()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (!m_deviceManager && !(m_stationClient && strictRemoteMode))
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        auto reply = QMessageBox::question(this, "确认",
                                           QString("确定开启待测试阀吗？\n\n当前设定电压: %1 V")
                                               .arg(m_testValveVoltageSpinBox->value(), 0, 'f', 1),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }

        m_testValveOpen = true;
        m_testValveVoltage = static_cast<float>(m_testValveVoltageSpinBox->value());

        if (m_stationClient && strictRemoteMode)
        {
            ControlCommand cmd;
            cmd.command_type = 2; // valve
            cmd.index = 2;        // 待测试阀（阀3）
            cmd.action = 1;
            if (!m_stationClient->sendCommand(cmd))
            {
                QMessageBox::warning(this, "错误", "发送开阀命令失败（主控通信异常）");
                m_testValveOpen = false;
                m_testValveVoltage = 0.0f;
                return;
            }
        }

        // 更新UI
        m_testValveStatusLabel->setText(QString("已开启 (%1 V)").arg(m_testValveVoltage, 0, 'f', 1));
        m_testValveStatusLabel->setProperty("tone", "good");
        m_testValveStatusLabel->style()->unpolish(m_testValveStatusLabel);
        m_testValveStatusLabel->style()->polish(m_testValveStatusLabel);
        m_testValveVoltageLabel->setText(QString("%1 V").arg(m_testValveVoltage, 0, 'f', 1));
        m_testValveOpenBtn->setEnabled(false);
        m_testValveCloseBtn->setEnabled(true);
        m_testValveVoltageSpinBox->setEnabled(false);

        QMessageBox::information(this, "成功", QString("待测试阀已开启，电压设定为 %1 V").arg(m_testValveVoltage, 0, 'f', 1));
    }

    void AutoTestPanel::onTestValveClose()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (!m_deviceManager && !(m_stationClient && strictRemoteMode))
        {
            return;
        }

        if (m_stationClient && strictRemoteMode)
        {
            ControlCommand cmd;
            cmd.command_type = 2; // valve
            cmd.index = 2;        // 待测试阀（阀3）
            cmd.action = 0;
            if (!m_stationClient->sendCommand(cmd))
            {
                QMessageBox::warning(this, "错误", "发送关阀命令失败（主控通信异常）");
                return;
            }
        }

        m_testValveOpen = false;
        m_testValveVoltage = 0.0f;

        // 更新UI
        m_testValveStatusLabel->setText("已关闭");
        m_testValveStatusLabel->setProperty("tone", "muted");
        m_testValveStatusLabel->style()->unpolish(m_testValveStatusLabel);
        m_testValveStatusLabel->style()->polish(m_testValveStatusLabel);
        m_testValveVoltageLabel->setText("0.0 V");
        m_testValveOpenBtn->setEnabled(true);
        m_testValveCloseBtn->setEnabled(false);
        m_testValveVoltageSpinBox->setEnabled(true);
    }

    void AutoTestPanel::onTestValveVoltageChanged(double value)
    {
        // 如果阀门已开启，实时更新电压值
        if (m_testValveOpen)
        {
            m_testValveVoltage = static_cast<float>(value);
            m_testValveVoltageLabel->setText(QString("%1 V").arg(value, 0, 'f', 1));
            m_testValveStatusLabel->setText(QString("已开启 (%1 V)").arg(value, 0, 'f', 1));
        }
    }

    void AutoTestPanel::onAddTestCondition()
    {
        // 创建对话框
        QDialog dialog(this);
        dialog.setWindowTitle("添加测试条件");
        dialog.setMinimumWidth(450);

        auto *layout = new QFormLayout(&dialog);

        // 输入控件
        auto *nameEdit = new QLineEdit(&dialog);
        nameEdit->setText(QString("条件%1").arg(m_testConditions.size() + 1));

        // 供电类型选择
        auto *powerTypeWidget = new QWidget(&dialog);
        auto *powerTypeLayout = new QHBoxLayout(powerTypeWidget);
        powerTypeLayout->setContentsMargins(0, 0, 0, 0);
        auto *powerTypeDC = new QRadioButton("DC直流", powerTypeWidget);
        powerTypeDC->setChecked(true);
        auto *powerTypeAC = new QRadioButton("AC交流", powerTypeWidget);
        powerTypeLayout->addWidget(powerTypeDC);
        powerTypeLayout->addWidget(powerTypeAC);
        powerTypeLayout->addStretch();

        auto *pressureSpinBox = new QDoubleSpinBox(&dialog);
        pressureSpinBox->setRange(0.0, 1000.0);
        pressureSpinBox->setValue(500.0);
        pressureSpinBox->setSuffix(" kPa");
        pressureSpinBox->setDecimals(1);

        auto *flowRateSpinBox = new QDoubleSpinBox(&dialog);
        flowRateSpinBox->setRange(0.0, 100.0);
        flowRateSpinBox->setValue(10.0);
        flowRateSpinBox->setSuffix(" L/min");

        auto *tempSpinBox = new QDoubleSpinBox(&dialog);
        tempSpinBox->setRange(-20.0, 100.0);
        tempSpinBox->setValue(25.0);
        tempSpinBox->setSuffix(" °C");

        auto *voltageSpinBox = new QDoubleSpinBox(&dialog);
        voltageSpinBox->setRange(0.0, 24.0);
        voltageSpinBox->setValue(12.0);
        voltageSpinBox->setSuffix(" V");

        auto *cycleSpinBox = new QSpinBox(&dialog);
        cycleSpinBox->setRange(1, 100000);
        cycleSpinBox->setValue(100);
        cycleSpinBox->setSuffix(" 次");

        auto *durationSpinBox = new QSpinBox(&dialog);
        durationSpinBox->setRange(60, 86400);
        durationSpinBox->setValue(3600);
        durationSpinBox->setSuffix(" 秒");

        layout->addRow("条件名称:", nameEdit);
        layout->addRow("供电类型:", powerTypeWidget);
        layout->addRow("目标压力:", pressureSpinBox);
        layout->addRow("流量:", flowRateSpinBox);
        layout->addRow("温度:", tempSpinBox);
        layout->addRow("阀门电压:", voltageSpinBox);
        layout->addRow("循环次数:", cycleSpinBox);
        layout->addRow("测试时长:", durationSpinBox);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addRow(buttonBox);

        if (dialog.exec() == QDialog::Accepted)
        {
            TestCondition condition;
            condition.name = nameEdit->text();
            condition.powerType = powerTypeDC->isChecked() ? PowerType::DC : PowerType::AC;
            condition.targetPressure = static_cast<float>(kgfCm2ToKPa(pressureSpinBox->value()));
            condition.flowRate = static_cast<float>(flowRateSpinBox->value());
            condition.temperature = static_cast<float>(tempSpinBox->value());
            condition.valveVoltage = static_cast<float>(voltageSpinBox->value());
            condition.cycleCount = cycleSpinBox->value();
            condition.durationSeconds = durationSpinBox->value();

            m_testConditions.push_back(condition);
            updateTestConditionTable();
        }
    }

    void AutoTestPanel::onEditTestCondition()
    {
        int row = m_testConditionTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_testConditions.size()))
        {
            QMessageBox::warning(this, "提示", "请先选择要编辑的测试条件");
            return;
        }

        TestCondition &condition = m_testConditions[row];

        // 创建对话框
        QDialog dialog(this);
        dialog.setWindowTitle("编辑测试条件");
        dialog.setMinimumWidth(450);

        auto *layout = new QFormLayout(&dialog);

        // 输入控件
        auto *nameEdit = new QLineEdit(&dialog);
        nameEdit->setText(condition.name);

        // 供电类型选择
        auto *powerTypeWidget = new QWidget(&dialog);
        auto *powerTypeLayout = new QHBoxLayout(powerTypeWidget);
        powerTypeLayout->setContentsMargins(0, 0, 0, 0);
        auto *powerTypeDC = new QRadioButton("DC直流", powerTypeWidget);
        auto *powerTypeAC = new QRadioButton("AC交流", powerTypeWidget);
        if (condition.powerType == PowerType::DC)
        {
            powerTypeDC->setChecked(true);
        }
        else
        {
            powerTypeAC->setChecked(true);
        }
        powerTypeLayout->addWidget(powerTypeDC);
        powerTypeLayout->addWidget(powerTypeAC);
        powerTypeLayout->addStretch();

        auto *pressureSpinBox = new QDoubleSpinBox(&dialog);
        pressureSpinBox->setRange(0.0, 1000.0);
        pressureSpinBox->setValue(condition.targetPressure);
        pressureSpinBox->setSuffix(" kPa");
        pressureSpinBox->setDecimals(1);

        auto *flowRateSpinBox = new QDoubleSpinBox(&dialog);
        flowRateSpinBox->setRange(0.0, 100.0);
        flowRateSpinBox->setValue(condition.flowRate);
        flowRateSpinBox->setSuffix(" L/min");

        auto *tempSpinBox = new QDoubleSpinBox(&dialog);
        tempSpinBox->setRange(-20.0, 100.0);
        tempSpinBox->setValue(condition.temperature);
        tempSpinBox->setSuffix(" °C");

        auto *voltageSpinBox = new QDoubleSpinBox(&dialog);
        voltageSpinBox->setRange(0.0, 24.0);
        voltageSpinBox->setValue(condition.valveVoltage);
        voltageSpinBox->setSuffix(" V");

        auto *cycleSpinBox = new QSpinBox(&dialog);
        cycleSpinBox->setRange(1, 100000);
        cycleSpinBox->setValue(condition.cycleCount);
        cycleSpinBox->setSuffix(" 次");

        auto *durationSpinBox = new QSpinBox(&dialog);
        durationSpinBox->setRange(60, 86400);
        durationSpinBox->setValue(condition.durationSeconds);
        durationSpinBox->setSuffix(" 秒");

        layout->addRow("条件名称:", nameEdit);
        layout->addRow("供电类型:", powerTypeWidget);
        layout->addRow("目标压力:", pressureSpinBox);
        layout->addRow("流量:", flowRateSpinBox);
        layout->addRow("温度:", tempSpinBox);
        layout->addRow("阀门电压:", voltageSpinBox);
        layout->addRow("循环次数:", cycleSpinBox);
        layout->addRow("测试时长:", durationSpinBox);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addRow(buttonBox);

        if (dialog.exec() == QDialog::Accepted)
        {
            condition.name = nameEdit->text();
            condition.powerType = powerTypeDC->isChecked() ? PowerType::DC : PowerType::AC;
            condition.targetPressure = static_cast<float>(kgfCm2ToKPa(pressureSpinBox->value()));
            condition.flowRate = static_cast<float>(flowRateSpinBox->value());
            condition.temperature = static_cast<float>(tempSpinBox->value());
            condition.valveVoltage = static_cast<float>(voltageSpinBox->value());
            condition.cycleCount = cycleSpinBox->value();
            condition.durationSeconds = durationSpinBox->value();

            updateTestConditionTable();
        }
    }

    void AutoTestPanel::onDeleteTestCondition()
    {
        int row = m_testConditionTable->currentRow();
        if (row < 0 || row >= static_cast<int>(m_testConditions.size()))
        {
            QMessageBox::warning(this, "提示", "请先选择要删除的测试条件");
            return;
        }

        auto reply = QMessageBox::question(this, "确认",
                                           QString("确定要删除测试条件 \"%1\" 吗？")
                                               .arg(m_testConditions[row].name),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            m_testConditions.erase(m_testConditions.begin() + row);
            updateTestConditionTable();
        }
    }

    void AutoTestPanel::onStartAutoTest()
    {
        if (m_testConditions.empty())
        {
            QMessageBox::warning(this, "提示", "请先添加至少一个测试条件");
            return;
        }

        if (m_isAutoTesting)
        {
            // 停止自动测试
            m_isAutoTesting = false;
            m_startAutoTestBtn->setText("开始自动测试");
            m_startAutoTestBtn->setProperty("tone", "info");
            m_startAutoTestBtn->setProperty("size", "lg");
            m_startAutoTestBtn->style()->unpolish(m_startAutoTestBtn);
            m_startAutoTestBtn->style()->polish(m_startAutoTestBtn);
            m_autoTestProgressLabel->setText("已停止");
            m_autoTestProgressLabel->setProperty("role", "statusBox");
            m_autoTestProgressLabel->setProperty("tone", "warn");
            m_autoTestProgressLabel->style()->unpolish(m_autoTestProgressLabel);
            m_autoTestProgressLabel->style()->polish(m_autoTestProgressLabel);

            // 关闭待测试阀
            if (m_testValveOpen)
            {
                onTestValveClose();
            }
            return;
        }

        // 开始自动测试
        auto reply = QMessageBox::question(this, "确认",
                                           QString("确定开始自动测试吗？\n\n"
                                                   "测试类型: %1\n"
                                                   "测试条件数: %2 个")
                                               .arg(m_testType == TestType::BY_COUNT ? "按次数" : "按时长")
                                               .arg(m_testConditions.size()),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }

        m_isAutoTesting = true;
        m_currentConditionIndex = 0;
        m_currentCycleCount = 0;
        m_autoTestElapsedSeconds = 0;

        m_startAutoTestBtn->setText("停止自动测试");
        m_startAutoTestBtn->setProperty("tone", "bad");
        m_startAutoTestBtn->setProperty("size", "lg");
        m_startAutoTestBtn->style()->unpolish(m_startAutoTestBtn);
        m_startAutoTestBtn->style()->polish(m_startAutoTestBtn);

        // 开始执行第一个测试条件
        executeAutoTest();
    }

    void AutoTestPanel::onTestTypeChanged()
    {
        m_testType = m_testByCountRadio->isChecked() ? TestType::BY_COUNT : TestType::BY_DURATION;

        // 更新表格列显示（列索引已更新：6=循环次数, 7=测试时长）
        if (m_testType == TestType::BY_COUNT)
        {
            m_testConditionTable->setColumnHidden(7, true);  // 隐藏测试时长列
            m_testConditionTable->setColumnHidden(6, false); // 显示循环次数列
        }
        else
        {
            m_testConditionTable->setColumnHidden(6, true);  // 隐藏循环次数列
            m_testConditionTable->setColumnHidden(7, false); // 显示测试时长列
        }
    }

    void AutoTestPanel::updateTestConditionTable()
    {
        m_testConditionTable->setRowCount(static_cast<int>(m_testConditions.size()));

        for (size_t i = 0; i < m_testConditions.size(); ++i)
        {
            const auto &condition = m_testConditions[i];

            m_testConditionTable->setItem(i, 0, new QTableWidgetItem(condition.name));
            m_testConditionTable->setItem(i, 1, new QTableWidgetItem(condition.powerType == PowerType::DC ? "DC直流" : "AC交流"));
            m_testConditionTable->setItem(i, 2, new QTableWidgetItem(QString::number(kPaToKgfCm2(condition.targetPressure), 'f', 2)));
            m_testConditionTable->setItem(i, 3, new QTableWidgetItem(QString::number(condition.flowRate, 'f', 1)));
            m_testConditionTable->setItem(i, 4, new QTableWidgetItem(QString::number(condition.temperature, 'f', 1)));
            m_testConditionTable->setItem(i, 5, new QTableWidgetItem(QString::number(condition.valveVoltage, 'f', 1)));
            m_testConditionTable->setItem(i, 6, new QTableWidgetItem(QString::number(condition.cycleCount)));
            m_testConditionTable->setItem(i, 7, new QTableWidgetItem(QString::number(condition.durationSeconds)));
        }

        // 根据测试类型隐藏相应的列
        onTestTypeChanged();
    }

    void AutoTestPanel::applyTestCondition(const TestCondition &condition)
    {
        // 应用测试条件到设备

        // 设置阀门电压
        m_testValveVoltageSpinBox->setValue(condition.valveVoltage);

        // 打开待测试阀
        if (!m_testValveOpen)
        {
            const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
            if (m_stationClient && strictRemoteMode)
            {
                ControlCommand cmd;
                cmd.command_type = 2; // valve
                cmd.index = 2;        // 待测试阀（阀3）
                cmd.action = 1;
                if (!m_stationClient->sendCommand(cmd))
                {
                    m_autoTestProgressLabel->setText("主控通信异常：待测阀开启失败");
                    m_autoTestProgressLabel->setProperty("tone", "bad");
                    m_autoTestProgressLabel->style()->unpolish(m_autoTestProgressLabel);
                    m_autoTestProgressLabel->style()->polish(m_autoTestProgressLabel);
                    return;
                }
            }

            m_testValveOpen = true;
            m_testValveVoltage = condition.valveVoltage;
            m_testValveStatusLabel->setText(QString("已开启 (%1 V)").arg(m_testValveVoltage, 0, 'f', 1));
            m_testValveStatusLabel->setProperty("tone", "good");
            m_testValveStatusLabel->style()->unpolish(m_testValveStatusLabel);
            m_testValveStatusLabel->style()->polish(m_testValveStatusLabel);
            m_testValveVoltageLabel->setText(QString("%1 V").arg(m_testValveVoltage, 0, 'f', 1));
            m_testValveOpenBtn->setEnabled(false);
            m_testValveCloseBtn->setEnabled(true);
            m_testValveVoltageSpinBox->setEnabled(false);
        }
    }

    void AutoTestPanel::executeAutoTest()
    {
        if (!m_isAutoTesting || m_currentConditionIndex >= static_cast<int>(m_testConditions.size()))
        {
            // 测试完成
            m_isAutoTesting = false;
            m_startAutoTestBtn->setText("开始自动测试");
            m_startAutoTestBtn->setProperty("tone", "info");
            m_startAutoTestBtn->setProperty("size", "lg");
            m_startAutoTestBtn->style()->unpolish(m_startAutoTestBtn);
            m_startAutoTestBtn->style()->polish(m_startAutoTestBtn);
            m_autoTestProgressLabel->setText("测试完成");
            m_autoTestProgressLabel->setProperty("role", "statusBox");
            m_autoTestProgressLabel->setProperty("tone", "good");
            m_autoTestProgressLabel->style()->unpolish(m_autoTestProgressLabel);
            m_autoTestProgressLabel->style()->polish(m_autoTestProgressLabel);

            // 关闭待测试阀
            if (m_testValveOpen)
            {
                onTestValveClose();
            }

            QMessageBox::information(this, "完成", "自动测试已完成！");
            return;
        }

        const auto &condition = m_testConditions[m_currentConditionIndex];

        // 应用当前测试条件
        applyTestCondition(condition);

        // 更新进度显示
        if (m_testType == TestType::BY_COUNT)
        {
            m_autoTestProgressLabel->setText(
                QString("条件 %1/%2: %3 - 第 %4/%5 次")
                    .arg(m_currentConditionIndex + 1)
                    .arg(m_testConditions.size())
                    .arg(condition.name)
                    .arg(m_currentCycleCount + 1)
                    .arg(condition.cycleCount));
        }
        else
        {
            m_autoTestProgressLabel->setText(
                QString("条件 %1/%2: %3 - 已用时 %4/%5 秒")
                    .arg(m_currentConditionIndex + 1)
                    .arg(m_testConditions.size())
                    .arg(condition.name)
                    .arg(m_autoTestElapsedSeconds)
                    .arg(condition.durationSeconds));
        }
        m_autoTestProgressLabel->setStyleSheet("QLabel { padding: 10px; border: 2px solid #2196F3; background-color: #E3F2FD; border-radius: 5px; font-size: 12pt; font-weight: bold; }");
    }

} // namespace WaterTest
