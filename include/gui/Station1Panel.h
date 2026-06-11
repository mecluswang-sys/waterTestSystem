/**
 * @file Station1Panel.h
 * @brief 1号操作台面板（流程图展示）
 */

#ifndef STATION1_PANEL_H
#define STATION1_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <memory>
#include <vector>
#include <array>

class QShowEvent;
class QHideEvent;
class QTimer;
class QPushButton;
class QFrame;
class QGraphicsItem;
class QGraphicsPathItem;

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
        void setupStageOverview(QWidget *parent);
        void setActiveStageIndex(int stageIndex);
        void buildScene();
        void applyAutoFit();
        void setRealtimeUpdatesEnabled(bool enabled);
        void updatePipeFlowVisibility();
        void updatePipeFlowAnimation();
        void updateSensorValues(bool force = false);

        // DQ 继电器控制
        void buildRelayPanel(QWidget *parent);
        void updateRelayButtons(bool force = false);
        void controlAllRelayValves(bool open, const char *source = "ui");
        void onRelayBtnClicked(uint8_t index, const char *source = "unknown");
        bool controlRegulatingValveState(uint16_t id, bool open, const char *source, bool scheduleReconcile = true);
        bool controlRelayState(uint8_t index, bool on, const char *source, bool scheduleReconcile = true);
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
        bool m_pipeFlowAnimationEnabled;
        std::vector<QGraphicsPathItem *> m_flowPipeItems;
        std::array<QGraphicsItem *, 16> m_pressureSensorItems{};
        std::array<QGraphicsItem *, 16> m_valveItems{};
        std::array<QGraphicsItem *, 3> m_regulatingValveItems{};
        QGraphicsItem *m_flowMeterItem = nullptr;
        std::array<bool, 16> m_valveGlyphCacheInitialized{{false}};
        std::array<bool, 16> m_valveGlyphDisplayedOpen{{false}};
        std::array<bool, 16> m_valveGlyphPendingOpen{{false}};
        std::array<qint64, 16> m_valveGlyphPendingSinceMs{{0}};

        // DQ 继电器按钮列表（与 kStation1Relays 同序）
        std::vector<QPushButton *> m_relayBtns;
        QPushButton *m_startBtn;
        QPushButton *m_stopBtn;

        QGroupBox *m_stageOverviewGroup;
        std::array<QFrame *, 5> m_stageCardFrames{{nullptr, nullptr, nullptr, nullptr, nullptr}};
        std::array<QLabel *, 5> m_stageNameLabels{{nullptr, nullptr, nullptr, nullptr, nullptr}};
        std::array<QLabel *, 5> m_stageParamLabels{{nullptr, nullptr, nullptr, nullptr, nullptr}};
        std::array<std::array<QLabel *, 3>, 5> m_stageRowLabelLabels{{
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}}
        }};
        std::array<std::array<QFrame *, 3>, 5> m_stageRowFrames{{
            std::array<QFrame *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QFrame *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QFrame *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QFrame *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QFrame *, 3>{{nullptr, nullptr, nullptr}}
        }};
        std::array<std::array<QLabel *, 3>, 5> m_stageValueLabels{{
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}},
            std::array<QLabel *, 3>{{nullptr, nullptr, nullptr}}
        }};
        int m_activeStageIndex = 0;

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
