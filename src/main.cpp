/**
 * @file main.cpp
 * @brief Water Medium Test System Main Program
 * @description PC host computer program with Qt GUI, communicating with Siemens S7-1200 PLC
 *              Supports both Terminal Server and Station Client modes
 */

#include "gui/MainWindow.h"
#include "TerminalServer.h"
#include "StationClient.h"
#include "DeviceManager.h"
#include "ConfigManager.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <exception>
#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>

using namespace WaterTest;

static std::ofstream g_crashLog;

static void writeCrashLog(const char *msg)
{
    if (!g_crashLog.is_open())
    {
        QDir().mkpath("deploy/logs");
        g_crashLog.open("deploy/logs/crash.log", std::ios::app);
    }
    if (g_crashLog.is_open())
    {
        std::time_t t = std::time(nullptr);
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        g_crashLog << "[" << buf << "] " << msg << std::endl;
        g_crashLog.flush();
    }
    std::cerr << msg << std::endl;
}

static void terminateHandler()
{
    const char *msg = "[CRASH] std::terminate called";
    if (auto eptr = std::current_exception())
    {
        try { std::rethrow_exception(eptr); }
        catch (const std::exception &e)
        {
            static char buf[512];
            snprintf(buf, sizeof(buf), "[CRASH] Unhandled exception: %s", e.what());
            writeCrashLog(buf);
        }
        catch (...) { writeCrashLog("[CRASH] Unhandled unknown exception"); }
    }
    else
    {
        writeCrashLog(msg);
    }
    std::abort();
}

static void qtMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const std::string s = msg.toStdString();
    switch (type)
    {
    case QtFatalMsg:
        writeCrashLog((std::string("[Qt FATAL] ") + s).c_str());
        std::abort();
    case QtCriticalMsg:
        writeCrashLog((std::string("[Qt CRITICAL] ") + s).c_str());
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Install crash handlers early
    std::set_terminate(terminateHandler);
    qInstallMessageHandler(qtMessageHandler);
    writeCrashLog("[INFO] Application started");
    const bool configOk = ConfigManager::getInstance().loadConfig("config/system.conf");
    qInfo() << "[Config] load config/system.conf" << (configOk ? "OK" : "FAIL");

    app.setApplicationName("Water Test System");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("WaterTest");

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Water Test System - Terminal/Station Architecture");
    parser.addHelpOption();
    parser.addVersionOption();

    // Mode option
    QCommandLineOption modeOption(QStringList() << "m" << "mode",
                                  "Running mode: 'terminal' or 'station' (default: station)",
                                  "mode", "station");
    parser.addOption(modeOption);

    // Station ID (for station mode)
    QCommandLineOption idOption(QStringList() << "id",
                                "Station ID (1-4, only for station mode)",
                                "id", "1");
    parser.addOption(idOption);

    // Station name
    QCommandLineOption nameOption(QStringList() << "name",
                                  "Station name (for station mode)",
                                  "name", "Operation Station");
    parser.addOption(nameOption);

    // Terminal server host (for station mode)
    QCommandLineOption hostOption(QStringList() << "host",
                                  "Terminal server host (for station mode)",
                                  "host", "127.0.0.1");
    parser.addOption(hostOption);

    // Terminal server port
    QCommandLineOption portOption(QStringList() << "port",
                                  "Terminal server port (default: 5555)",
                                  "port", "5555");
    parser.addOption(portOption);

    parser.process(app);

    // Determine running mode
    QString mode = parser.value(modeOption).toLower();

    if (mode == "terminal")
    {
        // ========== TERMINAL MODE ==========
        qInfo() << "Starting Water Test System in TERMINAL mode...";

        try
        {
            auto deviceManager = std::make_shared<DeviceManager>();
            auto server = std::make_unique<TerminalServer>(deviceManager);

            int port = parser.value(portOption).toInt();
            if (!server->startServer(port))
            {
                qWarning() << "Failed to start Terminal Server on port" << port;
                return 1;
            }

            qInfo() << "Terminal Server started successfully on port" << port;
            qInfo() << "Waiting for Station connections...";

            // Keep the application running
            return app.exec();
        }
        catch (const std::exception &e)
        {
            qCritical() << "Terminal Server error:" << QString::fromStdString(e.what());
            return 1;
        }
    }
    else if (mode == "station")
    {
        // ========== STATION MODE ==========
        bool ok = true;
        int stationId = parser.value(idOption).toInt(&ok);

        if (!ok || stationId < 1 || stationId > 4)
        {
            qWarning() << "Invalid station ID. Must be 1-4.";
            return 1;
        }

        QString stationName = parser.value(nameOption);
        if (stationName.isEmpty())
        {
            stationName = QString("Operation Station %1").arg(stationId);
        }

        QString host = parser.value(hostOption);
        int port = parser.value(portOption).toInt();

        qInfo() << QString("Starting Water Test System in STATION mode...")
                << QString("\nStation ID: %1").arg(stationId)
                << QString("\nStation Name: %1").arg(stationName)
                << QString("\nConnecting to Terminal: %1:%2").arg(host).arg(port);

        auto stationClient = std::make_shared<StationClient>(
            static_cast<uint8_t>(stationId), stationName);

        // Create and show main window (as Station Client UI)
        MainWindow mainWindow;
        mainWindow.setStationClient(stationClient);
        mainWindow.setWindowTitle(QString("Water Test System - %1").arg(stationName));

        if (!stationClient->connectToTerminal(host, port))
        {
            qWarning() << "Station failed to connect to Terminal server" << host << port;
            qWarning() << "The UI will keep running in standalone mode.";
        }

        mainWindow.showMaximized();

        qInfo() << "Station GUI started. Ready for user interaction.";

        return app.exec();
    }
    else
    {
        qWarning() << "Invalid mode:" << mode;
        qWarning() << "Valid modes: 'terminal' or 'station'";
        parser.showHelp(1);
        return 1;
    }
}
