/**
 * @file ConfigDialog.cpp
 * @brief 配置对话框实现
 */

#include "gui/ConfigDialog.h"
#include "ConfigManager.h"
#include "S7PLCClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QTabWidget>

namespace WaterTest
{

    ConfigDialog::ConfigDialog(QWidget *parent)
        : QDialog(parent), m_plcIpEdit(nullptr), m_plcRackSpinBox(nullptr), m_plcSlotSpinBox(nullptr), m_collectionIntervalSpinBox(nullptr), m_pressureMaxLimitSpinBox(nullptr), m_pressureMinLimitSpinBox(nullptr), m_pressureAlarmThresholdSpinBox(nullptr), m_flowMaxRateSpinBox(nullptr), m_flowAlarmThresholdSpinBox(nullptr), m_emergencyStopCheckBox(nullptr), m_autoRecoveryCheckBox(nullptr), m_logLevelComboBox(nullptr), m_alarmSoundCheckBox(nullptr), m_alarmEmailCheckBox(nullptr), m_emailAddressEdit(nullptr)
    {
        setWindowTitle("系统配置");
        setMinimumSize(600, 550);

        setupUI();
        loadConfig();
    }

    ConfigDialog::~ConfigDialog()
    {
    }

    void ConfigDialog::setupUI()
    {
        auto *mainLayout = new QVBoxLayout(this);

        // 创建选项卡控件
        auto *tabWidget = new QTabWidget(this);

        // ================== Tab 1: PLC连接配置 ==================
        auto *plcWidget = new QWidget(this);
        auto *plcLayout = new QFormLayout(plcWidget);

        m_plcIpEdit = new QLineEdit(plcWidget);
        m_plcIpEdit->setPlaceholderText("192.168.0.1");
        QRegularExpression ipRegex("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
        m_plcIpEdit->setValidator(new QRegularExpressionValidator(ipRegex, this));
        plcLayout->addRow("IP地址:", m_plcIpEdit);

        m_plcRackSpinBox = new QSpinBox(plcWidget);
        m_plcRackSpinBox->setRange(0, 7);
        plcLayout->addRow("机架:", m_plcRackSpinBox);

        m_plcSlotSpinBox = new QSpinBox(plcWidget);
        m_plcSlotSpinBox->setRange(0, 15);
        plcLayout->addRow("插槽:", m_plcSlotSpinBox);

        m_collectionIntervalSpinBox = new QSpinBox(plcWidget);
        m_collectionIntervalSpinBox->setRange(100, 60000);
        m_collectionIntervalSpinBox->setSingleStep(100);
        m_collectionIntervalSpinBox->setSuffix(" ms");
        plcLayout->addRow("采集周期:", m_collectionIntervalSpinBox);

        plcLayout->addRow(new QLabel("")); // 空行
        auto *testButton = new QPushButton("测试连接", plcWidget);
        plcLayout->addRow("", testButton);
        connect(testButton, &QPushButton::clicked, this, &ConfigDialog::onTest);

        tabWidget->addTab(plcWidget, "PLC连接");

        // ================== Tab 2: 传感器配置 ==================
        auto *sensorWidget = new QWidget(this);
        auto *sensorLayout = new QVBoxLayout(sensorWidget);

        // 压力传感器配置
        auto *pressureGroup = new QGroupBox("压力传感器", sensorWidget);
        auto *pressureLayout = new QFormLayout(pressureGroup);

        m_pressureMaxLimitSpinBox = new QDoubleSpinBox(pressureGroup);
        m_pressureMaxLimitSpinBox->setRange(0.0, 100.0);
        m_pressureMaxLimitSpinBox->setDecimals(2);
        m_pressureMaxLimitSpinBox->setSuffix(" MPa");
        pressureLayout->addRow("最大压力:", m_pressureMaxLimitSpinBox);

        m_pressureMinLimitSpinBox = new QDoubleSpinBox(pressureGroup);
        m_pressureMinLimitSpinBox->setRange(0.0, 100.0);
        m_pressureMinLimitSpinBox->setDecimals(2);
        m_pressureMinLimitSpinBox->setSuffix(" MPa");
        pressureLayout->addRow("最小压力:", m_pressureMinLimitSpinBox);

        m_pressureAlarmThresholdSpinBox = new QDoubleSpinBox(pressureGroup);
        m_pressureAlarmThresholdSpinBox->setRange(0.0, 100.0);
        m_pressureAlarmThresholdSpinBox->setDecimals(2);
        m_pressureAlarmThresholdSpinBox->setSuffix(" MPa");
        pressureLayout->addRow("报警阈值:", m_pressureAlarmThresholdSpinBox);

        sensorLayout->addWidget(pressureGroup);

        // 流量计配置
        auto *flowGroup = new QGroupBox("流量计", sensorWidget);
        auto *flowLayout = new QFormLayout(flowGroup);

        m_flowMaxRateSpinBox = new QDoubleSpinBox(flowGroup);
        m_flowMaxRateSpinBox->setRange(0.0, 1000.0);
        m_flowMaxRateSpinBox->setDecimals(2);
        m_flowMaxRateSpinBox->setSuffix(" L/min");
        flowLayout->addRow("最大流量:", m_flowMaxRateSpinBox);

        m_flowAlarmThresholdSpinBox = new QDoubleSpinBox(flowGroup);
        m_flowAlarmThresholdSpinBox->setRange(0.0, 1000.0);
        m_flowAlarmThresholdSpinBox->setDecimals(2);
        m_flowAlarmThresholdSpinBox->setSuffix(" L/min");
        flowLayout->addRow("报警阈值:", m_flowAlarmThresholdSpinBox);

        sensorLayout->addWidget(flowGroup);
        sensorLayout->addStretch();

        tabWidget->addTab(sensorWidget, "传感器配置");

        // ================== Tab 3: 系统配置 ==================
        auto *systemWidget = new QWidget(this);
        auto *systemLayout = new QFormLayout(systemWidget);

        m_emergencyStopCheckBox = new QCheckBox("启用", systemWidget);
        systemLayout->addRow("紧急停止:", m_emergencyStopCheckBox);

        m_autoRecoveryCheckBox = new QCheckBox("启用", systemWidget);
        systemLayout->addRow("自动恢复:", m_autoRecoveryCheckBox);

        m_logLevelComboBox = new QComboBox(systemWidget);
        m_logLevelComboBox->addItems({"debug", "info", "warning", "error"});
        systemLayout->addRow("日志级别:", m_logLevelComboBox);

        systemLayout->addRow(new QLabel("")); // 空行
        systemLayout->addRow(new QLabel("<small>提示: 修改日志级别后重启生效</small>", systemWidget));

        tabWidget->addTab(systemWidget, "系统配置");

        // ================== Tab 4: 报警配置 ==================
        auto *alarmWidget = new QWidget(this);
        auto *alarmLayout = new QFormLayout(alarmWidget);

        m_alarmSoundCheckBox = new QCheckBox("启用声音报警", alarmWidget);
        alarmLayout->addRow("", m_alarmSoundCheckBox);

        m_alarmEmailCheckBox = new QCheckBox("启用邮件报警", alarmWidget);
        alarmLayout->addRow("", m_alarmEmailCheckBox);

        m_emailAddressEdit = new QLineEdit(alarmWidget);
        m_emailAddressEdit->setPlaceholderText("example@domain.com");
        alarmLayout->addRow("报警邮箱:", m_emailAddressEdit);

        // 邮箱启用状态关联
        connect(m_alarmEmailCheckBox, &QCheckBox::toggled, m_emailAddressEdit, &QLineEdit::setEnabled);

        alarmLayout->addRow(new QLabel("")); // 空行
        alarmLayout->addRow(new QLabel("<small>提示: 需配置SMTP服务器才能发送邮件</small>", alarmWidget));

        tabWidget->addTab(alarmWidget, "报警配置");

        // ================== 底部按钮 ==================
        mainLayout->addWidget(tabWidget);

        auto *buttonLayout = new QHBoxLayout();
        auto *okButton = new QPushButton("确定", this);
        auto *cancelButton = new QPushButton("取消", this);

        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);

        mainLayout->addLayout(buttonLayout);

        // 连接信号
        connect(okButton, &QPushButton::clicked, this, &ConfigDialog::onSave);
        connect(cancelButton, &QPushButton::clicked, this, &ConfigDialog::onCancel);
    }

