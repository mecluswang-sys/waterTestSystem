/**
 * @file DeviceManager.cpp
 * @brief Device Manager Implementation
 */

#include "DeviceManager.h"
#include "ConfigManager.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <QtSerialPort/QSerialPort>
#include <QByteArray>
#include <QDebug>

namespace WaterTest
{

    namespace
    {
        uint16_t modbusCrc16(const QByteArray &data)
        {
            uint16_t crc = 0xFFFF;
            for (unsigned char byte : data)
            {
                crc ^= byte;
                for (int i = 0; i < 8; ++i)
                {
                    const bool lsb = (crc & 0x0001) != 0;
                    crc >>= 1;
                    if (lsb)
                    {
                        crc ^= 0xA001;
                    }
                }
            }
            return crc;
        }

        std::string bytesToHex(const QByteArray &data)
        {
            std::ostringstream oss;
            oss << std::uppercase << std::hex << std::setfill('0');
            for (int i = 0; i < data.size(); ++i)
            {
                if (i > 0)
                {
                    oss << ' ';
                }
                oss << std::setw(2) << static_cast<int>(static_cast<uint8_t>(data[i]));
            }
            return oss.str();
        }

        std::string trimAscii(const std::string &value)
        {
            size_t begin = 0;
            size_t end = value.size();
            while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            {
                --end;
            }
            return value.substr(begin, end - begin);
        }

