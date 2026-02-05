/**
 * @file NetworkProtocol.cpp
 * @brief Network protocol implementation
 */

#include "NetworkProtocol.h"
#include <cstring>

namespace WaterTest
{

    std::vector<uint8_t> NetworkMessage::serialize() const
    {
        std::vector<uint8_t> buffer;

        // Add header
        buffer.push_back(header_.magic_byte);
        buffer.push_back(static_cast<uint8_t>(header_.type));
        buffer.push_back((header_.payload_length >> 8) & 0xFF);
        buffer.push_back(header_.payload_length & 0xFF);
        buffer.push_back((header_.sequence_number >> 8) & 0xFF);
        buffer.push_back(header_.sequence_number & 0xFF);
        buffer.push_back(header_.station_id);

        // Add payload
        buffer.insert(buffer.end(), payload_.begin(), payload_.end());

        return buffer;
    }

    bool NetworkMessage::deserialize(const std::vector<uint8_t> &data, NetworkMessage &message)
    {
        if (data.size() < MessageHeader::SIZE)
        {
            return false;
        }

        // Parse header
        if (data[0] != 0xA5)
        { // Check magic byte
            return false;
        }

        message.header_.type = static_cast<MessageType>(data[1]);
        message.header_.payload_length = (static_cast<uint16_t>(data[2]) << 8) | data[3];
        message.header_.sequence_number = (static_cast<uint16_t>(data[4]) << 8) | data[5];
        message.header_.station_id = data[6];

        // Validate payload length
        if (data.size() < MessageHeader::SIZE + message.header_.payload_length)
        {
            return false;
        }

        // Extract payload
        if (message.header_.payload_length > 0)
        {
            message.payload_.assign(data.begin() + MessageHeader::SIZE,
                                    data.begin() + MessageHeader::SIZE + message.header_.payload_length);
        }

        return true;
    }

} // namespace WaterTest