    void ConfigDialog::loadConfig()
    {
        auto &config = ConfigManager::getInstance();

        // 尝试加载配置文件
        config.loadConfig("config/system.conf");

        // ============ 加载PLC配置 ============
        m_plcIpEdit->setText(QString::fromStdString(config.getString("plc.ip", "192.168.0.1")));
        m_plcRackSpinBox->setValue(config.getInt("plc.rack", 0));
        m_plcSlotSpinBox->setValue(config.getInt("plc.slot", 1));

        // ============ 加载数据采集配置 ============
        m_collectionIntervalSpinBox->setValue(config.getInt("data.collection_interval", 1000));

        // ============ 加载压力传感器配置 ============
        m_pressureMaxLimitSpinBox->setValue(config.getFloat("pressure.max_limit", 10.0));
        m_pressureMinLimitSpinBox->setValue(config.getFloat("pressure.min_limit", 0.0));
        m_pressureAlarmThresholdSpinBox->setValue(config.getFloat("pressure.alarm_threshold", 8.0));

        // ============ 加载流量计配置 ============
        m_flowMaxRateSpinBox->setValue(config.getFloat("flow.max_rate", 100.0));
        m_flowAlarmThresholdSpinBox->setValue(config.getFloat("flow.alarm_threshold", 90.0));

        // ============ 加载系统配置 ============
        m_emergencyStopCheckBox->setChecked(config.getBool("system.emergency_stop_enabled", true));
        m_autoRecoveryCheckBox->setChecked(config.getBool("system.auto_recovery", false));

        QString logLevel = QString::fromStdString(config.getString("system.log_level", "info"));
        int logLevelIndex = m_logLevelComboBox->findText(logLevel);
        if (logLevelIndex >= 0)
        {
            m_logLevelComboBox->setCurrentIndex(logLevelIndex);
        }

        // ============ 加载报警配置 ============
        m_alarmSoundCheckBox->setChecked(config.getBool("alarm.enable_sound", true));
        m_alarmEmailCheckBox->setChecked(config.getBool("alarm.enable_email", false));
        m_emailAddressEdit->setText(QString::fromStdString(config.getString("alarm.email_address", "")));

        // 根据邮件报警开关状态设置邮箱输入框
        m_emailAddressEdit->setEnabled(m_alarmEmailCheckBox->isChecked());
    }

