/**
 * @file MonitorPanel.h
 * @brief Monitor Panel - Simplified Real-time Data Display
 */

#ifndef WATERTEST_GUI_MONITORPANEL_H
#define WATERTEST_GUI_MONITORPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QTimer>
#include <memory>

namespace WaterTest
{

    class DeviceManager;

    class MonitorPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit MonitorPanel(std::shared_ptr<DeviceManager> deviceMgr, QWidget *parent = nullptr);
        ~MonitorPanel();

        void startUpdate();
        void stopUpdate();

    private slots:
        void updateDisplay();

    private:
        void setupUI();
        void updateSensorTable();
        void updateFlowTable();
        void updateValveTable();
        void updatePumpTable();

        std::shared_ptr<DeviceManager> m_deviceManager;

        QTableWidget *m_sensorTable;
        QTableWidget *m_flowTable;
        QTableWidget *m_valveTable;
        QTableWidget *m_pumpTable;

        QTimer *m_updateTimer;
    };

} // namespace WaterTest

#endif // WATERTEST_GUI_MONITORPANEL_H
