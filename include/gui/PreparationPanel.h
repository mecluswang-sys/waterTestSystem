/**
 * @file PreparationPanel.h
 * @brief 测试准备区面板
 * @description 显示从室外水池加水到分水罐的准备流程
 */

#ifndef PREPARATION_PANEL_H
#define PREPARATION_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDateTime>
#include <QList>
#include <memory>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsItem;

namespace WaterTest
{
    class DeviceManager;

    class PreparationPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit PreparationPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~PreparationPanel();

        // 启动/停止更新
        void startUpdate(int intervalMs = 1000);
        void stopUpdate();

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private slots:
        void onUpdateData();
        void onSelfCheck();
        void onStartFilling();
        void onStopFilling();
        void onEmergencyStop();
        void onReliefValveToggled(bool open);
        void onPumpFrequencyChanged(double value);
        void onTargetPressureChanged(double value);
        void onRelay1Toggled(bool on);
        void onRelay2Toggled(bool on);
        void onRelay3Toggled(bool on);

    private:
        void setupUI();
        void updatePumpStatus();
        void updateValveStatus();
        void updateWaterLevel();
        void updateRelayStates();
        void updateClock();
        void updateReliefValveStatus();
        void buildHmiScene();

        // 设备管理器
        std::shared_ptr<DeviceManager> m_deviceManager;

        // 更新定时器
        QTimer *m_updateTimer;

        // ========== UI组件（HMI流程图） ==========
        QLabel *m_clockLabel;

        QGraphicsView *m_view;
        QGraphicsScene *m_scene;

        // HMI拟物设备图元（用于随数据刷新渲染状态）
        QGraphicsItem *m_itemPump1;
        QGraphicsItem *m_itemPump2;
        QGraphicsItem *m_itemValve1;
        QGraphicsItem *m_itemValve2;
        QGraphicsItem *m_itemPS1;
        QGraphicsItem *m_itemPS2;
        QGraphicsItem *m_itemPS3;
        QGraphicsItem *m_itemTank;
        QGraphicsItem *m_itemOutdoorPool;
        // 动态管道
        QList<QGraphicsItem *> m_pipes;

        // 顶部/底部固定区
        void onUpdatePipes();
        QLabel *m_statusBadge;
        QLabel *m_fillingTimeLabel;
        QProgressBar *m_tankLevelBar;
        QLabel *m_tankLevelText;

        // 参数
        QDoubleSpinBox *m_pumpFrequencySpinBox;
        QDoubleSpinBox *m_targetPressureSpinBox;

        // 操作按钮
        QPushButton *m_selfCheckBtn;
        QPushButton *m_startFillingBtn;
        QPushButton *m_stopFillingBtn;
        QPushButton *m_reliefValveBtn;      // 作为“长按开启泄压阀”按钮（危险）
        QPushButton *m_reliefValveCloseBtn; // 关闭泄压阀
        QPushButton *m_emergencyStopBtn;    // 作为“长按紧急停止”按钮（危险）

        // 继电器（准备区当前不显示，但保留接口兼容）
        QCheckBox *m_relay1Check;
        QCheckBox *m_relay2Check;
        QCheckBox *m_relay3Check;

        // 内部状态
        bool m_isFilling;
        int m_fillingTimeSeconds;
        float m_pumpFrequency;   // 变频泵频率（Hz）
        float m_targetPressure;  // 目标压力（MPa）
        float m_currentPressure; // 当前压力（MPa）

        // 样式
        QString getStatusColor(bool isGood) const;
        QString getValveColor(bool isOpen) const;
    };

} // namespace WaterTest

#endif // PREPARATION_PANEL_H