        std::string toUpperAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch)
                           { return static_cast<char>(std::toupper(ch)); });
            return value;
        }

        struct DcPowerRangeSelection
        {
            std::string command;
            float maxVoltage;
            float maxCurrent;
        };

        bool selectDcPowerRange(float voltageV,
                                float currentA,
                                DcPowerRangeSelection &selection)
        {
            if (voltageV > 25.0f)
            {
                if (currentA > 4.0f)
                {
                    return false;
                }

                selection.command = "VOLT:RANG P50V";
                selection.maxVoltage = 50.0f;
                selection.maxCurrent = 4.0f;
                return true;
            }

            selection.command = "VOLT:RANG P25V";
            selection.maxVoltage = 25.0f;
            selection.maxCurrent = 7.0f;
            return true;
        }

        void appendDcPowerDebugLog(const std::string &message)
        {
            std::error_code ec;
            std::filesystem::create_directories("deploy/logs", ec);
            std::ofstream logFile("deploy/logs/e3634a_rs232.log", std::ios::app);
            if (!logFile.is_open())
            {
                return;
            }

            const auto now = std::chrono::system_clock::now();
            const std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tmStruct{};
#ifdef _WIN32
            localtime_s(&tmStruct, &t);
#else
            localtime_r(&t, &tmStruct);
#endif
            logFile << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S")
                    << " " << message << "\n";
        }

        bool configureE3634ASerialPort(QSerialPort &port,
                                       std::string *errText = nullptr)
        {
            auto &cfg = ConfigManager::getInstance();
            const bool enabled = cfg.getBool("e3634a.enabled", false);
            if (!enabled)
            {
                if (errText)
                {
                    *errText = "disabled";
                }
                return false;
            }

            const std::string portName = cfg.getString("e3634a.rs232.port", "COM3");
            const int baud = cfg.getInt("e3634a.rs232.baud", 9600);
            const int dataBits = cfg.getInt("e3634a.rs232.data_bits", 8);
            const std::string parity = cfg.getString("e3634a.rs232.parity", "N");
            const int stopBits = cfg.getInt("e3634a.rs232.stop_bits", 1);

            port.setPortName(QString::fromStdString(portName));
            port.setBaudRate(baud);
            port.setDataBits(dataBits == 7 ? QSerialPort::Data7 : QSerialPort::Data8);

            if (parity == "E" || parity == "e")
            {
                port.setParity(QSerialPort::EvenParity);
            }
            else if (parity == "O" || parity == "o")
            {
                port.setParity(QSerialPort::OddParity);
            }
            else
            {
                port.setParity(QSerialPort::NoParity);
            }

            port.setStopBits(stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
            port.setFlowControl(QSerialPort::NoFlowControl);

            if (!port.open(QIODevice::ReadWrite))
            {
                if (errText)
                {
                    *errText = port.errorString().toStdString();
                }
                return false;
            }

            port.clear(QSerialPort::AllDirections);
            if (errText)
            {
                *errText = "ok";
            }
            return true;
        }

        bool writeScpiLine(QSerialPort &port,
                           const std::string &command,
                           int writeTimeoutMs,
                           std::string *errText = nullptr)
        {
            const QByteArray payload = QByteArray::fromStdString(command + "\n");
            const qint64 written = port.write(payload);
            if (written != payload.size())
            {
                if (errText)
                {
                    *errText = "write_failed";
                }
                return false;
            }

            if (!port.waitForBytesWritten(writeTimeoutMs))
            {
                if (errText)
                {
                    *errText = "write_timeout";
                }
                return false;
            }

            if (errText)
            {
                *errText = "ok";
            }
            return true;
        }

        bool queryScpiLine(QSerialPort &port,
                           const std::string &query,
                           std::string &response,
                           int writeTimeoutMs,
                           int readTimeoutMs,
                           std::string *errText = nullptr)
        {
            response.clear();
            std::string writeErr;
            if (!writeScpiLine(port, query, writeTimeoutMs, &writeErr))
            {
                if (errText)
                {
                    *errText = writeErr;
                }
                return false;
            }

            QByteArray rx;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(readTimeoutMs);
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (port.waitForReadyRead(80))
                {
                    rx += port.readAll();
                    if (rx.contains('\n') || rx.contains('\r'))
                    {
                        const std::string parsed = trimAscii(rx.toStdString());
                        if (!parsed.empty())
                        {
                            response = parsed;
                            if (errText)
                            {
                                *errText = "ok";
                            }
                            return true;
                        }

                        // Some instruments may emit extra CR/LF between replies.
                        // Keep waiting for the first non-empty line within timeout.
                        rx.clear();
                    }
                }
            }

            if (!rx.isEmpty())
            {
                const std::string parsed = trimAscii(rx.toStdString());
                if (!parsed.empty())
                {
                    response = parsed;
                    if (errText)
                    {
                        *errText = "ok";
                    }
                    return true;
                }
            }

            if (errText)
            {
                *errText = "read_timeout_or_empty";
            }
            return false;
        }

        bool queryScpiNoError(QSerialPort &port,
                              int writeTimeoutMs,
                              int readTimeoutMs,
                              std::string *errText = nullptr)
        {
            std::string errorText;
            std::string queryErr;
            if (!queryScpiLine(port, "SYST:ERR?", errorText, writeTimeoutMs, readTimeoutMs, &queryErr))
            {
                if (errText)
                {
                    *errText = "query_failed SYST:ERR?: " + queryErr;
                }
                return false;
            }

            errorText = trimAscii(errorText);
            if (!errorText.empty() && errorText.rfind("+0", 0) != 0)
            {
                if (errText)
                {
                    *errText = errorText;
                }
                return false;
            }

            if (errText)
            {
                *errText = errorText.empty() ? "+0,\"No error\"" : errorText;
            }
            return true;
        }

        bool modbusReadHoldingRegisters(QSerialPort &port,
                                        uint8_t unitId,
                                        uint16_t startAddr,
                                        uint16_t count,
                                        std::vector<uint16_t> &outValues,
                                        std::string *requestHex = nullptr,
                                        std::string *responseHex = nullptr,
                                        std::string *errorText = nullptr)
        {
            QByteArray frame;
            frame.append(static_cast<char>(unitId));
            frame.append(static_cast<char>(0x03));
            frame.append(static_cast<char>((startAddr >> 8) & 0xFF));
            frame.append(static_cast<char>(startAddr & 0xFF));
            frame.append(static_cast<char>((count >> 8) & 0xFF));
            frame.append(static_cast<char>(count & 0xFF));

            const uint16_t crc = modbusCrc16(frame);
            frame.append(static_cast<char>(crc & 0xFF));
            frame.append(static_cast<char>((crc >> 8) & 0xFF));
            if (requestHex)
            {
                *requestHex = bytesToHex(frame);
            }

            if (port.write(frame) != frame.size())
            {
                if (errorText)
                {
                    *errorText = "write_failed";
                }
                return false;
            }
            if (!port.waitForBytesWritten(500))
            {
                if (errorText)
                {
                    *errorText = "write_timeout";
                }
                return false;
            }

            const int expectedLength = 5 + static_cast<int>(count) * 2;
            QByteArray response;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
            while (response.size() < expectedLength && std::chrono::steady_clock::now() < deadline)
            {
                if (port.waitForReadyRead(100))
                {
                    response += port.readAll();
                }
            }

            if (response.size() < expectedLength)
            {
                if (responseHex)
                {
                    *responseHex = bytesToHex(response);
                }
                if (errorText)
                {
                    *errorText = "read_timeout";
                }
                return false;
            }

            if (responseHex)
            {
                *responseHex = bytesToHex(response);
            }

            const QByteArray dataForCrc = response.left(response.size() - 2);
            const uint16_t crcResp = static_cast<uint8_t>(response[response.size() - 2]) |
                                     (static_cast<uint8_t>(response[response.size() - 1]) << 8);
            const uint16_t crcCalc = modbusCrc16(dataForCrc);
            if (crcResp != crcCalc)
            {
                if (errorText)
                {
                    *errorText = "crc_mismatch";
                }
                return false;
            }

            if (static_cast<uint8_t>(response[1]) >= 0x80)
            {
                if (errorText)
                {
                    *errorText = "modbus_exception_fc";
                }
                return false;
            }

            const int byteCount = static_cast<uint8_t>(response[2]);
            if (byteCount != static_cast<int>(count) * 2)
            {
                if (errorText)
                {
                    *errorText = "byte_count_mismatch";
                }
                return false;
            }

            outValues.clear();
            outValues.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const uint16_t value = (static_cast<uint8_t>(response[3 + 2 * i]) << 8) |
                                       static_cast<uint8_t>(response[4 + 2 * i]);
                outValues.push_back(value);
            }

            if (errorText)
            {
                *errorText = "ok";
            }

            return true;
        }

        float modbusRegsToFloatBigEndianWords(uint16_t highWord, uint16_t lowWord)
        {
            const uint32_t raw = (static_cast<uint32_t>(highWord) << 16) | static_cast<uint32_t>(lowWord);
            float value = 0.0f;
            std::memcpy(&value, &raw, sizeof(float));
            return value;
        }

        struct HartPressureUnitInfo
        {
            const char *label;
            float toKPaFactor;
        };

        bool tryGetHartPressureUnitInfo(uint16_t unitCode, HartPressureUnitInfo &info)
        {
            switch (unitCode)
            {
            case 1:
                info = {"inH2O", 0.24908891f};
                return true;
            case 2:
                info = {"inHg", 3.386389f};
                return true;
            case 3:
                info = {"ftH2O", 2.9890668f};
                return true;
            case 4:
                info = {"mmH2O", 0.00980665f};
                return true;
            case 5:
                info = {"mmHg", 0.13332239f};
                return true;
            case 6:
                info = {"psi", 6.8947573f};
                return true;
            case 7:
                info = {"bar", 100.0f};
                return true;
            case 8:
                info = {"mbar", 0.1f};
                return true;
            case 9:
                info = {"g/cm2", 98.0665f};
                return true;
            case 10:
                info = {"kg/cm2", 98.0665f};
                return true;
            case 11:
                info = {"Pa", 0.001f};
                return true;
            case 12:
                info = {"kPa", 1.0f};
                return true;
            case 13:
                info = {"torr", 0.13332239f};
                return true;
            case 14:
                info = {"atm", 101.325f};
                return true;
            case 15:
                info = {"MPa", 1000.0f};
                return true;
            default:
                return false;
            }
        }

        std::string flowUnitLabelFromCode(uint16_t unitCode)
        {
            switch (unitCode)
            {
            case 0:
                return "L/h";
            case 1:
                return "L/min";
            case 2:
                return "L/s";
            case 3:
                return "m3/h";
            case 4:
                return "m3/min";
            case 5:
                return "m3/s";
            case 6:
                return "kg/h";
            case 7:
                return "kg/min";
            case 8:
                return "kg/s";
            case 9:
                return "t/h";
            case 10:
                return "t/min";
            case 11:
                return "t/s";
            default:
                return "unknown";
            }
        }

        float decodeFloatByWordOrder(const uint8_t bytes[4], const std::string &wordOrder)
        {
            // bytes[] 按 PLC 缓冲区原样读取（4 字节）
            // 常见字节序：
            // - ABCD: 标准大端
            // - CDAB: Modbus 设备常见“字交换”格式（本项目压力传感器文档）
            uint8_t ordered[4] = {bytes[0], bytes[1], bytes[2], bytes[3]};

            if (wordOrder == "CDAB" || wordOrder == "cdab")
            {
                ordered[0] = bytes[2];
                ordered[1] = bytes[3];
                ordered[2] = bytes[0];
                ordered[3] = bytes[1];
            }
            else if (wordOrder == "BADC" || wordOrder == "badc")
            {
                ordered[0] = bytes[1];
                ordered[1] = bytes[0];
                ordered[2] = bytes[3];
                ordered[3] = bytes[2];
            }
            else if (wordOrder == "DCBA" || wordOrder == "dcba")
            {
                ordered[0] = bytes[3];
                ordered[1] = bytes[2];
                ordered[2] = bytes[1];
                ordered[3] = bytes[0];
            }

            const uint32_t raw = (static_cast<uint32_t>(ordered[0]) << 24) |
                                 (static_cast<uint32_t>(ordered[1]) << 16) |
                                 (static_cast<uint32_t>(ordered[2]) << 8) |
                                 static_cast<uint32_t>(ordered[3]);

            float value = 0.0f;
            std::memcpy(&value, &raw, sizeof(float));
            return value;
        }

        std::mutex &pressureDebugLogMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::string pressureDebugTimestamp()
        {
            const auto now = std::chrono::system_clock::now();
            const auto time = std::chrono::system_clock::to_time_t(now);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return oss.str();
        }

        void appendPressureDebugLog(const std::string &message)
        {
            std::lock_guard<std::mutex> lock(pressureDebugLogMutex());
            std::filesystem::create_directories("deploy/logs");

            std::ofstream logFile("deploy/logs/pressure_device_debug.log", std::ios::app);
            if (!logFile.is_open())
            {
                return;
            }

            logFile << pressureDebugTimestamp() << ' ' << message << '\n';
        }

        void appendFlowModbusDebugLog(const std::string &message)
        {
            std::lock_guard<std::mutex> lock(pressureDebugLogMutex());
            std::filesystem::create_directories("deploy/logs");

            std::ofstream logFile("deploy/logs/flowmeter_modbus_debug.log", std::ios::app);
            if (!logFile.is_open())
            {
                return;
            }

            logFile << pressureDebugTimestamp() << ' ' << message << '\n';
        }
    }

    bool DeviceManager::hasPlcClient() const
    {
        return static_cast<bool>(m_plcClient);
    }

    bool DeviceManager::isPlcConnected() const
    {
        return m_plcClient && m_plcClient->isConnected();
    }

    bool DeviceManager::isDataCollecting() const
    {
        return m_running;
    }

    std::string DeviceManager::getPlcLastError() const
    {
        return m_plcClient ? m_plcClient->getLastError() : std::string();
    }

    void DeviceManager::setDcPowerLastError(const std::string &errorText)
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_dcPowerLastError = errorText;
    }

    std::string DeviceManager::getDcPowerLastError() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        return m_dcPowerLastError;
    }

    DeviceManager::DeviceManager()
        : m_running(false)
    {
        // 流量计：1个
        for (int i = 1; i <= 1; ++i)
        {
            FlowMeter meter;
            meter.id = i;
            meter.name = "FlowMeter" + std::to_string(i);
            m_flowMeters[i] = meter;
        }

        // 电动阀：11个
        for (int i = 1; i <= 11; ++i)
        {
            ElectricValve valve;
            valve.id = i;
            valve.name = "Valve" + std::to_string(i);
            m_valves[i] = valve;
        }

        // 变频泵：2个
        for (int i = 1; i <= 2; ++i)
        {
            FrequencyPump pump;
            pump.id = i;
            pump.name = "Pump" + std::to_string(i);
            m_pumps[i] = pump;
        }

        // 温度传感器：由 initialize() 根据配置重建

        // 电动调压阀：默认2个，并初始化 PID / AO / AI 默认地址
        // 接线图 AOX SRCU1TA:
        //   AO 命令  → 端子 10-11 (4-20mA)  : 默认 QW80(阀1) / QW82(阀2)
        //   AI 反馈  ← 端子 16-17 (4-20mA)  : 默认 IW96(阀1) / IW98(阀2)
        //   DI 开到位 ← 端子 12-13           : 默认 I1.0(阀1) / I1.2(阀2)
        //   DI 关到位 ← 端子 14-15           : 默认 I1.1(阀1) / I1.3(阀2)
        //   DI 报警   ← 端子 22-23           : 默认 I1.4(阀1) / I1.5(阀2)
        for (int i = 1; i <= 2; ++i)
        {
            RegulatingValve regValve;
            regValve.id = static_cast<uint16_t>(i);
            regValve.name = (i == 1) ? "调节阀1" : "调节阀2";
            m_regulatingValves[i] = regValve;

            m_valveAoMerkerByteOffset[i] = -1;
            m_valveAiMerkerByteOffset[i] = -1;
        }
    }

    DeviceManager::~DeviceManager()
    {
        stopDataCollection();
    }

    bool DeviceManager::initialize(std::shared_ptr<S7PLCClient> plcClient)
    {
        if (!plcClient)
        {
            return false;
        }

        m_plcClient = plcClient;

        // Rebuild sensor lists according to config
        {
            auto &cfg = ConfigManager::getInstance();
            int pcount = cfg.getInt("pressure.count", 1);
            if (pcount < 1)
                pcount = 1;
            int tcount = cfg.getInt("temp.count", 1);
            if (tcount < 0)
                tcount = 0;

            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_pressureSensors.clear();
            for (int i = 1; i <= pcount; ++i)
            {
                PressureSensor sensor;
                sensor.id = static_cast<uint16_t>(i);
                sensor.maxPressure = cfg.getFloat("pressure.max_limit", 1000.0f);
                sensor.minPressure = cfg.getFloat("pressure.min_limit", 0.0f);
                m_pressureSensors[i] = sensor;
            }

            m_tempSensors.clear();
            for (int i = 1; i <= tcount; ++i)
            {
                TemperatureSensor temp;
                temp.id = static_cast<uint16_t>(i);
                m_tempSensors[i] = temp;
            }

            // 从配置文件加载调节阀 AO/AI/DI 地址（支持现场灵活配置）
            // 数量由 valve.count 控制，默认2，范围[1, 8]
            int valveCount = cfg.getInt("valve.count", 2);
            if (valveCount < 1)
                valveCount = 1;
            else if (valveCount > 8)
                valveCount = 8;

            m_regulatingValves.clear();
            m_valveAoMerkerByteOffset.clear();
            m_valveAiMerkerByteOffset.clear();

            for (int i = 1; i <= valveCount; ++i)
            {
                const std::string pfx = "valve." + std::to_string(i) + ".";

                RegulatingValve regValve;
                regValve.id = static_cast<uint16_t>(i);
                regValve.name = "调节阀" + std::to_string(i);
                m_regulatingValves[i] = regValve;

                m_valveAoMerkerByteOffset[i] = cfg.getInt(pfx + "ao.merker_real.byte_offset", -1);
                m_valveAiMerkerByteOffset[i] = cfg.getInt(pfx + "ai.merker_real.byte_offset", -1);
            }
        }
        return true;
    }

    bool DeviceManager::startDataCollection(int intervalMs)
    {
        if (m_running)
        {
            return false;
        }

        const std::string flowSource = ConfigManager::getInstance().getString("flow.source", "plc");
        const bool flowViaModbus = (flowSource == "modbus" || flowSource == "MODBUS");

        if ((!m_plcClient || !m_plcClient->isConnected()) && !flowViaModbus)
        {
            std::cerr << "PLC not connected!" << std::endl;
            return false;
        }

        m_running = true;
        m_collectionThread = std::make_shared<std::thread>(
            &DeviceManager::collectionThreadFunc, this, intervalMs);

        return true;
    }

    void DeviceManager::stopDataCollection()
    {
        // Signal thread to stop
        m_running = false;

        // Give thread a moment to notice the flag and exit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Wait for thread to finish
        if (m_collectionThread && m_collectionThread->joinable())
        {
            m_collectionThread->join();
        }
    }

    bool DeviceManager::updateAllDevices()
    {
        bool success = true;

        success &= readPressureSensors();
        success &= readFlowMeters();
        success &= readValves();
        success &= readPumps();
        success &= readTemperatureSensors();
        success &= readRegulatingValves();
        success &= readSystemStatus();
        success &= readPlcSelfCheckStatus();

        checkAlarms();

        // 保存数据到数据库和CSV
        if (m_dataLoggingEnabled && m_dataLogger && m_dataLogger->isInitialized())
        {
            // 注意：getPressureSensor/getFlowMeter 内部各自加 m_dataMutex，
            // 不能在持有 m_dataMutex 的情况下再调用它们（同线程重入 → abort）。
            // 直接在各自的独立加锁块里取值即可。
            auto p1 = getPressureSensor(1);
            auto p2 = getPressureSensor(2);
            auto p3 = getPressureSensor(3);
            auto flow = getFlowMeter(1);

            m_dataLogger->recordSensorData(p1, p2, p3, flow);
        }

        if (m_dataUpdateCallback)
        {
            m_dataUpdateCallback();
        }

        return success;
    }

    bool DeviceManager::setPlcSelfCheckEnable(bool enabled)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int dbCmd = cfg.getInt("selfcheck.plc.db_cmd", 20);
        const int byteOffset = cfg.getInt("selfcheck.plc.cmd.enable.byte_offset", 0);
        const int bitOffset = cfg.getInt("selfcheck.plc.cmd.enable.bit_offset", 0);
        if (bitOffset < 0 || bitOffset > 7)
        {
            return false;
        }

        return m_plcClient->writeBool(dbCmd, byteOffset, bitOffset, enabled) == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::writePlcSelfCheckCmdBit(const std::string &cmdKeyPrefix, bool pulse)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int dbCmd = cfg.getInt("selfcheck.plc.db_cmd", 20);
        const int byteOffset = cfg.getInt(cmdKeyPrefix + ".byte_offset", 0);
        const int bitOffset = cfg.getInt(cmdKeyPrefix + ".bit_offset", 0);
        if (bitOffset < 0 || bitOffset > 7)
        {
            return false;
        }

        if (!pulse)
        {
            return m_plcClient->writeBool(dbCmd, byteOffset, bitOffset, true) == S7PLCClient::Result::SUCCESS;
        }

        if (m_plcClient->writeBool(dbCmd, byteOffset, bitOffset, true) != S7PLCClient::Result::SUCCESS)
        {
            return false;
        }

        const int pulseMs = cfg.getInt("selfcheck.plc.cmd.pulse_ms", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(20, pulseMs)));
        return m_plcClient->writeBool(dbCmd, byteOffset, bitOffset, false) == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::startPlcSelfCheck()
    {
        return writePlcSelfCheckCmdBit("selfcheck.plc.cmd.start", true);
    }

    bool DeviceManager::abortPlcSelfCheck()
    {
        return writePlcSelfCheckCmdBit("selfcheck.plc.cmd.abort", true);
    }

    bool DeviceManager::resetPlcSelfCheck()
    {
        return writePlcSelfCheckCmdBit("selfcheck.plc.cmd.reset", true);
    }

    PlcSelfCheckStatus DeviceManager::getPlcSelfCheckStatus() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        return m_plcSelfCheckStatus;
    }

    PressureSensor DeviceManager::getPressureSensor(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_pressureSensors.find(id);
        if (it != m_pressureSensors.end())
        {
            return it->second;
        }

        return PressureSensor();
    }

    std::vector<PressureSensor> DeviceManager::getAllPressureSensors() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<PressureSensor> sensors;
        for (const auto &pair : m_pressureSensors)
        {
            sensors.push_back(pair.second);
        }

        return sensors;
    }

    FlowMeter DeviceManager::getFlowMeter(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_flowMeters.find(id);
        if (it != m_flowMeters.end())
        {
            return it->second;
        }

        return FlowMeter();
    }

    std::vector<FlowMeter> DeviceManager::getAllFlowMeters() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<FlowMeter> meters;
        for (const auto &pair : m_flowMeters)
        {
            meters.push_back(pair.second);
        }

        return meters;
    }

    TemperatureSensor DeviceManager::getTemperatureSensor(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_tempSensors.find(id);
        if (it != m_tempSensors.end())
        {
            return it->second;
        }

        return TemperatureSensor();
    }

    std::vector<TemperatureSensor> DeviceManager::getAllTemperatureSensors() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<TemperatureSensor> sensors;
        for (const auto &pair : m_tempSensors)
        {
            sensors.push_back(pair.second);
        }

        return sensors;
    }

    bool DeviceManager::controlValve(uint16_t id, bool open)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        // Write valve control command to PLC
        // DB block 3, offset = (id-1) * 20 + 0 (control bit)
        int offset = (id - 1) * 20;
        auto result = m_plcClient->writeBool(DB_VALVES, offset, 0, open);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            if (m_valves.find(id) != m_valves.end())
            {
                m_valves[id].status = open ? ValveStatus::OPENING : ValveStatus::CLOSING;
            }
            return true;
        }

        return false;
    }

    bool DeviceManager::setValveOpening(uint16_t id, uint8_t degree)
    {
        if (!m_plcClient || !m_plcClient->isConnected() || degree > 100)
        {
            return false;
        }

        int offset = (id - 1) * 20 + 2; // Opening degree at offset 2
        auto result = m_plcClient->writeDB(DB_VALVES, offset, 1, &degree);

        return result == S7PLCClient::Result::SUCCESS;
    }

    ElectricValve DeviceManager::getValve(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_valves.find(id);
        if (it != m_valves.end())
        {
            return it->second;
        }

        return ElectricValve();
    }

    std::vector<ElectricValve> DeviceManager::getAllValves() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<ElectricValve> valves;
        for (const auto &pair : m_valves)
        {
            valves.push_back(pair.second);
        }

        return valves;
    }

    bool DeviceManager::controlPump(uint16_t id, bool start)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        int offset = (id - 1) * 30;
        auto result = m_plcClient->writeBool(DB_PUMPS, offset, 0, start);

        return result == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::setPumpFrequency(uint16_t id, float frequency)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        int offset = (id - 1) * 30 + 4; // Frequency at offset 4
        auto result = m_plcClient->writeReal(DB_PUMPS, offset, frequency);

        return result == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::setDcPowerOutput(bool on)
    {
        QSerialPort port;
        std::string err;
        if (!configureE3634ASerialPort(port, &err))
        {
            appendDcPowerDebugLog("[E3634A] open_failed err=\"" + err + "\"");
            setDcPowerLastError("open_failed: " + err);
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int writeTimeoutMs = cfg.getInt("e3634a.rs232.write_timeout_ms", 600);
        const int readTimeoutMs = cfg.getInt("e3634a.rs232.read_timeout_ms", 900);
        const std::string cmd = std::string("OUTP ") + (on ? "ON" : "OFF");

        std::string stepErr;
        if (!writeScpiLine(port, "SYST:REM", writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=SYST:REM err=" + stepErr);
            setDcPowerLastError("cmd_failed SYST:REM: " + stepErr);
            return false;
        }

        if (!writeScpiLine(port, cmd, writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=" + cmd + " err=" + stepErr);
            setDcPowerLastError("cmd_failed " + cmd + ": " + stepErr);
            return false;
        }
        if (!queryScpiNoError(port, writeTimeoutMs, readTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_rejected cmd=" + cmd + " err=" + stepErr);
            setDcPowerLastError("cmd_rejected " + cmd + ": " + stepErr);
            return false;
        }

        appendDcPowerDebugLog("[E3634A] output=" + std::string(on ? "ON" : "OFF"));
        setDcPowerLastError(std::string());
        return true;
    }

    bool DeviceManager::setDcPowerSetpoint(float voltageV, float currentA)
    {
        auto &cfg = ConfigManager::getInstance();
        const float maxVoltage = cfg.getFloat("e3634a.limit.max_voltage", 50.0f);
        const float maxCurrent = cfg.getFloat("e3634a.limit.max_current", 7.0f);
        DcPowerRangeSelection rangeSelection;
        if (voltageV < 0.0f || currentA < 0.0f || voltageV > maxVoltage || currentA > maxCurrent)
        {
            std::ostringstream oss;
            oss << "[E3634A] setpoint_out_of_range"
                << " voltage=" << voltageV
                << " current=" << currentA
                << " maxVoltage=" << maxVoltage
                << " maxCurrent=" << maxCurrent;
            appendDcPowerDebugLog(oss.str());
            setDcPowerLastError(oss.str());
            return false;
        }
        if (!selectDcPowerRange(voltageV, currentA, rangeSelection))
        {
            std::ostringstream oss;
            oss << "[E3634A] setpoint_out_of_range"
                << " voltage=" << voltageV
                << " current=" << currentA
                << " range=P50V"
                << " rangeMaxCurrent=4";
            appendDcPowerDebugLog(oss.str());
            setDcPowerLastError(oss.str());
            return false;
        }

        QSerialPort port;
        std::string err;
        if (!configureE3634ASerialPort(port, &err))
        {
            appendDcPowerDebugLog("[E3634A] open_failed err=\"" + err + "\"");
            setDcPowerLastError("open_failed: " + err);
            return false;
        }

        const int writeTimeoutMs = cfg.getInt("e3634a.rs232.write_timeout_ms", 600);
        const int readTimeoutMs = cfg.getInt("e3634a.rs232.read_timeout_ms", 900);

        std::ostringstream voltCmd;
        voltCmd << std::fixed << std::setprecision(3) << "VOLT " << voltageV;
        std::ostringstream currCmd;
        currCmd << std::fixed << std::setprecision(3) << "CURR " << currentA;

        std::string stepErr;
        if (!writeScpiLine(port, "SYST:REM", writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=SYST:REM err=" + stepErr);
            setDcPowerLastError("cmd_failed SYST:REM: " + stepErr);
            return false;
        }
        if (!writeScpiLine(port, rangeSelection.command, writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=" + rangeSelection.command + " err=" + stepErr);
            setDcPowerLastError("cmd_failed " + rangeSelection.command + ": " + stepErr);
            return false;
        }
        if (!queryScpiNoError(port, writeTimeoutMs, readTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_rejected cmd=" + rangeSelection.command + " err=" + stepErr);
            setDcPowerLastError("cmd_rejected " + rangeSelection.command + ": " + stepErr);
            return false;
        }
        if (!writeScpiLine(port, voltCmd.str(), writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=" + voltCmd.str() + " err=" + stepErr);
            setDcPowerLastError("cmd_failed " + voltCmd.str() + ": " + stepErr);
            return false;
        }
        if (!writeScpiLine(port, currCmd.str(), writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=" + currCmd.str() + " err=" + stepErr);
            setDcPowerLastError("cmd_failed " + currCmd.str() + ": " + stepErr);
            return false;
        }
        if (!queryScpiNoError(port, writeTimeoutMs, readTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_rejected cmd=" + voltCmd.str() + "; " + currCmd.str() + " err=" + stepErr);
            setDcPowerLastError("cmd_rejected setpoint: " + stepErr);
            return false;
        }

        std::ostringstream log;
        log << "[E3634A] setpoint_ok voltage=" << voltageV
            << " current=" << currentA
            << " range=" << rangeSelection.command;
        appendDcPowerDebugLog(log.str());
        setDcPowerLastError(std::string());
        return true;
    }

    bool DeviceManager::readDcPowerMeasurements(float &voltageV, float &currentA)
    {
        voltageV = 0.0f;
        currentA = 0.0f;

        QSerialPort port;
        std::string err;
        if (!configureE3634ASerialPort(port, &err))
        {
            appendDcPowerDebugLog("[E3634A] open_failed err=\"" + err + "\"");
            setDcPowerLastError("open_failed: " + err);
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int writeTimeoutMs = cfg.getInt("e3634a.rs232.write_timeout_ms", 600);
        const int readTimeoutMs = cfg.getInt("e3634a.rs232.read_timeout_ms", 900);

        std::string vResp;
        std::string iResp;
        std::string queryErr;

        std::string stepErr;
        if (!writeScpiLine(port, "SYST:REM", writeTimeoutMs, &stepErr))
        {
            appendDcPowerDebugLog("[E3634A] cmd_failed cmd=SYST:REM err=" + stepErr);
            setDcPowerLastError("cmd_failed SYST:REM: " + stepErr);
            return false;
        }

        if (!queryScpiLine(port, "MEAS:VOLT?", vResp, writeTimeoutMs, readTimeoutMs, &queryErr))
        {
            appendDcPowerDebugLog("[E3634A] query_failed cmd=MEAS:VOLT? err=" + queryErr);
            setDcPowerLastError("query_failed MEAS:VOLT?: " + queryErr);
            return false;
        }
        if (!queryScpiLine(port, "MEAS:CURR?", iResp, writeTimeoutMs, readTimeoutMs, &queryErr))
        {
            appendDcPowerDebugLog("[E3634A] query_failed cmd=MEAS:CURR? err=" + queryErr);
            setDcPowerLastError("query_failed MEAS:CURR?: " + queryErr);
            return false;
        }

        try
        {
            voltageV = std::stof(vResp);
            currentA = std::stof(iResp);
        }
        catch (...)
        {
            appendDcPowerDebugLog("[E3634A] parse_failed vResp=\"" + vResp + "\" iResp=\"" + iResp + "\"");
            setDcPowerLastError("parse_failed vResp=\"" + vResp + "\" iResp=\"" + iResp + "\"");
            return false;
        }

        std::ostringstream log;
        log << "[E3634A] meas_ok voltage=" << voltageV << " current=" << currentA;
        appendDcPowerDebugLog(log.str());
        setDcPowerLastError(std::string());
        return true;
    }

    bool DeviceManager::getDcPowerIdentity(std::string &idn)
    {
        idn.clear();

        QSerialPort port;
        std::string err;
        if (!configureE3634ASerialPort(port, &err))
        {
            appendDcPowerDebugLog("[E3634A] open_failed err=\"" + err + "\"");
            setDcPowerLastError("open_failed: " + err);
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int writeTimeoutMs = cfg.getInt("e3634a.rs232.write_timeout_ms", 600);
        const int readTimeoutMs = cfg.getInt("e3634a.rs232.read_timeout_ms", 900);

        std::string queryErr;
        if (!queryScpiLine(port, "*IDN?", idn, writeTimeoutMs, readTimeoutMs, &queryErr))
        {
            appendDcPowerDebugLog("[E3634A] query_failed cmd=*IDN? err=" + queryErr);
            setDcPowerLastError("query_failed *IDN?: " + queryErr);
            return false;
        }

        idn = trimAscii(idn);
        appendDcPowerDebugLog("[E3634A] idn=\"" + toUpperAscii(idn) + "\"");
        setDcPowerLastError(std::string());
        return true;
    }

    bool DeviceManager::readDcPowerErrorCode(std::string &errorText)
    {
        errorText.clear();

        QSerialPort port;
        std::string err;
        if (!configureE3634ASerialPort(port, &err))
        {
            setDcPowerLastError("open_failed: " + err);
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int writeTimeoutMs = cfg.getInt("e3634a.rs232.write_timeout_ms", 600);
        const int readTimeoutMs = cfg.getInt("e3634a.rs232.read_timeout_ms", 900);

        std::string stepErr;
        if (!writeScpiLine(port, "SYST:REM", writeTimeoutMs, &stepErr))
        {
            setDcPowerLastError("cmd_failed SYST:REM: " + stepErr);
            return false;
        }

        std::string queryErr;
        if (!queryScpiLine(port, "SYST:ERR?", errorText, writeTimeoutMs, readTimeoutMs, &queryErr))
        {
            setDcPowerLastError("query_failed SYST:ERR?: " + queryErr);
            return false;
        }

        errorText = trimAscii(errorText);
        if (!errorText.empty() && errorText.rfind("+0", 0) != 0)
        {
            setDcPowerLastError(errorText);
        }
        return true;
    }

    FrequencyPump DeviceManager::getPump(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_pumps.find(id);
        if (it != m_pumps.end())
        {
            return it->second;
        }

        return FrequencyPump();
    }

    std::vector<FrequencyPump> DeviceManager::getAllPumps() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<FrequencyPump> pumps;
        for (const auto &pair : m_pumps)
        {
            pumps.push_back(pair.second);
        }

        return pumps;
    }

    // ======== 电动调压阀相关 ========
    bool DeviceManager::setValveOpeningPercent(uint16_t id, float percent)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
            return false;

        const float clamped = std::max(0.0f, std::min(100.0f, percent));
        auto aoMkIt = m_valveAoMerkerByteOffset.find(id);
        if (aoMkIt == m_valveAoMerkerByteOffset.end() || aoMkIt->second < 0)
            return false;

        const S7PLCClient::Result res = m_plcClient->writeMerkerReal(aoMkIt->second, clamped);

        if (res == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_regulatingValves.find(id);
            if (it != m_regulatingValves.end())
                it->second.openingSetpoint = clamped;
            return true;
        }
        return false;
    }

    RegulatingValve DeviceManager::getRegulatingValve(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        auto it = m_regulatingValves.find(id);
        if (it != m_regulatingValves.end())
        {
            return it->second;
        }

        return RegulatingValve();
    }

    std::vector<RegulatingValve> DeviceManager::getAllRegulatingValves() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<RegulatingValve> valves;
        for (const auto &pair : m_regulatingValves)
        {
            valves.push_back(pair.second);
        }

        return valves;
    }

    bool DeviceManager::setRelay(uint8_t index, bool on)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            qWarning() << "[M100][DeviceManager] setRelay failed: plc not connected" << "index=" << index << "on=" << on;
            return false;
        }

        // 特殊映射：
        // - index=0~3 -> M100.0~M100.3
        // - index=5   -> M100.4（站1电磁阀5）
        if (index <= 3 || index == 5)
        {
            const int mBit = (index <= 3) ? static_cast<int>(index) : 4;
            auto res = m_plcClient->writeMerkerBool(100, mBit, on);
            if (res == S7PLCClient::Result::SUCCESS)
            {
                qInfo() << "[M100][DeviceManager] setRelay M100 path"
                        << "index=" << index
                        << "addr=" << QString("M100.%1").arg(mBit)
                        << "on=" << on
                        << "result=" << static_cast<int>(res);
            }
            else
            {
                qWarning() << "[M100][DeviceManager] setRelay M100 path failed"
                           << "index=" << index
                           << "addr=" << QString("M100.%1").arg(mBit)
                           << "on=" << on
                           << "result=" << static_cast<int>(res)
                           << "lastError=" << QString::fromStdString(m_plcClient->getLastError());
            }
            return res == S7PLCClient::Result::SUCCESS;
        }

        // 其余 index 支持 Q0.1-Q1.7（index 1-15）：byteOffset = index/8，bit = index%8
        if (index > 15)
        {
            return false;
        }

        const int byteOff = static_cast<int>(index) / 8;
        const int bit     = static_cast<int>(index) % 8;
        auto res = m_plcClient->writeOutputBool(byteOff, bit, on);
        if (res == S7PLCClient::Result::SUCCESS)
        {
            qInfo() << "[M100][DeviceManager] setRelay Q path"
                << "index=" << index
                << "addr=" << QString("Q%1.%2").arg(byteOff).arg(bit)
                << "on=" << on
                << "result=" << static_cast<int>(res);
        }
        else
        {
            qWarning() << "[M100][DeviceManager] setRelay Q path failed"
                   << "index=" << index
                   << "addr=" << QString("Q%1.%2").arg(byteOff).arg(bit)
                   << "on=" << on
                   << "result=" << static_cast<int>(res)
                   << "lastError=" << QString::fromStdString(m_plcClient->getLastError());
        }
        return res == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::getRelayState(uint8_t index, bool &on) const
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            qWarning() << "[M100][DeviceManager] getRelayState failed: plc not connected" << "index=" << index;
            return false;
        }

        // 特殊映射：
        // - index=0~3 -> M100.0~M100.3
        // - index=5   -> M100.4（站1电磁阀5）
        if (index <= 3 || index == 5)
        {
            const int mBit = (index <= 3) ? static_cast<int>(index) : 4;
            bool value = false;
            auto res = m_plcClient->readMerkerBool(100, mBit, value);
            if (res == S7PLCClient::Result::SUCCESS)
            {
                on = value;
                qInfo() << "[M100][DeviceManager] getRelayState M100 path"
                        << "index=" << index
                        << "addr=" << QString("M100.%1").arg(mBit)
                        << "on=" << on
                        << "result=" << static_cast<int>(res);
                return true;
            }
            qWarning() << "[M100][DeviceManager] getRelayState M100 path failed"
                       << "index=" << index
                       << "addr=" << QString("M100.%1").arg(mBit)
                       << "result=" << static_cast<int>(res)
                       << "lastError=" << QString::fromStdString(m_plcClient->getLastError());
            return false;
        }

        // 其余 index 支持 Q0.1-Q1.7（index 1-15）
        if (index > 15)
        {
            return false;
        }

        const int byteOff = static_cast<int>(index) / 8;
        const int bit     = static_cast<int>(index) % 8;
        bool value = false;
        auto res = m_plcClient->readOutputBool(byteOff, bit, value);
        if (res == S7PLCClient::Result::SUCCESS)
        {
            on = value;
            qInfo() << "[M100][DeviceManager] getRelayState Q path"
                    << "index=" << index
                    << "addr=" << QString("Q%1.%2").arg(byteOff).arg(bit)
                    << "on=" << on;
            return true;
        }
        qWarning() << "[M100][DeviceManager] getRelayState Q path failed"
                   << "index=" << index
                   << "addr=" << QString("Q%1.%2").arg(byteOff).arg(bit)
                   << "result=" << static_cast<int>(res)
                   << "lastError=" << QString::fromStdString(m_plcClient->getLastError());
        return false;
    }

    bool DeviceManager::readMerkerState(uint16_t byteOffset, uint8_t bit, bool &on) const
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }
        if (bit > 7)
        {
            return false;
        }

        bool value = false;
        const auto res = m_plcClient->readMerkerBool(static_cast<int>(byteOffset), static_cast<int>(bit), value);
        if (res == S7PLCClient::Result::SUCCESS)
        {
            on = value;
            return true;
        }
        return false;
    }

    bool DeviceManager::setSystemMode(SystemMode mode)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        uint16_t modeValue = static_cast<uint16_t>(mode);
        auto result = m_plcClient->writeInt16(DB_SYSTEM, 0, modeValue);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_systemStatus.mode = mode;
            return true;
        }

        return false;
    }

    SystemStatus DeviceManager::getSystemStatus() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        return m_systemStatus;
    }

    bool DeviceManager::emergencyStop()
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        // Best-effort safe state: stop pumps first, then de-energize key relays.
        const uint8_t safeRelays[] = {0, 1, 2, 3, 8, 9};
        bool safeStateOk = true;
        safeStateOk &= controlPump(1, false);
        safeStateOk &= controlPump(2, false);
        for (uint8_t idx : safeRelays)
        {
            safeStateOk &= setRelay(idx, false);
        }

        // Emergency stop signal
        auto result = m_plcClient->writeBool(DB_SYSTEM, 10, 0, true);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            setSystemMode(SystemMode::EMERGENCY);
            addAlarm(AlarmLevel::CRITICAL,
                     safeStateOk ? "Emergency Stop Triggered"
                                 : "Emergency Stop Triggered (safe state partial failure)",
                     "System");
            return true;
        }

        return false;
    }

    bool DeviceManager::switchTestLine(TestLine line)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        uint16_t lineValue = static_cast<uint16_t>(line);
        auto result = m_plcClient->writeInt16(DB_SYSTEM, 2, lineValue);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_systemStatus.activeLine = line;
            return true;
        }

        return false;
    }

    std::vector<AlarmInfo> DeviceManager::getActiveAlarms() const
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        std::vector<AlarmInfo> activeAlarms;
        for (const auto &alarm : m_alarms)
        {
            if (alarm.isActive)
            {
                activeAlarms.push_back(alarm);
            }
        }

        return activeAlarms;
    }

    bool DeviceManager::acknowledgeAlarm(uint32_t alarmId)
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &alarm : m_alarms)
        {
            if (alarm.id == alarmId)
            {
                alarm.isActive = false;
                return true;
            }
        }

        return false;
    }

    void DeviceManager::setAlarmCallback(AlarmCallback callback)
    {
        m_alarmCallback = callback;
    }

    void DeviceManager::setDataUpdateCallback(DataUpdateCallback callback)
    {
        m_dataUpdateCallback = callback;
    }

    void DeviceManager::collectionThreadFunc(int intervalMs)
    {
        if (intervalMs < 10)
        {
            intervalMs = 10;
        }

        while (m_running)
        {
            try
            {
                updateAllDevices();
            }
            catch (const std::exception &e)
            {
                std::cerr << "[DeviceManager] collectionThread exception: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "[DeviceManager] collectionThread unknown exception" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

    bool DeviceManager::readPressureSensors()
    {
        if (!m_plcClient)
        {
            appendPressureDebugLog("[DEVICE][PRESSURE] skipped: plc client not initialized");
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int dbSensor = cfg.getInt("db.sensor.kpa.db_number", cfg.getInt("db.sensor.number", -1));
        if (dbSensor < 0)
        {
            appendPressureDebugLog("[DEVICE][PRESSURE][KPA] skipped: missing db.sensor.kpa.db_number (or db.sensor.number)");
            return false;
        }

        const int baseOffset = cfg.getInt("db.sensor.kpa.base_offset", 0);
        const int elemSize = cfg.getInt("db.sensor.kpa.element_size", 4); // REAL=4B
        const int indexBase = cfg.getInt("db.sensor.kpa.index_base", 1);  // DB_Sensor.SensorKpa[1..8]
        const float scale = cfg.getFloat("db.pressure.scale", 1.0f);
        const std::string kpaWordOrder = cfg.getString("db.sensor.kpa.word_order", "ABCD");
        bool anyKpaReadSuccess = false;

        std::vector<uint16_t> sensorIds;
        std::unordered_map<uint16_t, int> defaultDisplayDecimals;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            sensorIds.reserve(m_pressureSensors.size());
            for (const auto &pair : m_pressureSensors)
            {
                sensorIds.push_back(pair.first);
                defaultDisplayDecimals[pair.first] = pair.second.displayDecimals;
            }
        }

        for (uint16_t id : sensorIds)
        {
            const std::string sensorPrefix = std::string("db.sensor.") + std::to_string(id) + ".";
            const int sensorIndex = cfg.getInt(sensorPrefix + "kpa.index", static_cast<int>(id));
            const float sensorScale = cfg.getFloat(sensorPrefix + "scale", scale);

            if (elemSize <= 0)
            {
                appendPressureDebugLog("[DEVICE][PRESSURE][KPA] skipped: invalid db.sensor.kpa.element_size");
                break;
            }

            const int byteOffset = baseOffset + (sensorIndex - indexBase) * elemSize;
            if (byteOffset < 0)
            {
                std::ostringstream oss;
                oss << "[DEVICE][PRESSURE][KPA] sensor=" << id
                    << " sensorIndex=" << sensorIndex
                    << " byteOffset=" << byteOffset
                    << " result=invalid_offset";
                appendPressureDebugLog(oss.str());
                continue;
            }

            uint8_t rawBytes[4] = {0, 0, 0, 0};
            auto rr = m_plcClient->readDB(dbSensor, byteOffset, 4, rawBytes);
            if (rr != S7PLCClient::Result::SUCCESS)
            {
                std::ostringstream oss;
                oss << "[DEVICE][PRESSURE][KPA] sensor=" << id
                    << " db=" << dbSensor
                    << " offset=" << byteOffset
                    << " result=read_failed"
                    << " error=\"" << m_plcClient->getLastError() << "\"";
                appendPressureDebugLog(oss.str());
                continue;
            }

            float pressureKPa = decodeFloatByWordOrder(rawBytes, kpaWordOrder) * sensorScale;
            if (!std::isfinite(pressureKPa))
            {
                std::ostringstream oss;
                oss << "[DEVICE][PRESSURE][KPA] sensor=" << id
                    << " db=" << dbSensor
                    << " offset=" << byteOffset
                    << " result=non_finite";
                appendPressureDebugLog(oss.str());
                continue;
            }

            int displayDecimals = 2;
            auto decIt = defaultDisplayDecimals.find(id);
            if (decIt != defaultDisplayDecimals.end())
            {
                displayDecimals = decIt->second;
            }
            {
                std::lock_guard<std::mutex> writeLock(m_dataMutex);
                auto sensorIt = m_pressureSensors.find(id);
                if (sensorIt != m_pressureSensors.end())
                {
                    sensorIt->second.pressure = pressureKPa;
                    sensorIt->second.displayDecimals = displayDecimals;
                    sensorIt->second.status = DeviceStatus::ONLINE;
                    sensorIt->second.timestamp = std::chrono::system_clock::now();
                }
            }
            anyKpaReadSuccess = true;

            std::ostringstream oss;
            oss << "[DEVICE][PRESSURE][KPA] sensor=" << id
                << " db=" << dbSensor
                << " sensorIndex=" << sensorIndex
                << " indexBase=" << indexBase
                << " baseOffset=" << baseOffset
                << " elemSize=" << elemSize
                << " readOffset=" << byteOffset
                << " wordOrder=" << kpaWordOrder
                << " rawBytes=0x"
                << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rawBytes[0])
                << std::setw(2) << static_cast<int>(rawBytes[1])
                << std::setw(2) << static_cast<int>(rawBytes[2])
                << std::setw(2) << static_cast<int>(rawBytes[3])
                << std::dec
                << " scale=" << sensorScale
                << " pressureKPa=" << pressureKPa
                << " status=" << static_cast<int>(DeviceStatus::ONLINE);
            appendPressureDebugLog(oss.str());
        }

        if (!anyKpaReadSuccess)
        {
            appendPressureDebugLog("[DEVICE][PRESSURE][KPA] all sensors read failed");
        }

        return anyKpaReadSuccess;
    }

    bool DeviceManager::readFlowMeters()
    {
        auto &cfg = ConfigManager::getInstance();
        const std::string flowSource = cfg.getString("flow.source", "plc");
        const bool flowViaModbus = (flowSource == "modbus" || flowSource == "MODBUS");
        const bool flowViaPlcBuffer = (flowSource == "plc_buffer" || flowSource == "PLC_BUFFER");

        if (flowViaModbus)
        {
            QSerialPort port;
            const std::string portName = cfg.getString("flow.modbus.port", "COM8");
            const int baud = cfg.getInt("flow.modbus.baud", 9600);
            port.setPortName(QString::fromStdString(portName));
            port.setBaudRate(baud);

            const int dataBits = cfg.getInt("flow.modbus.data_bits", 8);
            port.setDataBits(dataBits == 7 ? QSerialPort::Data7 : QSerialPort::Data8);

            const std::string parity = cfg.getString("flow.modbus.parity", "N");
            if (parity == "E" || parity == "e")
            {
                port.setParity(QSerialPort::EvenParity);
            }
            else if (parity == "O" || parity == "o")
            {
                port.setParity(QSerialPort::OddParity);
            }
            else
            {
                port.setParity(QSerialPort::NoParity);
            }

            const int stopBits = cfg.getInt("flow.modbus.stop_bits", 1);
            port.setStopBits(stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
            port.setFlowControl(QSerialPort::NoFlowControl);

            if (!port.open(QIODevice::ReadWrite))
            {
                std::ostringstream oss;
                oss << "[FLOW][MODBUS] open_failed"
                    << " port=" << portName
                    << " baud=" << baud
                    << " err=\"" << port.errorString().toStdString() << "\"";
                appendFlowModbusDebugLog(oss.str());
                return false;
            }

            const uint8_t slaveId = static_cast<uint8_t>(cfg.getInt("flow.modbus.slave_id", 1));
            const uint16_t regTotal = static_cast<uint16_t>(cfg.getInt("flow.modbus.reg_total", 90));
            const uint16_t regRate = static_cast<uint16_t>(cfg.getInt("flow.modbus.reg_rate", 98));
            const uint16_t regUnit = static_cast<uint16_t>(cfg.getInt("flow.modbus.reg_unit", 105));

            std::vector<uint16_t> totalRegs;
            std::vector<uint16_t> rateRegs;
            std::vector<uint16_t> metaRegs;
            std::string reqTotal, respTotal, errTotal;
            std::string reqRate, respRate, errRate;
            std::string reqMeta, respMeta, errMeta;
            const bool okTotal = modbusReadHoldingRegisters(port, slaveId, regTotal, 2, totalRegs, &reqTotal, &respTotal, &errTotal);
            const bool okRate = modbusReadHoldingRegisters(port, slaveId, regRate, 2, rateRegs, &reqRate, &respRate, &errRate);
            const bool okMeta = modbusReadHoldingRegisters(port, slaveId, regUnit, 3, metaRegs, &reqMeta, &respMeta, &errMeta);

            {
                std::ostringstream oss;
                oss << "[FLOW][MODBUS] read_total"
                    << " port=" << portName
                    << " unit=" << static_cast<int>(slaveId)
                    << " reg=" << regTotal
                    << " ok=" << (okTotal ? 1 : 0)
                    << " err=" << errTotal
                    << " req=[" << reqTotal << "]"
                    << " resp=[" << respTotal << "]";
                appendFlowModbusDebugLog(oss.str());
            }

            {
                std::ostringstream oss;
                oss << "[FLOW][MODBUS] read_rate"
                    << " port=" << portName
                    << " unit=" << static_cast<int>(slaveId)
                    << " reg=" << regRate
                    << " ok=" << (okRate ? 1 : 0)
                    << " err=" << errRate
                    << " req=[" << reqRate << "]"
                    << " resp=[" << respRate << "]";
                appendFlowModbusDebugLog(oss.str());
            }

            {
                std::ostringstream oss;
                oss << "[FLOW][MODBUS] read_meta"
                    << " port=" << portName
                    << " unit=" << static_cast<int>(slaveId)
                    << " reg=" << regUnit
                    << " ok=" << (okMeta ? 1 : 0)
                    << " err=" << errMeta
                    << " req=[" << reqMeta << "]"
                    << " resp=[" << respMeta << "]";
                appendFlowModbusDebugLog(oss.str());
            }

            if (!okTotal || !okRate || totalRegs.size() < 2 || rateRegs.size() < 2)
            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                auto it = m_flowMeters.find(1);
                if (it != m_flowMeters.end())
                {
                    it->second.status = DeviceStatus::OFFLINE;
                    it->second.timestamp = std::chrono::system_clock::now();
                }
                return false;
            }

            const float totalFlow = modbusRegsToFloatBigEndianWords(totalRegs[0], totalRegs[1]);
            const float flowRate = modbusRegsToFloatBigEndianWords(rateRegs[0], rateRegs[1]);
            const uint16_t unitCode = (okMeta && metaRegs.size() >= 1) ? metaRegs[0] : 0;
            const uint16_t emptyPipeAlarm = (okMeta && metaRegs.size() >= 2) ? metaRegs[1] : 0;
            const uint16_t excitationAlarm = (okMeta && metaRegs.size() >= 3) ? metaRegs[2] : 0;
            const std::string unitLabel = flowUnitLabelFromCode(unitCode);

            {
                std::ostringstream oss;
                oss << "[FLOW][MODBUS] parsed"
                    << " total_regs=[0x" << std::hex << std::uppercase << totalRegs[0]
                    << ",0x" << totalRegs[1] << "]"
                    << " rate_regs=[0x" << rateRegs[0] << ",0x" << rateRegs[1] << "]"
                    << std::dec
                    << " total=" << totalFlow
                    << " rate=" << flowRate
                    << " unitCode=" << unitCode
                    << " unitLabel=" << unitLabel
                    << " emptyPipeAlarm=" << emptyPipeAlarm
                    << " excitationAlarm=" << excitationAlarm;
                appendFlowModbusDebugLog(oss.str());
            }

            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_flowMeters.find(1);
            if (it != m_flowMeters.end())
            {
                it->second.status = DeviceStatus::ONLINE;
                it->second.flowRate = flowRate;
                it->second.totalFlow = totalFlow;
                it->second.unitCode = unitCode;
                it->second.unitLabel = unitLabel;
                it->second.emptyPipeAlarm = emptyPipeAlarm;
                it->second.excitationAlarm = excitationAlarm;
                it->second.timestamp = std::chrono::system_clock::now();
            }

            return true;
        }

        if (flowViaPlcBuffer)
        {
            if (!m_plcClient)
            {
                return false;
            }

            const int dbNumber = cfg.getInt("flow.plc_buffer.db_number", 1);
            const int wordOffset = cfg.getInt("flow.plc_buffer.word_offset", -1);
            const int byteOffset = wordOffset >= 0
                                       ? wordOffset * 2
                                       : cfg.getInt("flow.plc_buffer.byte_offset", 119);
            const int wordCount = cfg.getInt("flow.plc_buffer.word_count", 18);
            const int totalIndex = cfg.getInt("flow.plc_buffer.reg_total_index", 0);
            const int rateIndex = cfg.getInt("flow.plc_buffer.reg_rate_index", 8);
            const int unitIndex = cfg.getInt("flow.plc_buffer.reg_unit_index", 15);
            const int emptyPipeAlarmIndex = cfg.getInt("flow.plc_buffer.reg_empty_pipe_alarm_index", 16);
            const int excitationAlarmIndex = cfg.getInt("flow.plc_buffer.reg_excitation_alarm_index", 17);
            const std::string probeWordOffsets = cfg.getString("flow.plc_buffer.probe_word_offsets", "");

            if (wordCount < 10 || totalIndex < 0 || rateIndex < 0 ||
                totalIndex + 1 >= wordCount || rateIndex + 1 >= wordCount ||
                unitIndex < 0 || unitIndex >= wordCount ||
                emptyPipeAlarmIndex < 0 || emptyPipeAlarmIndex >= wordCount ||
                excitationAlarmIndex < 0 || excitationAlarmIndex >= wordCount)
            {
                return false;
            }

            auto readRegsAtByteOffset = [&](int blockByteOffset, std::vector<uint16_t> &outRegs) -> bool
            {
                std::vector<uint8_t> raw(static_cast<size_t>(wordCount) * 2, 0);
                auto rr = m_plcClient->readDB(dbNumber, blockByteOffset, static_cast<int>(raw.size()), raw.data());
                if (rr != S7PLCClient::Result::SUCCESS)
                {
                    return false;
                }

                outRegs.assign(static_cast<size_t>(wordCount), 0);
                for (int i = 0; i < wordCount; ++i)
                {
                    outRegs[static_cast<size_t>(i)] =
                        (static_cast<uint16_t>(raw[static_cast<size_t>(i) * 2]) << 8) |
                        static_cast<uint16_t>(raw[static_cast<size_t>(i) * 2 + 1]);
                }
                return true;
            };

            std::vector<uint16_t> regs;
            if (!readRegsAtByteOffset(byteOffset, regs))
            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                auto it = m_flowMeters.find(1);
                if (it != m_flowMeters.end())
                {
                    it->second.status = DeviceStatus::OFFLINE;
                    it->second.timestamp = std::chrono::system_clock::now();
                }
                return false;
            }

            int effectiveByteOffset = byteOffset;
            const bool allZeroAtPrimary = std::all_of(regs.begin(), regs.end(), [](uint16_t v)
                                                      { return v == 0; });
            if (allZeroAtPrimary && !probeWordOffsets.empty())
            {
                std::istringstream iss(probeWordOffsets);
                std::string token;
                while (std::getline(iss, token, ','))
                {
                    try
                    {
                        const int candidateWordOffset = std::stoi(token);
                        if (candidateWordOffset < 0)
                        {
                            continue;
                        }
                        const int candidateByteOffset = candidateWordOffset * 2;
                        if (candidateByteOffset == byteOffset)
                        {
                            continue;
                        }

                        std::vector<uint16_t> candidateRegs;
                        if (!readRegsAtByteOffset(candidateByteOffset, candidateRegs))
                        {
                            continue;
                        }

                        const bool candidateAllZero = std::all_of(candidateRegs.begin(), candidateRegs.end(), [](uint16_t v)
                                                                  { return v == 0; });
                        if (!candidateAllZero)
                        {
                            effectiveByteOffset = candidateByteOffset;
                            regs.swap(candidateRegs);

                            std::ostringstream probeOss;
                            probeOss << "[FLOW][PLC_BUFFER] probe_selected"
                                     << " db=" << dbNumber
                                     << " primaryWordOffset=" << (byteOffset / 2)
                                     << " selectedWordOffset=" << candidateWordOffset;
                            appendFlowModbusDebugLog(probeOss.str());
                            break;
                        }
                    }
                    catch (...)
                    {
                        continue;
                    }
                }
            }

            const float totalFlow = modbusRegsToFloatBigEndianWords(regs[static_cast<size_t>(totalIndex)],
                                                                     regs[static_cast<size_t>(totalIndex + 1)]);
            const float flowRate = modbusRegsToFloatBigEndianWords(regs[static_cast<size_t>(rateIndex)],
                                                                    regs[static_cast<size_t>(rateIndex + 1)]);
            const uint16_t unitCode = regs[static_cast<size_t>(unitIndex)];
            const uint16_t emptyPipeAlarm = regs[static_cast<size_t>(emptyPipeAlarmIndex)];
            const uint16_t excitationAlarm = regs[static_cast<size_t>(excitationAlarmIndex)];
            const std::string unitLabel = flowUnitLabelFromCode(unitCode);

            {
                std::ostringstream oss;
                oss << "[FLOW][PLC_BUFFER] parsed"
                    << " db=" << dbNumber
                    << " wordOffset=" << (effectiveByteOffset / 2)
                    << " byteOffset=" << effectiveByteOffset
                    << " wordCount=" << wordCount
                    << " totalIndex=" << totalIndex
                    << " rateIndex=" << rateIndex
                    << " unitIndex=" << unitIndex
                    << " emptyPipeAlarmIndex=" << emptyPipeAlarmIndex
                    << " excitationAlarmIndex=" << excitationAlarmIndex
                    << " regs=[";
                for (int i = 0; i < wordCount; ++i)
                {
                    if (i > 0)
                    {
                        oss << ',';
                    }
                    oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << regs[static_cast<size_t>(i)] << std::dec;
                }
                oss << "]"
                    << " total=" << totalFlow
                    << " rate=" << flowRate
                    << " unitCode=" << unitCode
                    << " unitLabel=" << unitLabel
                    << " emptyPipeAlarm=" << emptyPipeAlarm
                    << " excitationAlarm=" << excitationAlarm;
                appendFlowModbusDebugLog(oss.str());
            }

            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_flowMeters.find(1);
            if (it != m_flowMeters.end())
            {
                it->second.status = DeviceStatus::ONLINE;
                it->second.flowRate = flowRate;
                it->second.totalFlow = totalFlow;
                it->second.unitCode = unitCode;
                it->second.unitLabel = unitLabel;
                it->second.emptyPipeAlarm = emptyPipeAlarm;
                it->second.excitationAlarm = excitationAlarm;
                it->second.timestamp = std::chrono::system_clock::now();
            }

            return true;
        }

        if (!m_plcClient)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_flowMeters)
        {
            uint16_t id = pair.first;
            FlowMeter &meter = pair.second;

            // DB block 2, each flow meter occupies 16 bytes
            int offset = (id - 1) * 16;

            int16_t status;
            float flowRate, totalFlow, temperature;

            if (m_plcClient->readInt16(DB_FLOW_METERS, offset, status) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_FLOW_METERS, offset + 2, flowRate) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_FLOW_METERS, offset + 6, totalFlow) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_FLOW_METERS, offset + 10, temperature) == S7PLCClient::Result::SUCCESS)
            {

                meter.status = static_cast<DeviceStatus>(status);
                meter.flowRate = flowRate;
                meter.totalFlow = totalFlow;
                meter.temperature = temperature;
                meter.timestamp = std::chrono::system_clock::now();
            }
        }

        return true;
    }

    bool DeviceManager::readValves()
    {
        if (!m_plcClient)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_valves)
        {
            uint16_t id = pair.first;
            ElectricValve &valve = pair.second;

            // DB block 3, each valve occupies 20 bytes
            int offset = (id - 1) * 20;

            int16_t status, valveStatus;
            uint8_t openingDegree;

            if (m_plcClient->readInt16(DB_VALVES, offset, valveStatus) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readDB(DB_VALVES, offset + 2, 1, &openingDegree) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_VALVES, offset + 4, status) == S7PLCClient::Result::SUCCESS)
            {

                valve.status = static_cast<ValveStatus>(valveStatus);
                valve.openingDegree = openingDegree;
                valve.deviceStatus = static_cast<DeviceStatus>(status);
                valve.timestamp = std::chrono::system_clock::now();
            }
        }

        return true;
    }

    bool DeviceManager::readPumps()
    {
        if (!m_plcClient)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_pumps)
        {
            uint16_t id = pair.first;
            FrequencyPump &pump = pair.second;

            // DB block 4, each pump occupies 30 bytes
            int offset = (id - 1) * 30;

            bool running;
            int16_t status;
            float frequency, current, power, speed;

            if (m_plcClient->readBool(DB_PUMPS, offset, 0, running) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_PUMPS, offset + 2, status) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_PUMPS, offset + 4, frequency) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_PUMPS, offset + 8, current) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_PUMPS, offset + 12, power) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_PUMPS, offset + 16, speed) == S7PLCClient::Result::SUCCESS)
            {

                pump.isRunning = running;
                pump.status = static_cast<DeviceStatus>(status);
                pump.frequency = frequency;
                pump.current = current;
                pump.power = power;
                pump.speed = speed;
                pump.timestamp = std::chrono::system_clock::now();
            }
        }

        return true;
    }

    bool DeviceManager::readTemperatureSensors()
    {
        if (!m_plcClient)
        {
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int dbSensor = cfg.getInt("db.sensor.number", -1);

        // 优先使用 DB_Sensor.Temp_Celsius 数组
        if (dbSensor >= 0)
        {
            const int baseOffset = cfg.getInt("db.sensor.temp_c.base_offset", 32); // 默认推断：Temp_Celsius 起始在32
            const int elemSize = cfg.getInt("db.sensor.temp_c.element_size", 4);   // REAL=4B

            std::lock_guard<std::mutex> lock(m_dataMutex);
            for (auto &pair : m_tempSensors)
            {
                uint16_t id = pair.first;
                TemperatureSensor &sensor = pair.second;
                const int offset = baseOffset + (static_cast<int>(id) - 1) * elemSize;
                float temp = 0.0f;
                auto r = m_plcClient->readReal(dbSensor, offset, temp);
                if (r == S7PLCClient::Result::SUCCESS)
                {
                    sensor.temperature = temp;
                    sensor.status = DeviceStatus::ONLINE;
                    sensor.timestamp = std::chrono::system_clock::now();
                }
            }
            return true;
        }

        // 回退到旧的 db.temp.* / db.temp1.*
        {
            // 支持新键 db.temp.*，并兼容旧键 db.temp1.*
            int globalDb = cfg.getInt("db.temp.number", cfg.getInt("db.temp1.number", -1));
            int globalItemSize = cfg.getInt("db.temp.item_size", cfg.getInt("db.temp1.item_size", 8));
            int globalValueOffset = cfg.getInt("db.temp.value_offset", cfg.getInt("db.temp1.value_offset", 0));

            if (globalDb < 0)
            {
                return true;
            }

            std::lock_guard<std::mutex> lock(m_dataMutex);

            for (auto &pair : m_tempSensors)
            {
                uint16_t id = pair.first;
                TemperatureSensor &sensor = pair.second;

                const std::string prefix = std::string("db.temp.") + std::to_string(id) + ".";
                int dbNumber = cfg.getInt(prefix + "number", globalDb);
                int itemSize = cfg.getInt(prefix + "item_size", globalItemSize);
                int valueOffset = cfg.getInt(prefix + "value_offset", globalValueOffset);

                int offset = 0;
                if (itemSize > 0)
                {
                    offset = (static_cast<int>(id) - 1) * itemSize + valueOffset;
                }
                else
                {
                    offset = valueOffset;
                }

                float t = 0.0f;
                auto res = m_plcClient->readReal(dbNumber, offset, t);
                if (res == S7PLCClient::Result::SUCCESS)
                {
                    sensor.temperature = t;
                    sensor.status = DeviceStatus::ONLINE;
                    sensor.timestamp = std::chrono::system_clock::now();
                }
            }

            return true;
        }
    }

    bool DeviceManager::readRegulatingValves()
    {
        if (!m_plcClient)
            return false;

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_regulatingValves)
        {
            const uint16_t id = pair.first;
            RegulatingValve &valve = pair.second;

            // 最小化路径：仅保留 MD 开度反馈。
            auto aiMkIt = m_valveAiMerkerByteOffset.find(id);
            if (aiMkIt != m_valveAiMerkerByteOffset.end() && aiMkIt->second >= 0)
            {
                float openingPercent = 0.0f;
                if (m_plcClient->readMerkerReal(aiMkIt->second, openingPercent) == S7PLCClient::Result::SUCCESS)
                {
                    valve.openingPercent = std::max(0.0f, std::min(100.0f, openingPercent));
                    valve.deviceStatus = DeviceStatus::ONLINE;
                }
            }
            else
            {
                valve.deviceStatus = DeviceStatus::OFFLINE;
            }

            if (valve.openingPercent > 1.0f)
                valve.status = ValveStatus::OPENING;
            else
                valve.status = ValveStatus::CLOSED;

            valve.timestamp = std::chrono::system_clock::now();
        }

        return true;
    }

    bool DeviceManager::readSystemStatus()
    {
        if (!m_plcClient)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        int16_t mode, line;
        bool running;

        if (m_plcClient->readInt16(DB_SYSTEM, 0, mode) == S7PLCClient::Result::SUCCESS &&
            m_plcClient->readInt16(DB_SYSTEM, 2, line) == S7PLCClient::Result::SUCCESS &&
            m_plcClient->readBool(DB_SYSTEM, 4, 0, running) == S7PLCClient::Result::SUCCESS)
        {

            m_systemStatus.mode = static_cast<SystemMode>(mode);
            m_systemStatus.activeLine = static_cast<TestLine>(line);
            m_systemStatus.isRunning = running;
        }

        return true;
    }

    bool DeviceManager::readPlcSelfCheckStatus()
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_plcSelfCheckStatus.online = false;
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        if (!cfg.getBool("selfcheck.plc.status.enable_readback", true))
        {
            return true;
        }

        const int dbStatus = cfg.getInt("selfcheck.plc.db_status", 21);

        const int bBusyByte = cfg.getInt("selfcheck.plc.status.busy.byte_offset", 0);
        const int bBusyBit = cfg.getInt("selfcheck.plc.status.busy.bit_offset", 0);
        const int bDoneByte = cfg.getInt("selfcheck.plc.status.done.byte_offset", 0);
        const int bDoneBit = cfg.getInt("selfcheck.plc.status.done.bit_offset", 1);
        const int bPassedByte = cfg.getInt("selfcheck.plc.status.passed.byte_offset", 0);
        const int bPassedBit = cfg.getInt("selfcheck.plc.status.passed.bit_offset", 2);
        const int bFailedByte = cfg.getInt("selfcheck.plc.status.failed.byte_offset", 0);
        const int bFailedBit = cfg.getInt("selfcheck.plc.status.failed.bit_offset", 3);

        const int stepOffset = cfg.getInt("selfcheck.plc.status.step_offset", 2);
        const int faultOffset = cfg.getInt("selfcheck.plc.status.fault_offset", 4);

        const int ps4BeforeOffset = cfg.getInt("selfcheck.plc.status.ps4_before_offset", 6);
        const int ps4AfterOffset = cfg.getInt("selfcheck.plc.status.ps4_after_offset", 10);
        const int ps4BuildOffset = cfg.getInt("selfcheck.plc.status.ps4_build_offset", 14);
        const int ps4HoldOffset = cfg.getInt("selfcheck.plc.status.ps4_hold_offset", 18);
        const int ps5BuildOffset = cfg.getInt("selfcheck.plc.status.ps5_build_offset", 22);
        const int ps5HoldOffset = cfg.getInt("selfcheck.plc.status.ps5_hold_offset", 26);
        const int ps4DeltaOffset = cfg.getInt("selfcheck.plc.status.ps4_delta_offset", 30);
        const int ps5DeltaOffset = cfg.getInt("selfcheck.plc.status.ps5_delta_offset", 34);

        bool busy = false;
        bool done = false;
        bool passed = false;
        bool failed = false;
        int16_t step = 0;
        int16_t fault = 0;
        float ps4Before = 0.0f;
        float ps4After = 0.0f;
        float ps4Build = 0.0f;
        float ps4Hold = 0.0f;
        float ps5Build = 0.0f;
        float ps5Hold = 0.0f;
        float ps4Delta = 0.0f;
        float ps5Delta = 0.0f;

        bool ok = true;
        ok &= (m_plcClient->readBool(dbStatus, bBusyByte, bBusyBit, busy) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readBool(dbStatus, bDoneByte, bDoneBit, done) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readBool(dbStatus, bPassedByte, bPassedBit, passed) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readBool(dbStatus, bFailedByte, bFailedBit, failed) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readInt16(dbStatus, stepOffset, step) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readInt16(dbStatus, faultOffset, fault) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps4BeforeOffset, ps4Before) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps4AfterOffset, ps4After) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps4BuildOffset, ps4Build) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps4HoldOffset, ps4Hold) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps5BuildOffset, ps5Build) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps5HoldOffset, ps5Hold) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps4DeltaOffset, ps4Delta) == S7PLCClient::Result::SUCCESS);
        ok &= (m_plcClient->readReal(dbStatus, ps5DeltaOffset, ps5Delta) == S7PLCClient::Result::SUCCESS);

        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_plcSelfCheckStatus.online = ok;
        if (ok)
        {
            m_plcSelfCheckStatus.busy = busy;
            m_plcSelfCheckStatus.done = done;
            m_plcSelfCheckStatus.passed = passed;
            m_plcSelfCheckStatus.failed = failed;
            m_plcSelfCheckStatus.stepNo = step;
            m_plcSelfCheckStatus.faultCode = static_cast<uint16_t>(fault);
            m_plcSelfCheckStatus.ps4Before = ps4Before;
            m_plcSelfCheckStatus.ps4After = ps4After;
            m_plcSelfCheckStatus.ps4Build = ps4Build;
            m_plcSelfCheckStatus.ps4Hold = ps4Hold;
            m_plcSelfCheckStatus.ps5Build = ps5Build;
            m_plcSelfCheckStatus.ps5Hold = ps5Hold;
            m_plcSelfCheckStatus.ps4Delta = ps4Delta;
            m_plcSelfCheckStatus.ps5Delta = ps5Delta;
            m_plcSelfCheckStatus.timestamp = std::chrono::system_clock::now();
        }

        return ok;
    }

    void DeviceManager::checkAlarms()
    {
        // Check pressure sensor alarms
        for (const auto &pair : m_pressureSensors)
        {
            const auto &sensor = pair.second;

            if (sensor.pressure > sensor.maxPressure)
            {
                addAlarm(AlarmLevel::FAULT,
                         "Pressure Sensor " + std::to_string(sensor.id) + " exceeded maximum value",
                         "Pressure Sensor " + std::to_string(sensor.id));
            }

            if (sensor.status == DeviceStatus::FAULT)
            {
                addAlarm(AlarmLevel::FAULT,
                         "Pressure Sensor " + std::to_string(sensor.id) + " fault",
                         "Pressure Sensor " + std::to_string(sensor.id));
            }
        }

        // Can add more alarm check logic
    }

    void DeviceManager::addAlarm(AlarmLevel level, const std::string &message, const std::string &source)
    {
        AlarmInfo alarm;
        alarm.id = static_cast<uint32_t>(m_alarms.size() + 1);
        alarm.level = level;
        alarm.message = message;
        alarm.source = source;
        alarm.isActive = true;
        alarm.timestamp = std::chrono::system_clock::now();

        m_alarms.push_back(alarm);

        if (m_alarmCallback)
        {
            m_alarmCallback(alarm);
        }
    }

    // ======== 数据保存相关实现 ========
    bool DeviceManager::initializeDataLogging(const std::string &logDir)
    {
        if (!m_dataLogger)
        {
            m_dataLogger = std::make_unique<DataLogger>();
        }

        bool success = m_dataLogger->initialize(logDir);
        if (success)
        {
            std::cout << "[DeviceManager] Data logging initialized successfully\n";
        }
        else
        {
            std::cerr << "[DeviceManager] Failed to initialize data logging\n";
        }

        return success;
    }

    void DeviceManager::setDataLoggingEnabled(bool enabled)
    {
        m_dataLoggingEnabled = enabled;
        if (enabled)
        {
            std::cout << "[DeviceManager] Data logging enabled\n";
        }
        else
        {
            std::cout << "[DeviceManager] Data logging disabled\n";
        }
    }

    bool DeviceManager::isDataLoggingEnabled() const
    {
        return m_dataLoggingEnabled;
    }

    void DeviceManager::flushDataLogging()
    {
        if (m_dataLogger && m_dataLogger->isInitialized())
        {
            m_dataLogger->flush();
            std::cout << "[DeviceManager] Data flushed to database\n";
        }
    }

    bool DeviceManager::exportDataToCSV(const std::string &filename)
    {
        if (!m_dataLogger || !m_dataLogger->isInitialized())
        {
            std::cerr << "[DeviceManager] Data logger not initialized\n";
            return false;
        }

        return m_dataLogger->exportToCSV(filename);
    }

    int DeviceManager::getDataRecordCount() const
    {
        if (!m_dataLogger || !m_dataLogger->isInitialized())
        {
            return 0;
        }

        return m_dataLogger->getRecordCount();
    }

    std::string DeviceManager::getLogDirectory() const
    {
        if (!m_dataLogger || !m_dataLogger->isInitialized())
        {
            return "";
        }

        return m_dataLogger->getLogDirectory();
    }

} // namespace WaterTest
