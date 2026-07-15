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
#include <algorithm>
#include <mutex>

using namespace WaterTest;

static std::ofstream g_crashLog;
static std::ofstream g_runtimeLog;
static std::mutex g_logMutex;

static void writeCrashLog(const char *msg)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
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

static void writeRuntimeLog(const char *msg)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_runtimeLog.is_open())
    {
        QDir().mkpath("deploy/logs");
        g_runtimeLog.open("deploy/logs/runtime.log", std::ios::app);
    }
    if (g_runtimeLog.is_open())
    {
        std::time_t t = std::time(nullptr);
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        g_runtimeLog << "[" << buf << "] " << msg << std::endl;
        g_runtimeLog.flush();
    }
    std::cout << msg << std::endl;
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
    case QtDebugMsg:
        writeRuntimeLog((std::string("[Qt DEBUG] ") + s).c_str());
        break;
    case QtInfoMsg:
        writeRuntimeLog((std::string("[Qt INFO] ") + s).c_str());
        break;
    case QtWarningMsg:
        writeRuntimeLog((std::string("[Qt WARN] ") + s).c_str());
        break;
    case QtFatalMsg:
        writeRuntimeLog((std::string("[Qt FATAL] ") + s).c_str());
        writeCrashLog((std::string("[Qt FATAL] ") + s).c_str());
        std::abort();
    case QtCriticalMsg:
        writeRuntimeLog((std::string("[Qt CRITICAL] ") + s).c_str());
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
                                      "Running mode: 'terminal' or 'station' (default: terminal)",
                                  "mode", "terminal");
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
        // ========== TERMINAL MODE (PC: GUI + PLC + TerminalServer) ==========
        qInfo() << "Starting Water Test System in TERMINAL mode (GUI + TerminalServer)...";

        try
        {
            MainWindow mainWindow;
            mainWindow.setWindowTitle("水介质电磁阀测试系统 - 主控台");

            auto deviceManager = mainWindow.getDeviceManager();
            auto server = std::make_shared<TerminalServer>(deviceManager);

            int port = parser.value(portOption).toInt();
            if (!server->startServer(port))
            {
                qWarning() << "[Terminal] Warning: TerminalServer failed to start on port" << port
                           << "- running without Station tablet support";
            }
            else
            {
                qInfo() << "[Terminal] TerminalServer listening on port" << port;
            }

            auto *serverPtr = server.get();
            QObject::connect(serverPtr, &TerminalServer::commandReceived, &app,
                             [deviceManager, serverPtr](uint8_t stationId, const ControlCommand &cmd)
                             {
                                 qInfo() << "[M100][Terminal] commandReceived"
                                         << "station=" << stationId
                                         << "type=" << cmd.command_type
                                         << "index=" << cmd.index
                                         << "action=" << cmd.action;

                                 bool ok = false;
                                 switch (cmd.command_type)
                                 {
                                 case 0:
                                     ok = deviceManager->setRelay(cmd.index, cmd.action != 0);
                                     break;
                                 case 1:
                                     ok = deviceManager->controlPump(static_cast<uint16_t>(cmd.index) + 1, cmd.action != 0);
                                     break;
                                 case 2:
                                     ok = deviceManager->controlValve(static_cast<uint16_t>(cmd.index) + 1, cmd.action != 0);
                                     break;
                                 case 3:
                                     ok = deviceManager->setDcPowerOutput(cmd.action != 0);
                                     break;
                                 case 4:
                                     ok = deviceManager->setDcPowerSetpoint(cmd.value1, cmd.value2);
                                     break;
                                 case 5:
                                     ok = deviceManager->setValveOpeningPercent(static_cast<uint16_t>(cmd.index) + 1, cmd.value1);
                                     break;
                                 case 6:
                                     switch (cmd.action)
                                     {
                                     case 1:
                                         ok = deviceManager->startPlcSelfCheck();
                                         break;
                                     case 2:
                                         ok = deviceManager->abortPlcSelfCheck();
                                         break;
                                     case 3:
                                         ok = deviceManager->resetPlcSelfCheck();
                                         break;
                                     case 4:
                                         ok = deviceManager->setPlcSelfCheckEnable(true);
                                         break;
                                     case 5:
                                         ok = deviceManager->setPlcSelfCheckEnable(false);
                                         break;
                                     default:
                                         ok = false;
                                         break;
                                     }
                                     break;
                                 default:
                                     break;
                                 }

                                 qInfo() << "[M100][Terminal] command dispatch result"
                                         << "station=" << stationId
                                         << "type=" << cmd.command_type
                                         << "index=" << cmd.index
                                         << "ok=" << ok;

                                 if (!ok)
                                 {
                                     qWarning() << "[Terminal] Command failed. station=" << stationId
                                                << "type=" << cmd.command_type << "idx=" << cmd.index;
                                 }
                                 if (stationId != 0)
                                 {
                                     serverPtr->sendCommandToStation(stationId, cmd);
                                 }
                             });

            mainWindow.setTerminalServer(server);

            QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                             [deviceManager]()
                             {
                                 deviceManager->stopDataCollection();
                             });

            mainWindow.showMaximized();
            qInfo() << "[Terminal] GUI ready. PC should connect PLC via UI connect/auto-connect.";
            return app.exec();
        }
        catch (const std::exception &e)
        {
            qCritical() << "Terminal mode error:" << QString::fromStdString(e.what());
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
            qWarning() << "The UI will keep running in local offline mode.";
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
