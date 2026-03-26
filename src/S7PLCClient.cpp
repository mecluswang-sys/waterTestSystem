/**
 * @file S7PLCClient.cpp
 * @brief Siemens S7-1200 PLC communication client implementation
 */

#include "S7PLCClient.h"
#ifdef SNAP7_FOUND
#include <snap7/snap7_libmain.h>
#endif
#include <cstring>
#include <sstream>
#include <iomanip>

namespace WaterTest
{

    S7PLCClient::S7PLCClient()
        : m_connected(false)
    {
        m_client = Cli_Create();
        if (!m_client)
        {
            m_lastError = "Failed to create Snap7 client handle";
        }
    }

    S7PLCClient::~S7PLCClient()
    {
        disconnect();
        if (m_client)
        {
            Cli_Destroy(m_client);
        }
    }

    bool S7PLCClient::connect(const ConnectionParams &params)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Guard against invalid handle and try to recover once.
        if (!m_client)
        {
            m_client = Cli_Create();
            if (!m_client)
            {
                m_connected = false;
                m_lastError = "Snap7 client handle is null";
                return false;
            }
        }

        if (m_connected)
        {
            disconnect();
        }

        m_params = params;

        // Connect to PLC (using C API)
        int result = Cli_ConnectTo(m_client,
                       m_params.ipAddress.c_str(),
                       m_params.rack,
                       m_params.slot);

