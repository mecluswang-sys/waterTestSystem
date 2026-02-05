/**
 * @file TerminalServer.h
 * @brief Terminal server that manages PLC connection and multiple stations
 * @description Central hub managing one PLC and up to 4 operation stations
 */

#ifndef TERMINAL_SERVER_H
#define TERMINAL_SERVER_H

#include "DeviceManager.h"
#include "NetworkProtocol.h"
#include <memory>
#include <map>
#include <QString>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace WaterTest
{

    class StationConnection;

    /**
     * @brief Terminal server managing PLC and station connections
     */
    class TerminalServer : public QObject
    {
        Q_OBJECT

    public:
        explicit TerminalServer(std::shared_ptr<DeviceManager> deviceManager, QObject *parent = nullptr);
        ~TerminalServer();

        // Server control
        bool startServer(int port = 5555);
        void stopServer();
        bool isRunning() const { return m_isRunning; }

        // Get connected stations
        std::vector<uint8_t> getConnectedStations() const;
        int getConnectedStationCount() const { return m_stations.size(); }

        // Broadcast sensor data to all stations
        void broadcastSensorData(const SensorData &data);

        // Send command to station
        bool sendCommandToStation(uint8_t station_id, const ControlCommand &cmd);

        // Get station info
        QString getStationName(uint8_t station_id) const;
        bool isStationConnected(uint8_t station_id) const;

    signals:
        void stationConnected(uint8_t station_id, const QString &name);
        void stationDisconnected(uint8_t station_id);
        void dataReceived(const SensorData &data);
        void commandReceived(uint8_t station_id, const ControlCommand &cmd);
        void errorOccurred(const QString &error);

    private slots:
        void onNewConnection();
        void onStationReadyRead();
        void onStationDisconnected();
        void onDataPoll();

    private:
        std::shared_ptr<DeviceManager> m_deviceManager;
        QTcpServer *m_server = nullptr;
        std::map<uint8_t, std::shared_ptr<StationConnection>> m_stations;
        QTimer *m_pollTimer = nullptr;
        bool m_isRunning = false;
        uint16_t m_sequenceNumber = 0;

        void handleStationMessage(uint8_t station_id, const NetworkMessage &message);
        NetworkMessage createDataUpdateMessage(const SensorData &data);
        uint16_t getNextSequenceNumber();
    };

    /**
     * @brief Represents a connected station
     */
    class StationConnection
    {
    public:
        explicit StationConnection(QTcpSocket *socket, uint8_t station_id);
        ~StationConnection();

        uint8_t getStationId() const { return m_stationId; }
        QString getStationName() const { return m_stationName; }
        QString getIpAddress() const { return m_ipAddress; }

        bool sendMessage(const NetworkMessage &message);
        void setName(const QString &name) { m_stationName = name; }
        void setIpAddress(const QString &ip) { m_ipAddress = ip; }

        bool isValid() const { return m_socket && m_socket->state() == QTcpSocket::ConnectedState; }
        QTcpSocket *socket() { return m_socket; }

    private:
        QTcpSocket *m_socket;
        uint8_t m_stationId;
        QString m_stationName;
        QString m_ipAddress;
    };

} // namespace WaterTest

#endif // TERMINAL_SERVER_H
