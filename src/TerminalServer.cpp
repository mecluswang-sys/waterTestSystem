/**
 * @file TerminalServer.cpp
 * @brief Terminal server implementation
 */

#include "TerminalServer.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QDataStream>
#include <QDateTime>
#include <cstring>

namespace WaterTest
{

    TerminalServer::TerminalServer(std::shared_ptr<DeviceManager> deviceManager, QObject *parent)
        : QObject(parent), m_deviceManager(deviceManager), m_server(nullptr), m_isRunning(false)
    {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &TerminalServer::onNewConnection);

        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &TerminalServer::onDataPoll);
    }

    TerminalServer::~TerminalServer()
    {
        stopServer();
    }

    bool TerminalServer::startServer(int port)
    {
        if (m_isRunning)
        {
            return false;
        }

        if (!m_server->listen(QHostAddress::Any, port))
        {
            emit errorOccurred(QString("Failed to start server: %1").arg(m_server->errorString()));
            return false;
        }

        m_isRunning = true;
        m_pollTimer->start(100); // Poll every 100ms

        return true;
    }

    void TerminalServer::stopServer()
    {
        m_pollTimer->stop();

        // Disconnect all stations
        for (auto &[id, station] : m_stations)
        {
            if (station && station->socket())
            {
                station->socket()->disconnectFromHost();
            }
        }
        m_stations.clear();

        if (m_server)
        {
            m_server->close();
        }
        m_isRunning = false;
    }

    std::vector<uint8_t> TerminalServer::getConnectedStations() const
    {
        std::vector<uint8_t> ids;
        for (const auto &[id, station] : m_stations)
        {
            if (station && station->isValid())
            {
                ids.push_back(id);
            }
        }
        return ids;
    }

    void TerminalServer::broadcastSensorData(const SensorData &data)
    {
        NetworkMessage msg(MessageType::DATA_UPDATE, 0); // From terminal
        msg.header().payload_length = sizeof(SensorData);
        msg.header().sequence_number = getNextSequenceNumber();

        std::vector<uint8_t> &payload = msg.payload();
        payload.resize(sizeof(SensorData));
        std::memcpy(payload.data(), &data, sizeof(SensorData));

        for (auto &[id, station] : m_stations)
        {
            if (station && station->isValid())
            {
                station->sendMessage(msg);
            }
        }
    }

    bool TerminalServer::sendCommandToStation(uint8_t station_id, const ControlCommand &cmd)
    {
        auto it = m_stations.find(station_id);
        if (it == m_stations.end() || !it->second->isValid())
        {
            return false;
        }

        NetworkMessage msg(MessageType::COMMAND_RESPONSE, 0); // From terminal
        msg.header().payload_length = sizeof(ControlCommand);
        msg.header().sequence_number = getNextSequenceNumber();

        std::vector<uint8_t> &payload = msg.payload();
        payload.resize(sizeof(ControlCommand));
        std::memcpy(payload.data(), &cmd, sizeof(ControlCommand));

        return it->second->sendMessage(msg);
    }

    QString TerminalServer::getStationName(uint8_t station_id) const
    {
        auto it = m_stations.find(station_id);
        if (it != m_stations.end())
        {
            return it->second->getStationName();
        }
        return QString();
    }

    bool TerminalServer::isStationConnected(uint8_t station_id) const
    {
        auto it = m_stations.find(station_id);
        return it != m_stations.end() && it->second->isValid();
    }

    void TerminalServer::onNewConnection()
    {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket)
        {
            return;
        }

        connect(socket, &QTcpSocket::readyRead, this, &TerminalServer::onStationReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TerminalServer::onStationDisconnected);
    }

    void TerminalServer::onStationReadyRead()
    {
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
        if (!socket)
        {
            return;
        }

        // Find station by socket
        uint8_t station_id = 0;
        for (const auto &[id, station] : m_stations)
        {
            if (station && station->socket() == socket)
            {
                station_id = id;
                break;
            }
        }

        // Read data
        QByteArray data = socket->readAll();
        if (data.isEmpty())
        {
            return;
        }

        // Deserialize message
        std::vector<uint8_t> buffer(data.begin(), data.end());
        NetworkMessage message;
        if (!NetworkMessage::deserialize(buffer, message))
        {
            return;
        }

        // Handle message
        handleStationMessage(station_id, message);
    }

    void TerminalServer::onStationDisconnected()
    {
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
        if (!socket)
        {
            return;
        }

        // Find and remove station
        for (auto it = m_stations.begin(); it != m_stations.end(); ++it)
        {
            if (it->second && it->second->socket() == socket)
            {
                emit stationDisconnected(it->first);
                m_stations.erase(it);
                break;
            }
        }

        socket->deleteLater();
    }

    void TerminalServer::onDataPoll()
    {
        // Poll sensor data from PLC and broadcast to all stations
        if (m_deviceManager)
        {
            SensorData data{};
            const auto pressures = m_deviceManager->getAllPressureSensors();
            for (size_t i = 0; i < 4 && i < pressures.size(); ++i)
            {
                data.pressure[i] = pressures[i].pressure;
            }

            const auto temperatures = m_deviceManager->getAllTemperatureSensors();
            for (size_t i = 0; i < 4 && i < temperatures.size(); ++i)
            {
                data.temperature[i] = temperatures[i].temperature;
            }

            data.flow_rate = m_deviceManager->getFlowMeter(1).flowRate;
            data.timestamp = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);

            const auto selfCheck = m_deviceManager->getPlcSelfCheckStatus();
            data.selfcheck_busy = selfCheck.busy ? 1 : 0;
            data.selfcheck_done = selfCheck.done ? 1 : 0;
            data.selfcheck_passed = selfCheck.passed ? 1 : 0;
            data.selfcheck_failed = selfCheck.failed ? 1 : 0;
            data.selfcheck_step_no = selfCheck.stepNo;
            data.selfcheck_fault_code = selfCheck.faultCode;

            broadcastSensorData(data);
            emit dataReceived(data);
        }
    }

    void TerminalServer::handleStationMessage(uint8_t station_id, const NetworkMessage &message)
    {
        switch (message.header().type)
        {
        case MessageType::STATION_REGISTER:
        {
            if (station_id == 0 && message.payload().size() >= sizeof(StationRegister))
            {
                StationRegister reg;
                std::memcpy(&reg, message.payload().data(), sizeof(StationRegister));

                QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
                if (!socket)
                {
                    break;
                }

                auto station = std::make_shared<StationConnection>(socket, reg.station_id);
                station->setName(QString::fromLatin1(reg.station_name));
                station->setIpAddress(QString::fromLatin1(reg.ip_address));

                m_stations[reg.station_id] = station;
                emit stationConnected(reg.station_id, station->getStationName());
            }
            break;
        }

        case MessageType::COMMAND_REQUEST:
        {
            if (message.payload().size() >= sizeof(ControlCommand))
            {
                ControlCommand cmd;
                std::memcpy(&cmd, message.payload().data(), sizeof(ControlCommand));
                emit commandReceived(station_id, cmd);
            }
            break;
        }

        case MessageType::STATION_HEARTBEAT:
        {
            // Send ACK
            NetworkMessage ack(MessageType::ACK, 0);
            ack.header().sequence_number = getNextSequenceNumber();
            auto it = m_stations.find(station_id);
            if (it != m_stations.end())
            {
                it->second->sendMessage(ack);
            }
            break;
        }

        default:
            break;
        }
    }

    NetworkMessage TerminalServer::createDataUpdateMessage(const SensorData &data)
    {
        NetworkMessage msg(MessageType::DATA_UPDATE, 0);
        msg.header().payload_length = sizeof(SensorData);
        msg.header().sequence_number = getNextSequenceNumber();
        msg.payload().resize(sizeof(SensorData));
        std::memcpy(msg.payload().data(), &data, sizeof(SensorData));
        return msg;
    }

    uint16_t TerminalServer::getNextSequenceNumber()
    {
        return ++m_sequenceNumber;
    }

    // StationConnection implementation

    StationConnection::StationConnection(QTcpSocket *socket, uint8_t station_id)
        : m_socket(socket), m_stationId(station_id)
    {
    }

    StationConnection::~StationConnection()
    {
        if (m_socket)
        {
            m_socket->deleteLater();
        }
    }

    bool StationConnection::sendMessage(const NetworkMessage &message)
    {
        if (!isValid())
        {
            return false;
        }

        std::vector<uint8_t> data = message.serialize();
        return m_socket->write(reinterpret_cast<const char *>(data.data()), data.size()) > 0;
    }

} // namespace WaterTest
