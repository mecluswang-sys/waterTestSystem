/**
 * @file S7PLCClient.h
 * @brief 西门子S7-1200 PLC通信客户端
 * @description 封装Snap7库，实现与S7-1200 PLC的通信功能
 */

#ifndef S7_PLC_CLIENT_H
#define S7_PLC_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

// Snap7 library headers (C API)
#ifdef SNAP7_FOUND
#include <snap7/snap7_libmain.h>
#include <snap7/s7_types.h>
#else
// Stub definitions for when Snap7 is not available
#define S7_OK 0
typedef void *S7Object;
typedef int S7_BYTE;
typedef short S7_WORD;
typedef int S7_DWORD;
#endif

namespace WaterTest
{

    class S7PLCClient
    {
    public:
        // 连接参数
        struct ConnectionParams
        {
            std::string ipAddress; // PLC IP地址
            int rack;              // 机架号 (通常为0)
            int slot;              // 槽号 (S7-1200通常为1)
            int timeout;           // 超时时间(ms)

            ConnectionParams() : ipAddress("192.168.0.1"), rack(0), slot(1), timeout(5000) {}
        };

        // 数据块读写结果
        enum class Result
        {
            SUCCESS = 0,
            CONNECTION_ERROR = 1,
            READ_ERROR = 2,
            WRITE_ERROR = 3,
            TIMEOUT = 4,
            INVALID_PARAMS = 5,
            SNAP7_NOT_AVAILABLE = 6 // Snap7 library not available
        };

        S7PLCClient();
        ~S7PLCClient();

        // 禁止拷贝
        S7PLCClient(const S7PLCClient &) = delete;
        S7PLCClient &operator=(const S7PLCClient &) = delete;

        /**
         * @brief 连接到PLC
         * @param params 连接参数
         * @return 是否连接成功
         */
        bool connect(const ConnectionParams &params);

        /**
         * @brief 断开连接
         */
        void disconnect();

        /**
         * @brief 检查是否已连接
         * @return 连接状态
         */
        bool isConnected() const;

        /**
         * @brief 读取DB块数据
         * @param dbNumber DB块号
         * @param start 起始字节
         * @param size 读取大小
         * @param buffer 数据缓冲区
         * @return 操作结果
         */
        Result readDB(int dbNumber, int start, int size, void *buffer);

        /**
         * @brief 写入DB块数据
         * @param dbNumber DB块号
         * @param start 起始字节
         * @param size 写入大小
         * @param buffer 数据缓冲区
         * @return 操作结果
         */
        Result writeDB(int dbNumber, int start, int size, const void *buffer);

        /**
         * @brief 读取Bool类型数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param bit 位偏移
         * @param value 输出值
         * @return 操作结果
         */
        Result readBool(int dbNumber, int start, int bit, bool &value);

        /**
         * @brief 写入Bool类型数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param bit 位偏移
         * @param value 写入值
         * @return 操作结果
         */
        Result writeBool(int dbNumber, int start, int bit, bool value);

        /**
         * @brief 读取Int16数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param value 输出值
         * @return 操作结果
         */
        Result readInt16(int dbNumber, int start, int16_t &value);

        /**
         * @brief 写入Int16数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param value 写入值
         * @return 操作结果
         */
        Result writeInt16(int dbNumber, int start, int16_t value);

        /**
         * @brief 读取Real(Float)数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param value 输出值
         * @return 操作结果
         */
        Result readReal(int dbNumber, int start, float &value);

        /**
         * @brief 写入Real(Float)数据
         * @param dbNumber DB块号
         * @param start 字节偏移
         * @param value 写入值
         * @return 操作结果
         */
        Result writeReal(int dbNumber, int start, float value);

        /**
         * @brief 读取输出区(Q)的Bool位，例如 Q0.0/Q0.1/Q0.2
         * @param byteOffset 字节偏移（Q0.* 对应 byteOffset=0）
         * @param bit 位偏移（Q0.0=0，Q0.1=1，...）
         * @param value 输出值
         * @return 操作结果
         */
        Result readOutputBool(int byteOffset, int bit, bool &value);

        /**
         * @brief 写入输出区(Q)的Bool位，例如 Q0.0/Q0.1/Q0.2
         * @param byteOffset 字节偏移（Q0.* 对应 byteOffset=0）
         * @param bit 位偏移（Q0.0=0，Q0.1=1，...）
         * @param value 写入值
         * @return 操作结果
         */
        Result writeOutputBool(int byteOffset, int bit, bool value);

        /**
         * @brief 读取 M 区（Merker）Bool 位，例如 M100.0
         * @param byteOffset 字节偏移（M100.* 对应 byteOffset=100）
         * @param bit 位偏移（M100.0=0，M100.1=1，...）
         * @param value 输出值
         * @return 操作结果
         */
        Result readMerkerBool(int byteOffset, int bit, bool &value);

        /**
         * @brief 写入 M 区（Merker）Bool 位，例如 M100.0
         * @param byteOffset 字节偏移（M100.* 对应 byteOffset=100）
         * @param bit 位偏移（M100.0=0，M100.1=1，...）
         * @param value 写入值
         * @return 操作结果
         */
        Result writeMerkerBool(int byteOffset, int bit, bool value);

        /**
         * @brief 读取 M 区（Merker）Real(Float) 数据，例如 MD21
         * @param byteOffset 字节偏移（MD21 对应 byteOffset=21）
         * @param value 输出值
         * @return 操作结果
         */
        Result readMerkerReal(int byteOffset, float &value);

        /**
         * @brief 写入 M 区（Merker）Real(Float) 数据，例如 MD26
         * @param byteOffset 字节偏移（MD26 对应 byteOffset=26）
         * @param value 写入值
         * @return 操作结果
         */
        Result writeMerkerReal(int byteOffset, float value);

        /**
         * @brief 读取外设输入区 (I/PE) 的 16-bit Word，用于读取 AI 模拟量反馈
         * @param byteOffset 字节偏移（如 IW64 对应 byteOffset=64）
         * @param value      输出的 16-bit 原始值（Siemens 工程量格式）
         * @return 操作结果
         */
        Result readPeripheralWord(int byteOffset, int16_t &value);

        /**
         * @brief 写入外设输出区 (Q/PQ) 的 16-bit Word，用于写入 AO 开度命令
         * @param byteOffset 字节偏移（如 QW80 对应 byteOffset=80）
         * @param value      16-bit 原始值（Siemens 工程量格式，4-20mA: 5530-27648）
         * @return 操作结果
         */
        Result writePeripheralWord(int byteOffset, int16_t value);

        /**
         * @brief 获取最后的错误信息
         * @return 错误描述
         */
        std::string getLastError() const;

        /**
         * @brief 获取连接信息
         * @return 连接信息字符串
         */
        std::string getConnectionInfo() const;

    private:
        S7Object m_client;          // Snap7 client handle (C API)
        ConnectionParams m_params;  // Connection parameters
        bool m_connected;           // Connection status
        mutable std::mutex m_mutex; // Thread lock
        std::string m_lastError;    // Last error message

        /**
         * @brief 更新错误信息
         * @param errorCode Snap7错误码
         */
        void updateLastError(int errorCode);

        /**
         * @brief 字节序转换（S7使用大端序）
         */
        static uint16_t swapUInt16(uint16_t value);
        static uint32_t swapUInt32(uint32_t value);
        static float swapFloat(float value);
    };

} // namespace WaterTest

#endif // S7_PLC_CLIENT_H
