/**
 * @file Station1Panel.h
 * @brief 1号操作台面板（流程图展示）
 */

#ifndef STATION1_PANEL_H
#define STATION1_PANEL_H

#include <QWidget>
#include <memory>
#include <vector>
#include <array>

class QShowEvent;
class QHideEvent;
class QTimer;
class QPushButton;

class QGraphicsView;
class QGraphicsScene;

namespace WaterTest
{
    class DeviceManager;
    class StationClient;

    class Station1Panel : public QWidget
    {
        Q_OBJECT

    public:
        struct PanelConfig
        {
            int stationNumber = 1;
            std::array<uint16_t, 15> pressureSensorIds{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
            uint16_t flowMeterId = 1;
        };

        explicit Station1Panel(std::shared_ptr<DeviceManager> deviceManager,
                               QWidget *parent = nullptr,
                               const PanelConfig &panelConfig = PanelConfig());
        ~Station1Panel();

        // 为操作台远程模式注入主控客户端
        void setStationClient(std::shared_ptr<StationClient> stationClient);

        // 连接成功后立即同步一次图元状态（不依赖页面是否可见）
        void syncVisualStateOnce();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void showEvent(QShowEvent *event) override;
        void hideEvent(QHideEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

        virtual void onSelfCheck();
        
        // Protected members for derived classes to use
        std::shared_ptr<DeviceManager> m_deviceManager;
        std::shared_ptr<StationClient> m_stationClient;
        QPushButton *m_selfCheckBtn;
        PanelConfig m_panelConfig;

    private:
        void setupUI();
        void buildScene();
        void applyAutoFit();
        void setRealtimeUpdatesEnabled(bool enabled);
        void updatePipeFlowAnimation();
        void updateSensorValues(bool force = false);

        // DQ 继电器控制
        void buildRelayPanel(QWidget *parent);
        void updateRelayButtons(bool force = false);
        void onRelayBtnClicked(uint8_t index, const char *source = "unknown");
        void onStartButtonClicked(const char *source = "ui");
        void onStopButtonClicked(const char *source = "ui");
        bool controlStartStop(bool start, const char *source);
        void pollPhysicalStartStopButtons();

        QGraphicsView *m_view;
        QGraphicsScene *m_scene;
        QTimer *m_flowTimer;
        QTimer *m_dataTimer;
        QTimer *m_relayTimer;
        qreal m_flowDashOffset;

        // DQ 继电器按钮列表（与 kStation1Relays 同序）
        std::vector<QPushButton *> m_relayBtns;
        QPushButton *m_startBtn;
        QPushButton *m_stopBtn;

        // M100.0 ~ M100.3 置位后若被 PLC 快速复位，用于触发可视化提示
        std::array<bool, 4> m_expectM100Hold{{false, false, false, false}};
        std::array<qint64, 4> m_expectM100SetMs{{0, 0, 0, 0}};

        // 图元点击防抖，避免单次物理点击触发多次 selectionChanged
        int m_lastRelayGlyphIndex = -1;
        qint64 m_lastRelayGlyphClickMs = 0;
        qint64 m_relayGlyphLockUntilMs = 0;
        std::array<qint64, 4> m_lastM100ToggleMs{{0, 0, 0, 0}};
        bool m_lastStartPhysicalPressed = false;
        bool m_lastStopPhysicalPressed = false;
    };

} // namespace WaterTest

#endif // STATION1_PANEL_H
