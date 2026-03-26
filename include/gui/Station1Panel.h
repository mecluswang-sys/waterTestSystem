/**
 * @file Station1Panel.h
 * @brief 1号操作台面板（流程图展示）
 */

#ifndef STATION1_PANEL_H
#define STATION1_PANEL_H

#include <QWidget>
#include <memory>
#include <vector>

class QShowEvent;
class QTimer;
class QPushButton;

class QGraphicsView;
class QGraphicsScene;

namespace WaterTest
{
    class DeviceManager;

    class Station1Panel : public QWidget
    {
        Q_OBJECT

    public:
        explicit Station1Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~Station1Panel();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void showEvent(QShowEvent *event) override;

    private:
        void setupUI();
        void buildScene();
        void applyAutoFit();
        void updatePipeFlowAnimation();
        void updateSensorValues();

        // DQ 继电器控制
        void buildRelayPanel(QWidget *parent);
        void updateRelayButtons();
        void onRelayBtnClicked(uint8_t index);

        std::shared_ptr<DeviceManager> m_deviceManager;
        QGraphicsView *m_view;
        QGraphicsScene *m_scene;
        QTimer *m_flowTimer;
        QTimer *m_dataTimer;
        qreal m_flowDashOffset;

        // DQ 继电器按钮列表（与 kStation1Relays 同序）
        std::vector<QPushButton *> m_relayBtns;
    };

} // namespace WaterTest

#endif // STATION1_PANEL_H
