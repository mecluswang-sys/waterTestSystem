/**
 * @file TestPanel.h
 * @brief 测试区面板
 * @description 显示从分水罐出来经过完整测试管路的流程
 */

#ifndef TEST_PANEL_H
#define TEST_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QTimer>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QRadioButton>
#include <memory>
#include <vector>

namespace WaterTest
{
    class DeviceManager;

    class TestPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit TestPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~TestPanel();

        // 启动/停止更新
        void startUpdate(int intervalMs = 1000);
        void stopUpdate();

    private slots:
        void onUpdateData();
        void onStartTest();
        void onStopTest();
        void onEmergencyStop();

    private:
        void setupUI();
        void createFlowDiagram();
        void updateDeviceStatus();
        void updateValveStatus(int valveId, QLabel *statusLabel);
        void updatePressureSensor(int sensorId, QLabel *valueLabel);
        void updateTopOverview();
        void updateFlowDiagramDynamic();
        void initTestRecordUI();
        void appendTestRecord(const QString &event);
        void updateSolenoidStates();

        // 设备管理器
        std::shared_ptr<DeviceManager> m_deviceManager;

        // 更新定时器
        QTimer *m_updateTimer;

        // ========== UI组件 ==========
        // 流程图显示
        QGroupBox *m_flowDiagramGroup;
        QLabel *m_flowDiagramLabel;

        // 顶部总览显示块
        QGroupBox *m_topOverviewGroup;
        QLabel *m_pumpBlock;
        QLabel *m_p1t1Block;
        QLabel *m_valve1Block;
        QLabel *m_p2t2Block;
        QLabel *m_flowBlock;
        QLabel *m_valve2Block;
        QLabel *m_p3t3Block;
        QLabel *m_dutValveBlock;
        QLabel *m_p4t4Block;

        // 设备状态显示区域
        QGroupBox *m_valveStatusGroup;  // 电动阀状态
        QGroupBox *m_sensorStatusGroup; // 压力传感器状态
        QGroupBox *m_flowMeterGroup;    // 流量计状态
        QGroupBox *m_otherDevicesGroup; // 其他设备（调压阀、温度传感器等）

        // 电动阀状态标签（11个）
        QLabel *m_valve2Label; // 分水罐出口阀
        QLabel *m_valve3Label;
        QLabel *m_valve4Label;
        QLabel *m_valve5Label;
        QLabel *m_valve6Label;
        QLabel *m_valve7Label;
        QLabel *m_valve8Label;
        QLabel *m_valve9Label;
        QLabel *m_valve10Label;
        QLabel *m_valve11Label;

        // 压力传感器标签
        QLabel *m_pressure2Label;
        QLabel *m_pressure3Label;
        QLabel *m_pressure4Label;
        QLabel *m_pressure5Label;
        QLabel *m_pressure6Label;
        QLabel *m_pressure7Label;
        QLabel *m_pressure8Label;

        // 流量计标签
        QLabel *m_flowMeterLabel;
        QLabel *m_flowMeter2Label;

        // 其他设备
        QLabel *m_regulatingValve1Label; // 电动调压镠1
        QLabel *m_regulatingValve2Label; // 电动调压镠2
        QLabel *m_tempSensorLabel;       // 温度传感器
        QLabel *m_testValveLabel;        // 待测试阀状态

        // 控制按钮
        QPushButton *m_startTestBtn;
        QPushButton *m_stopTestBtn;
        QPushButton *m_emergencyStopBtn;

        // 电磁阀控制 (D0.0 / D0.1 / D0.2)
        QGroupBox *m_solenoidGroup;
        QPushButton *m_solenoid0Btn;
        QPushButton *m_solenoid1Btn;
        QPushButton *m_solenoid2Btn;

        // 状态信息
        QLabel *m_statusLabel;
        QLabel *m_testTimeLabel;

        // 测试记录
        QGroupBox *m_testRecordGroup;
        QTableWidget *m_testRecordTable;

        // 内部状态
        bool m_isTesting;
        int m_testTimeSeconds;
    };

} // namespace WaterTest

#endif // TEST_PANEL_H
