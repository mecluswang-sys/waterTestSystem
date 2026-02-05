/**
 * @file S7PLCClient_stub.cpp
 * @brief Stub implementation of S7PLCClient (when Snap7 is not available)
 */

#include "S7PLCClient.h"

namespace WaterTest
{
    // Constructor
    S7PLCClient::S7PLCClient() : m_connected(false), m_client(nullptr) {}

    // Destructor
    S7PLCClient::~S7PLCClient() {}

    // Connection methods
    bool S7PLCClient::connect(const ConnectionParams &params)
    {
        m_connected = false; // Stub: always return false
        m_lastError = "Snap7 library not available. Running in stub mode.";
        return false;
    }

    void S7PLCClient::disconnect()
    {
        m_connected = false;
    }

    bool S7PLCClient::isConnected() const
    {
        return m_connected;
    }

    // Read methods
    S7PLCClient::Result S7PLCClient::readDB(int dbNumber, int start, int length, void *data)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::writeDB(int dbNumber, int start, int length, const void *data)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::readBool(int dbNumber, int offset, int bit, bool &value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::writeBool(int dbNumber, int offset, int bit, bool value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::readInt16(int dbNumber, int offset, short &value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::writeInt16(int dbNumber, int offset, short value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::readReal(int dbNumber, int offset, float &value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::writeReal(int dbNumber, int offset, float value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::readOutputBool(int dbNumber, int offset, bool &value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    S7PLCClient::Result S7PLCClient::writeOutputBool(int dbNumber, int offset, bool value)
    {
        return Result::SNAP7_NOT_AVAILABLE;
    }

    // Error handling
    std::string S7PLCClient::getLastError() const
    {
        return m_lastError;
    }

    std::string S7PLCClient::getConnectionInfo() const
    {
        return "Snap7 library not available (stub mode)";
    }

} // namespace WaterTest
