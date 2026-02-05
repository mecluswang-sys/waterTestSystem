/**
 * @file NetworkProtocol.h
 * @brief Network communication protocol between Terminal and Stations
 * @description Defines message structures and communication protocol
 */

#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace WaterTest
{

    /**
     * @brief Message types for Terminal-Station communication
     */
    enum class MessageType : uint8_t
    {
        // Station registration and heartbeat
        STATION_REGISTER = 0x01,   // Station registers to Terminal
        STATION_HEARTBEAT = 0x02,  // Keep-alive message
        STATION_DISCONNECT = 0x03, // Station disconnects

        // Data sync messages
        DATA_UPDATE = 0x10,      // Terminal -> Station: Sensor data update
        COMMAND_REQUEST = 0x11,  // Station -> Terminal: Request to execute command
        COMMAND_RESPONSE = 0x12, // Terminal -> Station: Command execution result

        // Control messages
        RELAY_CONTROL = 0x20, // Control relay/solenoid
        PUMP_CONTROL = 0x21,  // Control pump
        VALVE_CONTROL = 0x22, // Control valve

        // System control
        START_TEST = 0x30,     // Start testing sequence
        STOP_TEST = 0x31,      // Stop testing sequence
        EMERGENCY_STOP = 0x32, // Emergency stop

        // Error/Status
        ERROR_MESSAGE = 0xFE, // Error notification
        ACK = 0xFF            // Acknowledgment
    };

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED_STRUCT
#else
#define PACKED_STRUCT __attribute__((packed))
#endif

    /**
     * @brief Message header (fixed 8 bytes)
     */
    struct MessageHeader
    {
        uint8_t magic_byte = 0xA5;           // Magic byte: 0xA5
        MessageType type = MessageType::ACK; // Message type (1 byte)
        uint16_t payload_length = 0;         // Payload length (2 bytes)
        uint16_t sequence_number = 0;        // Sequence number for tracking (2 bytes)
        uint8_t station_id = 0;              // Station ID (1-4), 0 for Terminal

        static constexpr size_t SIZE = 8;
    } PACKED_STRUCT;

    /**
     * @brief Sensor data structure
     */
    struct SensorData
    {
        float pressure[4];      // 4 pressure sensors (Pa)
        float temperature[4];   // 4 temperature sensors (°C)
        float flow_rate = 0.0f; // Flow rate
        uint32_t timestamp = 0; // Timestamp (ms)
    } PACKED_STRUCT;

    /**
     * @brief Control command structure
     */
    struct ControlCommand
    {
        uint8_t command_type = 0; // 0: Relay, 1: Pump, 2: Valve
        uint8_t index = 0;        // Device index
        uint8_t action = 0;       // 0: Off, 1: On
        uint32_t duration_ms = 0; // Optional duration in milliseconds
    } PACKED_STRUCT;

    /**
     * @brief Station registration payload
     */
    struct StationRegister
    {
        uint8_t station_id = 0;     // 1-4
        char station_name[32] = {}; // Station name (e.g., "Operation Station 1")
        uint16_t port = 0;          // Station's listening port
        char ip_address[16] = {};   // Station's IP address
    } PACKED_STRUCT;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

    /**
     * @brief Network message wrapper
     */
    class NetworkMessage
    {
    public:
        NetworkMessage() = default;

        explicit NetworkMessage(MessageType type, uint8_t station_id = 0)
            : header_{}
        {
            header_.type = type;
            header_.station_id = station_id;
        }

        // Getters
        MessageHeader &header() { return header_; }
        const MessageHeader &header() const { return header_; }
        std::vector<uint8_t> &payload() { return payload_; }
        const std::vector<uint8_t> &payload() const { return payload_; }

        // Serialization
        std::vector<uint8_t> serialize() const;
        static bool deserialize(const std::vector<uint8_t> &data, NetworkMessage &message);

    private:
        MessageHeader header_{};
        std::vector<uint8_t> payload_;
    };

} // namespace WaterTest

#endif // NETWORK_PROTOCOL_H
