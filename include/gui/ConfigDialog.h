/**
 * @file ConfigDialog.h
 * @brief 配置对话框 - 用于修改系统配置
 */

#ifndef WATERTEST_GUI_CONFIGDIALOG_H
#define WATERTEST_GUI_CONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTabWidget>

namespace WaterTest
{

    class ConfigDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit ConfigDialog(QWidget *parent = nullptr);
        ~ConfigDialog();

        // 获取配置值
        QString getPlcIp() const;
        int getPlcRack() const;
        int getPlcSlot() const;
        int getCollectionInterval() const;

    private slots:
        void onSave();
        void onCancel();
        void onTest();

    private:
        void setupUI();
        void loadConfig();
        void saveConfig();
        bool validateInput();

        // UI组件 - PLC连接
        QLineEdit *m_plcIpEdit;
        QSpinBox *m_plcRackSpinBox;
        QSpinBox *m_plcSlotSpinBox;
        QSpinBox *m_collectionIntervalSpinBox;

        // UI组件 - 压力传感器
        QDoubleSpinBox *m_pressureMaxLimitSpinBox;
        QDoubleSpinBox *m_pressureMinLimitSpinBox;
        QDoubleSpinBox *m_pressureAlarmThresholdSpinBox;

        // UI组件 - 流量计
        QDoubleSpinBox *m_flowMaxRateSpinBox;
        QDoubleSpinBox *m_flowAlarmThresholdSpinBox;

        // UI组件 - 系统配置
        QCheckBox *m_emergencyStopCheckBox;
        QCheckBox *m_autoRecoveryCheckBox;
        QComboBox *m_logLevelComboBox;

        // UI组件 - 报警配置
        QCheckBox *m_alarmSoundCheckBox;
        QCheckBox *m_alarmEmailCheckBox;
        QLineEdit *m_emailAddressEdit;
    };

} // namespace WaterTest

#endif // WATERTEST_GUI_CONFIGDIALOG_H