    void ConfigDialog::saveConfig()
    {
        auto &config = ConfigManager::getInstance();

        // ============ 保存PLC配置 ============
        config.setString("plc.ip", m_plcIpEdit->text().toStdString());
        config.setInt("plc.rack", m_plcRackSpinBox->value());
        config.setInt("plc.slot", m_plcSlotSpinBox->value());

        // ============ 保存数据采集配置 ============
        config.setInt("data.collection_interval", m_collectionIntervalSpinBox->value());

        // ============ 保存压力传感器配置 ============
        config.setFloat("pressure.max_limit", m_pressureMaxLimitSpinBox->value());
        config.setFloat("pressure.min_limit", m_pressureMinLimitSpinBox->value());
        config.setFloat("pressure.alarm_threshold", m_pressureAlarmThresholdSpinBox->value());

        // ============ 保存流量计配置 ============
        config.setFloat("flow.max_rate", m_flowMaxRateSpinBox->value());
        config.setFloat("flow.alarm_threshold", m_flowAlarmThresholdSpinBox->value());

        // ============ 保存系统配置 ============
        config.setBool("system.emergency_stop_enabled", m_emergencyStopCheckBox->isChecked());
        config.setBool("system.auto_recovery", m_autoRecoveryCheckBox->isChecked());
        config.setString("system.log_level", m_logLevelComboBox->currentText().toStdString());

        // ============ 保存报警配置 ============
        config.setBool("alarm.enable_sound", m_alarmSoundCheckBox->isChecked());
        config.setBool("alarm.enable_email", m_alarmEmailCheckBox->isChecked());
        config.setString("alarm.email_address", m_emailAddressEdit->text().toStdString());

        // 保存到文件
        if (config.saveConfig("config/system.conf"))
        {
            QMessageBox::information(this, "成功", "配置已保存！\n重新连接PLC后配置将生效。");
        }
        else
        {
            QMessageBox::warning(this, "警告", "配置保存失败！\n请检查config目录是否存在。");
        }
    }

    bool ConfigDialog::validateInput()
    {
        // 验证IP地址
        if (m_plcIpEdit->text().isEmpty())
        {
            QMessageBox::warning(this, "输入错误", "请输入PLC IP地址！");
            m_plcIpEdit->setFocus();
            return false;
        }

        // 验证IP地址格式
        if (!m_plcIpEdit->hasAcceptableInput())
        {
            QMessageBox::warning(this, "输入错误", "IP地址格式不正确！\n请输入有效的IPv4地址，例如: 192.168.0.1");
            m_plcIpEdit->setFocus();
            return false;
        }

        return true;
    }

    void ConfigDialog::onSave()
    {
        if (!validateInput())
            return;

        saveConfig();
        accept();
    }

    void ConfigDialog::onCancel()
    {
        reject();
    }

    void ConfigDialog::onTest()
    {
        if (!validateInput())
            return;

        // 创建临时PLC客户端测试连接（仅在失败时提示，不弹成功提示）

        auto plcClient = std::make_shared<S7PLCClient>();

        S7PLCClient::ConnectionParams params;
        params.ipAddress = m_plcIpEdit->text().toStdString();
        params.rack = m_plcRackSpinBox->value();
        params.slot = m_plcSlotSpinBox->value();
        params.timeout = 5000;

        if (plcClient->connect(params))
        {
            // 连接成功：不弹窗，仅断开临时连接
            plcClient->disconnect();
        }
        else
        {
            QString error = QString::fromStdString(plcClient->getLastError());
            QMessageBox::critical(this, "连接失败",
                                  "无法连接到PLC！\n\n错误: " + error +
                                      "\n\n请检查:\n• IP地址是否正确\n• PLC是否开机\n• 网络是否连通");
        }
    }

    QString ConfigDialog::getPlcIp() const
    {
        return m_plcIpEdit->text();
    }

    int ConfigDialog::getPlcRack() const
    {
        return m_plcRackSpinBox->value();
    }

    int ConfigDialog::getPlcSlot() const
    {
        return m_plcSlotSpinBox->value();
    }

    int ConfigDialog::getCollectionInterval() const
    {
        return m_collectionIntervalSpinBox->value();
    }

} // namespace WaterTest