        if (result == 0)
        {
            m_connected = true;
            m_lastError = "Connected successfully";
            return true;
        }
        else
        {
            m_connected = false;
            updateLastError(result);
            return false;
        }
    }

    void S7PLCClient::disconnect()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_connected && m_client)
        {
            Cli_Disconnect(m_client);
            m_connected = false;
        }
    }

    bool S7PLCClient::isConnected() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return m_connected && m_client != 0;
    }

    S7PLCClient::Result S7PLCClient::readDB(int dbNumber, int start, int size, void *buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        if (buffer == nullptr || size <= 0)
        {
            return Result::INVALID_PARAMS;
        }

        int result = Cli_DBRead(m_client, dbNumber, start, size, buffer);

        if (result == 0)
        {
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(result);
            return Result::READ_ERROR;
        }
    }

    S7PLCClient::Result S7PLCClient::writeDB(int dbNumber, int start, int size, const void *buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        if (buffer == nullptr || size <= 0)
        {
            return Result::INVALID_PARAMS;
        }

        int result = Cli_DBWrite(m_client, dbNumber, start, size, const_cast<void *>(buffer));

        if (result == 0)
        {
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(result);
            return Result::WRITE_ERROR;
        }
    }

    S7PLCClient::Result S7PLCClient::readBool(int dbNumber, int start, int bit, bool &value)
    {
        uint8_t buffer;
        Result result = readDB(dbNumber, start, 1, &buffer);

        if (result == Result::SUCCESS)
        {
            value = (buffer & (1 << bit)) != 0;
        }

        return result;
    }

    S7PLCClient::Result S7PLCClient::writeBool(int dbNumber, int start, int bit, bool value)
    {
        uint8_t buffer;
        Result result = readDB(dbNumber, start, 1, &buffer);

        if (result != Result::SUCCESS)
        {
            return result;
        }

        if (value)
        {
            buffer |= (1 << bit);
        }
        else
        {
            buffer &= ~(1 << bit);
        }

        return writeDB(dbNumber, start, 1, &buffer);
    }

    S7PLCClient::Result S7PLCClient::readInt16(int dbNumber, int start, int16_t &value)
    {
        uint16_t buffer;
        Result result = readDB(dbNumber, start, 2, &buffer);

        if (result == Result::SUCCESS)
        {
            // S7使用大端序，需要转换
            buffer = swapUInt16(buffer);
            value = static_cast<int16_t>(buffer);
        }

        return result;
    }

    S7PLCClient::Result S7PLCClient::writeInt16(int dbNumber, int start, int16_t value)
    {
        uint16_t buffer = swapUInt16(static_cast<uint16_t>(value));
        return writeDB(dbNumber, start, 2, &buffer);
    }

    S7PLCClient::Result S7PLCClient::readReal(int dbNumber, int start, float &value)
    {
        uint32_t buffer;
        Result result = readDB(dbNumber, start, 4, &buffer);

        if (result == Result::SUCCESS)
        {
            // S7使用大端序，需要转换
            buffer = swapUInt32(buffer);
            std::memcpy(&value, &buffer, sizeof(float));
        }

        return result;
    }

    S7PLCClient::Result S7PLCClient::writeReal(int dbNumber, int start, float value)
    {
        uint32_t buffer;
        std::memcpy(&buffer, &value, sizeof(float));
        buffer = swapUInt32(buffer);
        return writeDB(dbNumber, start, 4, &buffer);
    }

    S7PLCClient::Result S7PLCClient::readOutputBool(int byteOffset, int bit, bool &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        uint8_t buffer = 0;
        // 读取输出区（Q）：S7AreaPA，DBNumber忽略，用字节长度
        int result = Cli_ReadArea(m_client, S7AreaPA, 0, byteOffset, 1, S7WLByte, &buffer);
        if (result == 0)
        {
            value = (buffer & (1 << bit)) != 0;
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(result);
            return Result::READ_ERROR;
        }
    }

    S7PLCClient::Result S7PLCClient::writeOutputBool(int byteOffset, int bit, bool value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        uint8_t buffer = 0;
        int resRead = Cli_ReadArea(m_client, S7AreaPA, 0, byteOffset, 1, S7WLByte, &buffer);
        if (resRead != 0)
        {
            updateLastError(resRead);
            return Result::READ_ERROR;
        }

        if (value)
            buffer |= (1 << bit);
        else
            buffer &= ~(1 << bit);

        int resWrite = Cli_WriteArea(m_client, S7AreaPA, 0, byteOffset, 1, S7WLByte, &buffer);
        if (resWrite == 0)
        {
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(resWrite);
            return Result::WRITE_ERROR;
        }
    }

    S7PLCClient::Result S7PLCClient::readPeripheralWord(int byteOffset, int16_t &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        uint16_t buffer = 0;
        // S7AreaPE = 外设输入区（AI 映射）
        int result = Cli_ReadArea(m_client, S7AreaPE, 0, byteOffset, 2, S7WLByte, &buffer);
        if (result == 0)
        {
            value = static_cast<int16_t>(swapUInt16(buffer));
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(result);
            return Result::READ_ERROR;
        }
    }

    S7PLCClient::Result S7PLCClient::writePeripheralWord(int byteOffset, int16_t value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_client)
        {
            m_lastError = "Snap7 client handle is null";
            return Result::CONNECTION_ERROR;
        }

        if (!m_connected)
        {
            return Result::CONNECTION_ERROR;
        }

        uint16_t buffer = swapUInt16(static_cast<uint16_t>(value));
        // S7AreaPA = 外设输出区（AO 映射）
        int result = Cli_WriteArea(m_client, S7AreaPA, 0, byteOffset, 2, S7WLByte, &buffer);
        if (result == 0)
        {
            return Result::SUCCESS;
        }
        else
        {
            updateLastError(result);
            return Result::WRITE_ERROR;
        }
    }

    std::string S7PLCClient::getLastError() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastError;
    }

    std::string S7PLCClient::getConnectionInfo() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::stringstream ss;
        ss << "PLC Address: " << m_params.ipAddress
           << ", Rack: " << m_params.rack
           << ", Slot: " << m_params.slot
           << ", Status: " << (m_connected ? "Connected" : "Disconnected");

        return ss.str();
    }

    void S7PLCClient::updateLastError(int errorCode)
    {
        char errorText[1024];
        Cli_ErrorText(errorCode, errorText, sizeof(errorText));
        m_lastError = std::string(errorText);
    }

    uint16_t S7PLCClient::swapUInt16(uint16_t value)
    {
        return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
    }

    uint32_t S7PLCClient::swapUInt32(uint32_t value)
    {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    }

    float S7PLCClient::swapFloat(float value)
    {
        uint32_t temp;
        std::memcpy(&temp, &value, sizeof(float));
        temp = swapUInt32(temp);
        std::memcpy(&value, &temp, sizeof(float));
        return value;
    }

} // namespace WaterTest
