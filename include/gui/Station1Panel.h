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
        explicit Station1Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~Station1Panel();

        // 为操作台远程模式注入主控客户端
        void setStationClient(std::shared_ptr<StationClient> stationClient);

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void showEvent(QShowEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        void setupUI();
        void buildScene();
        void applyAutoFit();
        void updatePipeFlowAnimation();
        void updateSensorValues();
        void onSelfCheck();

        // DQ 继电器控制
        void buildRelayPanel(QWidget *parent);
        void updateRelayButtons();
        void onRelayBtnClicked(uint8_t index, const char *source = "unknown");

        std::shared_ptr<DeviceManager> m_deviceManager;
        std::shared_ptr<StationClient> m_stationClient;
        QGraphicsView *m_view;
        QGraphicsScene *m_scene;
        QTimer *m_flowTimer;
        QTimer *m_dataTimer;
        qreal m_flowDashOffset;
        QPushButton *m_selfCheckBtn;

        // DQ 继电器按钮列表（与 kStation1Relays 同序）
        std::vector<QPushButton *> m_relayBtns;

        // M100.0 ~ M100.3 置位后若被 PLC 快速复位，用于触发可视化提示
        std::array<bool, 4> m_expectM100Hold{{false, false, false, false}};
        std::array<qint64, 4> m_expectM100SetMs{{0, 0, 0, 0}};

        // 图元点击防抖，避免单次物理点击触发多次 selectionChanged
        int m_lastRelayGlyphIndex = -1;
        qint64 m_lastRelayGlyphClickMs = 0;
        qint64 m_relayGlyphLockUntilMs = 0;
        std::array<qint64, 4> m_lastM100ToggleMs{{0, 0, 0, 0}};
    };

} // namespace WaterTest

#endif // STATION1_PANEL_H
