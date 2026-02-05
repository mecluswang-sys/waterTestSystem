/**
 * @file StationClient.cpp
 * @brief Station client implementation
 */

#include "StationClient.h"
#include <QHostAddress>
#include <cstring>

namespace WaterTest
{

    StationClient::StationClient(uint8_t station_id, const QString &station_name, QObject *parent)
        : QObject(parent), m_stationId(station_id), m_stationName(station_name)
    {
        m_socket = new QTcpSocket(this);
        // Use a lambda to handle the error signal
        connect(m_socket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::errorOccurred),
                this, &StationClient::onError);
        connect(m_socket, &QTcpSocket::connected, this, &StationClient::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &StationClient::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &StationClient::onReadyRead);

        m_heartbeatTimer = new QTimer(this);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &StationClient::onHeartbeat);
    }

    StationClient::~StationClient()
    {
        disconnectFromTerminal();
    }

    bool StationClient::connectToTerminal(const QString &host, int port)
    {
        if (isConnected())
        {
            return true;
        }

        m_terminalHost = host;
        m_terminalPort = port;

        m_socket->connectToHost(host, port);
        return m_socket->waitForConnected(3000);
    }

    void StationClient::disconnectFromTerminal()
    {
        m_heartbeatTimer->stop();
        if (m_socket && m_socket->state() == QTcpSocket::ConnectedState)
        {
            m_socket->disconnectFromHost();
        }
    }

    bool StationClient::isConnected() const
    {
        return m_socket && m_socket->state() == QTcpSocket::ConnectedState;
    }

    bool StationClient::sendCommand(const ControlCommand &cmd)
    {
        if (!isConnected())
        {
            emit errorOccurred("Not connected to terminal");
            return false;
        }

        NetworkMessage msg(MessageType::COMMAND_REQUEST, m_stationId);
        msg.header().payload_length = sizeof(ControlCommand);
        msg.payload().resize(sizeof(ControlCommand));
        std::memcpy(msg.payload().data(), &cmd, sizeof(ControlCommand));

        return sendMessage(msg);
    }

    void StationClient::onConnected()
    {
        // Send registration
        NetworkMessage msg(MessageType::STATION_REGISTER, m_stationId);
        StationRegister reg;
        reg.station_id = m_stationId;
        strncpy(reg.station_name, m_stationName.toLatin1().data(), sizeof(reg.station_name) - 1);
        reg.station_name[sizeof(reg.station_name) - 1] = '\0';
        reg.port = 0; // Client doesn't listen
        strncpy(reg.ip_address, "127.0.0.1", sizeof(reg.ip_address) - 1);

        msg.header().payload_length = sizeof(StationRegister);
        msg.payload().resize(sizeof(StationRegister));
        std::memcpy(msg.payload().data(), &reg, sizeof(StationRegister));

        if (sendMessage(msg))
        {
            m_heartbeatTimer->start(5000); // Heartbeat every 5 seconds
            emit connected();
        }
    }

    void StationClient::onDisconnected()
    {
        m_heartbeatTimer->stop();
        emit disconnected();
    }

    void StationClient::onReadyRead()
    {
        QByteArray data = m_socket->readAll();
        if (data.isEmpty())
        {
            return;
        }

        std::vector<uint8_t> buffer(data.begin(), data.end());
        NetworkMessage message;
        if (!NetworkMessage::deserialize(buffer, message))
        {
            return;
        }

        handleMessage(message);
    }

    void StationClient::onError()
    {
        emit errorOccurred(m_socket->errorString());
    }

    void StationClient::onHeartbeat()
    {
        if (!isConnected())
        {
            return;
        }

        NetworkMessage msg(MessageType::STATION_HEARTBEAT, m_stationId);
        sendMessage(msg);
    }

    void StationClient::handleMessage(const NetworkMessage &message)
    {
        switch (message.header().type)
        {
        case MessageType::DATA_UPDATE:
        {
            if (message.payload().size() >= sizeof(SensorData))
            {
                std::memcpy(&m_latestData, message.payload().data(), sizeof(SensorData));
                emit dataUpdated(m_latestData);
            }
            break;
        }

        case MessageType::COMMAND_RESPONSE:
        {
            emit commandResponse(true);
            break;
        }

        case MessageType::ACK:
        {
            // Heartbeat acknowledged
            break;
        }

        case MessageType::ERROR_MESSAGE:
        {
            QString error(reinterpret_cast<const char *>(message.payload().data()));
            emit errorOccurred(error);
            break;
        }

        default:
            break;
        }
    }

    bool StationClient::sendMessage(const NetworkMessage &message)
    {
        if (!isConnected())
        {
            return false;
        }

        std::vector<uint8_t> data = message.serialize();
        return m_socket->write(reinterpret_cast<const char *>(data.data()), data.size()) > 0;
    }

    uint16_t StationClient::getNextSequenceNumber()
    {
        return ++m_sequenceNumber;
    }

} // namespace WaterTest
