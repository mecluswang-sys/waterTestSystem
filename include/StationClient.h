/**
 * @file StationClient.h
 * @brief Station client that connects to Terminal server
 * @description Client application for operation station connecting to Terminal
 */

#ifndef STATION_CLIENT_H
#define STATION_CLIENT_H

#include "NetworkProtocol.h"
#include <memory>
#include <QString>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace WaterTest
{

    /**
     * @brief Station client connecting to Terminal server
     */
    class StationClient : public QObject
    {
        Q_OBJECT

    public:
        explicit StationClient(uint8_t station_id, const QString &station_name, QObject *parent = nullptr);
        ~StationClient();

        // Connection control
        bool connectToTerminal(const QString &host, int port = 5555);
        void disconnectFromTerminal();
        bool isConnected() const;

        // Getters
        uint8_t getStationId() const { return m_stationId; }
        QString getStationName() const { return m_stationName; }
        QString getTerminalHost() const { return m_terminalHost; }

        // Send command to terminal
        bool sendCommand(const ControlCommand &cmd);

        // Get latest sensor data
        SensorData getLatestSensorData() const { return m_latestData; }

    signals:
        void connected();
        void disconnected();
        void dataUpdated(const SensorData &data);
        void commandResponse(bool success);
        void errorOccurred(const QString &error);

    private slots:
        void onConnected();
        void onDisconnected();
        void onReadyRead();
        void onError();
        void onHeartbeat();
        void onReconnectTimer();

    private:
        uint8_t m_stationId;
        QString m_stationName;
        QTcpSocket *m_socket = nullptr;
        QString m_terminalHost;
        int m_terminalPort = 5555;
        QTimer *m_heartbeatTimer = nullptr;
        QTimer *m_reconnectTimer = nullptr;
        bool m_autoReconnect = false;
        SensorData m_latestData{};
        uint16_t m_sequenceNumber = 0;

        void handleMessage(const NetworkMessage &message);
        bool sendMessage(const NetworkMessage &message);
        uint16_t getNextSequenceNumber();
    };

} // namespace WaterTest

#endif // STATION_CLIENT_H
