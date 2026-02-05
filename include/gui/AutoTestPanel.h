/**
 * @file AutoTestPanel.h
 * @brief 自动化测试配置面板
 * @description 配置测试阀参数和自动测试条件
 */

#ifndef AUTO_TEST_PANEL_H
#define AUTO_TEST_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QRadioButton>
#include <memory>
#include <vector>

namespace WaterTest
{
    class DeviceManager;

    // 供电类型
    enum class PowerType
    {
        DC, // 直流供电
        AC  // 交流供电
    };

    // 测试类型
    enum class TestType
    {
        BY_COUNT,   // 按次数测试
        BY_DURATION // 按时长测试
    };

    // 测试条件结构
    struct TestCondition
    {
        QString name;         // 条件名称
        PowerType powerType;  // 供电类型 (AC/DC)
        float targetPressure; // 目标压力 (MPa)
        float flowRate;       // 流量 (L/min)
        float temperature;    // 温度 (°C)
        float valveVoltage;   // 阀门电压 (V)
        int cycleCount;       // 循环次数 (仅BY_COUNT类型使用)
        int durationSeconds;  // 测试时长(秒) (仅BY_DURATION类型使用)

        TestCondition()
            : powerType(PowerType::DC), targetPressure(0.5f), flowRate(10.0f), temperature(25.0f), valveVoltage(12.0f), cycleCount(100), durationSeconds(3600)
        {
        }
    };

    class AutoTestPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit AutoTestPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~AutoTestPanel();

        // 启动/停止更新
        void startUpdate(int intervalMs = 1000);
        void stopUpdate();

    private slots:
        void onUpdateData();
        void onTestValveOpen();
        void onTestValveClose();
        void onTestValveVoltageChanged(double value);
        void onPowerTypeChanged();
        void onAddTestCondition();
        void onEditTestCondition();
        void onDeleteTestCondition();
        void onStartAutoTest();
        void onTestTypeChanged();

    private:
        void setupUI();
        void updateTestConditionTable();
        void applyTestCondition(const TestCondition &condition);
        void executeAutoTest();

        // 设备管理器
        std::shared_ptr<DeviceManager> m_deviceManager;

        // 更新定时器
        QTimer *m_updateTimer;

        // ========== UI组件 ==========
        // 待测试阀控制组
        QGroupBox *m_testValveControlGroup;
        QRadioButton *m_powerTypeDC;               // DC供电选择
        QRadioButton *m_powerTypeAC;               // AC供电选择
        QDoubleSpinBox *m_testValveVoltageSpinBox; // 电压设置
        QPushButton *m_testValveOpenBtn;           // 开启按钮
        QPushButton *m_testValveCloseBtn;          // 关闭按钮
        QLabel *m_testValveVoltageLabel;           // 当前电压显示
        QLabel *m_testValveStatusLabel;            // 阀门状态

        // 测试条件配置组
        QGroupBox *m_testConditionGroup;
        QTableWidget *m_testConditionTable;  // 测试条件列表
        QPushButton *m_addConditionBtn;      // 添加条件
        QPushButton *m_editConditionBtn;     // 编辑条件
        QPushButton *m_deleteConditionBtn;   // 删除条件
        QRadioButton *m_testByCountRadio;    // 按次数测试
        QRadioButton *m_testByDurationRadio; // 按时长测试
        QPushButton *m_startAutoTestBtn;     // 开始自动测试
        QLabel *m_autoTestProgressLabel;     // 自动测试进度

        // 内部状态
        bool m_testValveOpen;                        // 待测试阀开关状态
        PowerType m_currentPowerType;                // 当前供电类型
        float m_testValveVoltage;                    // 待测试阀电压值
        std::vector<TestCondition> m_testConditions; // 测试条件列表
        TestType m_testType;                         // 测试类型
        bool m_isAutoTesting;                        // 是否在自动测试
        int m_currentConditionIndex;                 // 当前测试条件索引
        int m_currentCycleCount;                     // 当前循环次数
        int m_autoTestElapsedSeconds;                // 自动测试已经过时间
    };

} // namespace WaterTest

#endif // AUTO_TEST_PANEL_H
