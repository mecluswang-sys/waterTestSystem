/**
 * @file MainWindow.h
 * @brief Main Window - Terminal Server & Station Client GUI
 */

#ifndef WATERTEST_GUI_MAINWINDOW_H
#define WATERTEST_GUI_MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <memory>

namespace WaterTest
{
    class DeviceManager;
    class TerminalServer;
    class StationClient;
    class MonitorPanel;
    class PreparationPanel;
    class TestPanel;
    class AutoTestPanel;
    class Station1Panel;

    // Forward declarations for Terminal GUI widgets
    class DashboardWidget;
    class PLCConnectionWidget;
    class StationManagerWidget;
    class DataMonitorWidget;
    class SystemLogsWidget;
    class SettingsWidget;

    /**
     * @class MainWindow
     * @brief Main application window supporting both Terminal Server and Station Client modes
     */
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        enum class WindowMode
        {
            STATION_MODE, // Operation station mode (existing)
            TERMINAL_MODE // Terminal server mode (new)
        };

        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        // Mode configuration
        void setWindowMode(WindowMode mode);
        void setTerminalServer(std::shared_ptr<TerminalServer> server);
        void setStationClient(std::shared_ptr<StationClient> client);
            std::shared_ptr<DeviceManager> getDeviceManager() const { return m_deviceManager; }

    protected:
        void closeEvent(QCloseEvent *event) override;

    private slots:
        // Station mode slots (existing)
        void onConnect();
        void onDisconnect();
        void onConfig();
        void onAbout();

        // Station mode helpers
        void onAutoConnectTick();
        void updatePlcStatusUi();
        void updateTerminalStatusUi();

        // Terminal mode slots (new)
        void onPLCConnected();
        void onPLCDisconnected();
        void onPLCError(const QString &error);
        void onStationConnected(uint8_t stationId);
        void onStationDisconnected(uint8_t stationId);
        void onDataReceived(const struct SensorData &data);
        void onTabChanged(int index);

    private:
        // UI creation methods
        void createMenuBar();
        void createStatusBar();
        void createStatusBarTerminal();
        void setupDeviceManager();
        void createTabWidgetStation();  // Station mode tabs
        void createTabWidgetTerminal(); // Terminal mode tabs (6 tabs)
        void applyAppTheme();

        // PLC connection helpers (station mode)
        bool connectPlc(bool interactive);
        void disconnectPlc();
        void setPlcStatusState(const char *state, const QString &toolTip);
        void startAutoConnect();

        // Mode-specific UI creation
        void createTerminalTabs();
        void setupTerminalConnections();

        // Member variables
        WindowMode m_mode = WindowMode::STATION_MODE;
        std::shared_ptr<DeviceManager> m_deviceManager;
        std::shared_ptr<TerminalServer> m_terminalServer;
        std::shared_ptr<StationClient> m_stationClient;

        // Common UI elements
        QTabWidget *m_tabWidget;

        // Station mode panels (existing)
        PreparationPanel *m_preparationPanel;
        MonitorPanel *m_monitorPanel;
        TestPanel *m_testPanel;
        AutoTestPanel *m_autoTestPanel;
        Station1Panel *m_station1Panel;

        QPushButton *m_connectBtn;
        QPushButton *m_disconnectBtn;
        QLabel *m_connectionStatusLabel;
        bool m_isConnected;

        // Station mode PLC status UI + auto connect
        QPushButton *m_plcStatusBtn;
        QTimer *m_plcStatusTimer;
        QTimer *m_terminalStatusTimer;
        QTimer *m_autoConnectTimer;
        bool m_autoConnectEnabled;
        bool m_autoConnecting;
        bool m_diagVirtualConnected;

        // Terminal mode widgets (new)
        DashboardWidget *m_dashboardWidget;
        PLCConnectionWidget *m_plcConnectionWidget;
        StationManagerWidget *m_stationManagerWidget;
        DataMonitorWidget *m_dataMonitorWidget;
        SystemLogsWidget *m_systemLogsWidget;
        SettingsWidget *m_settingsWidget;

        // Terminal mode status indicators
        QLabel *m_terminalStatusLabel;
        QLabel *m_plcStatusLabel;
        QLabel *m_stationStatusLabel;
        QLabel *m_dataStatsLabel;
        QProgressBar *m_dataCollectionProgressBar;

        // Menu actions
        QAction *m_connectAction;
        QAction *m_disconnectAction;
        QAction *m_exitAction;
        QAction *m_settingsAction;
        QAction *m_viewLogsAction;
        QAction *m_helpAction;
        QAction *m_aboutAction;
    };

} // namespace WaterTest

#endif // WATERTEST_GUI_MAINWINDOW_H
