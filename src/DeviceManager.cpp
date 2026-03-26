/**
 * @file DeviceManager.cpp
 * @brief Device Manager Implementation
 */

#include "DeviceManager.h"
#include "ConfigManager.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace WaterTest
{

    namespace
    {
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

        // 电动调压阀：2个，并初始化 PID / AO / AI 默认地址
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

            // PID 默认参数（现场调试后可通过 setValvePIDGains 覆盖）
            m_valvePIDs.emplace(static_cast<uint16_t>(i),
                                PIDController(0.5, 0.01, 0.1, 0.0, 100.0, 1000.0));

            // AO/AI 默认地址（字节偏移）
            m_valveAoByteOffset[i] = 80 + (i - 1) * 2; // QW80, QW82
            m_valveAiByteOffset[i] = 96 + (i - 1) * 2; // IW96, IW98

            // DI 默认地址（字节 1，偏移按顺序分配）：I1.0/I1.1... → byte=1, bit=0/1/...
            m_valveOpenLimitByte[i]  = 1;
            m_valveOpenLimitBit[i]   = (i - 1) * 3;         // I1.0 / I1.3
            m_valveCloseLimitByte[i] = 1;
            m_valveCloseLimitBit[i]  = (i - 1) * 3 + 1;     // I1.1 / I1.4
            m_valveAlarmByte[i]      = 1;
            m_valveAlarmBit[i]       = (i - 1) * 3 + 2;     // I1.2 / I1.5

            // 压力反馈 AI 默认地址（IW100/IW102），量程 0-1000 kPa
            m_valvePressureAiByteOffset[i] = 100 + (i - 1) * 2; // IW100, IW102
            m_valvePressureRangeMin[i]     = 0.0f;
            m_valvePressureRangeMax[i]     = 1000.0f;
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
            for (int i = 1; i <= 2; ++i)
            {
                const std::string pfx = "valve." + std::to_string(i) + ".";
                const int aoDefault = 80 + (i - 1) * 2;
                const int aiDefault = 96 + (i - 1) * 2;
                m_valveAoByteOffset[i] = cfg.getInt(pfx + "ao.byte_offset", aoDefault);
                m_valveAiByteOffset[i] = cfg.getInt(pfx + "ai.byte_offset", aiDefault);
                m_valveOpenLimitByte[i]  = cfg.getInt(pfx + "di.open_limit.byte",  1);
                m_valveOpenLimitBit[i]   = cfg.getInt(pfx + "di.open_limit.bit",   (i - 1) * 3);
                m_valveCloseLimitByte[i] = cfg.getInt(pfx + "di.close_limit.byte", 1);
                m_valveCloseLimitBit[i]  = cfg.getInt(pfx + "di.close_limit.bit",  (i - 1) * 3 + 1);
                m_valveAlarmByte[i]      = cfg.getInt(pfx + "di.alarm.byte",       1);
                m_valveAlarmBit[i]       = cfg.getInt(pfx + "di.alarm.bit",        (i - 1) * 3 + 2);

                // PID 参数也可从配置文件覆盖
                auto pidIt = m_valvePIDs.find(static_cast<uint16_t>(i));
                if (pidIt != m_valvePIDs.end())
                {
                    pidIt->second.setGains(
                        cfg.getFloat(pfx + "pid.kp", 0.5f),
                        cfg.getFloat(pfx + "pid.ki", 0.01f),
                        cfg.getFloat(pfx + "pid.kd", 0.1f));
                }

                // 压力反馈 AI：IW100/IW102 为默认地址，量程继承 pressure.max_limit
                const int pressAiDefault      = 100 + (i - 1) * 2;
                const float pressMaxDefault   = cfg.getFloat("pressure.max_limit", 1000.0f);
                m_valvePressureAiByteOffset[i] = cfg.getInt(pfx + "pressure_ai.byte_offset", pressAiDefault);
                m_valvePressureRangeMin[i]     = cfg.getFloat(pfx + "pressure_range_min", 0.0f);
                m_valvePressureRangeMax[i]     = cfg.getFloat(pfx + "pressure_range_max", pressMaxDefault);
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

        if (!m_plcClient || !m_plcClient->isConnected())
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
        m_running = false;

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

        checkAlarms();

        if (m_dataUpdateCallback)
        {
            m_dataUpdateCallback();
        }

        return success;
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
    bool DeviceManager::setValveControlMode(uint16_t id, ValveControlMode mode)
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        auto it = m_regulatingValves.find(id);
        if (it == m_regulatingValves.end())
            return false;

        it->second.controlMode = mode;

        // 切换到闭环时重置 PID 积分，防止历史误差导致突变
        if (mode == ValveControlMode::CLOSED_LOOP_PRESSURE)
        {
            auto pidIt = m_valvePIDs.find(id);
            if (pidIt != m_valvePIDs.end())
                pidIt->second.reset();
        }
        return true;
    }

    bool DeviceManager::setValveOpeningPercent(uint16_t id, float percent)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
            return false;

        auto aoIt = m_valveAoByteOffset.find(id);
        if (aoIt == m_valveAoByteOffset.end())
            return false;

        // 转换为 Siemens 4-20mA AO 原始值并写入外设输出区
        const int16_t rawVal = static_cast<int16_t>(percentToAO(percent));
        auto res = m_plcClient->writePeripheralWord(aoIt->second, rawVal);
        if (res == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_regulatingValves.find(id);
            if (it != m_regulatingValves.end())
                it->second.openingSetpoint = percent;
            return true;
        }
        return false;
    }

    bool DeviceManager::setRegulatingValvePressure(uint16_t id, float pressure)
    {
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_regulatingValves.find(id);
            if (it == m_regulatingValves.end())
                return false;
            it->second.setPressure = pressure;
            // 如果当前不是闭环模式，自动切换
            it->second.controlMode = ValveControlMode::CLOSED_LOOP_PRESSURE;
        }
        // 重置对应 PID
        auto pidIt = m_valvePIDs.find(id);
        if (pidIt != m_valvePIDs.end())
            pidIt->second.reset();
        return true;
    }

    void DeviceManager::setValvePIDGains(uint16_t id, double kp, double ki, double kd)
    {
        auto pidIt = m_valvePIDs.find(id);
        if (pidIt != m_valvePIDs.end())
            pidIt->second.setGains(kp, ki, kd);
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
            return false;
        }

        // 支持 Q0.0-Q1.7（index 0-15）：byteOffset = index/8，bit = index%8
        if (index > 15)
        {
            return false;
        }

        const int byteOff = static_cast<int>(index) / 8;
        const int bit     = static_cast<int>(index) % 8;
        auto res = m_plcClient->writeOutputBool(byteOff, bit, on);
        return res == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::getRelayState(uint8_t index, bool &on) const
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        // 支持 Q0.0-Q1.7（index 0-15）
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

        // Emergency stop signal
        auto result = m_plcClient->writeBool(DB_SYSTEM, 10, 0, true);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            setSystemMode(SystemMode::EMERGENCY);
            addAlarm(AlarmLevel::CRITICAL, "Emergency Stop Triggered", "System");
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
            updateAllDevices();
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
        constexpr int kUnset = (std::numeric_limits<int>::min)();
        const int dbSensor = cfg.getInt("db.sensor.number", -1);
        const int mainValueRealOffset = cfg.getInt("db.sensor.main_value_real.offset", -1);

        if (dbSensor < 0 || mainValueRealOffset < 0)
        {
            appendPressureDebugLog("[DEVICE][PRESSURE][UDT] skipped: missing db.sensor.number or db.sensor.main_value_real.offset");
            return false;
        }

        const int baseOffset = cfg.getInt("db.sensor.base_offset", 0);
        const int itemSize = cfg.getInt("db.sensor.item_size", 0);
        const int mainDecimalOffset = cfg.getInt("db.sensor.main_decimal.offset", -1);
        const float scale = cfg.getFloat("db.pressure.scale", 1.0f); // 工程值缩放，当前统一按 kPa 保存/显示
        bool anyUdtReadSuccess = false;

        std::lock_guard<std::mutex> lock(m_dataMutex);
        for (auto &pair : m_pressureSensors)
        {
            uint16_t id = pair.first;
            PressureSensor &sensor = pair.second;

            const std::string sensorPrefix = std::string("db.sensor.") + std::to_string(id) + ".";
            const int sensorDbNumber = cfg.getInt(sensorPrefix + "number", dbSensor);
            const int sensorBaseOffset = cfg.getInt(sensorPrefix + "base_offset", kUnset);
            const int sensorItemSize = cfg.getInt(sensorPrefix + "item_size", kUnset);
            const int sensorMainValueRealOffset = cfg.getInt(sensorPrefix + "main_value_real.offset", kUnset);
            const int sensorMainDecimalOffset = cfg.getInt(sensorPrefix + "main_decimal.offset", kUnset);
            const float sensorScale = cfg.getFloat(sensorPrefix + "scale", scale);

            const bool hasSensorBaseOffset = (sensorBaseOffset != kUnset);
            const bool hasSensorItemSize = (sensorItemSize != kUnset);
            const bool hasSensorMainValueRealOffset = (sensorMainValueRealOffset != kUnset);
            const bool hasSensorMainDecimalOffset = (sensorMainDecimalOffset != kUnset);
            const int resolvedItemSize = hasSensorItemSize ? sensorItemSize : itemSize;

            // item_size=0 时默认只有单结构体；可通过 db.sensor.<id>.* 覆盖读取多传感器。
            if (resolvedItemSize <= 0 && id != 1 && !hasSensorBaseOffset && !hasSensorMainValueRealOffset)
            {
                continue;
            }

            const int itemBase = hasSensorBaseOffset
                                     ? sensorBaseOffset
                                     : baseOffset + ((resolvedItemSize > 0) ? (static_cast<int>(id) - 1) * resolvedItemSize : 0);
            const int realOffset = itemBase + (hasSensorMainValueRealOffset ? sensorMainValueRealOffset : mainValueRealOffset);
            const int decimalOffset = hasSensorMainDecimalOffset ? sensorMainDecimalOffset : mainDecimalOffset;

            uint8_t rawRealBytes[4] = {0, 0, 0, 0};
            auto rRaw = m_plcClient->readDB(sensorDbNumber, realOffset, 4, rawRealBytes);
            if (rRaw != S7PLCClient::Result::SUCCESS)
            {
                std::ostringstream oss;
                oss << "[DEVICE][PRESSURE][UDT] sensor=" << id
                    << " db=" << sensorDbNumber
                    << " itemBase=" << itemBase
                    << " realOffset=" << realOffset
                    << " decimalOffset=" << decimalOffset
                    << " result=readRaw_failed"
                    << " error=\"" << m_plcClient->getLastError() << "\"";
                appendPressureDebugLog(oss.str());
                continue;
            }

            float mainValueReal = 0.0f;
            auto r = m_plcClient->readReal(sensorDbNumber, realOffset, mainValueReal);
            if (r != S7PLCClient::Result::SUCCESS)
            {
                std::ostringstream oss;
                oss << "[DEVICE][PRESSURE][UDT] sensor=" << id
                    << " db=" << sensorDbNumber
                    << " itemBase=" << itemBase
                    << " realOffset=" << realOffset
                    << " decimalOffset=" << decimalOffset
                    << " result=readReal_failed"
                    << " error=\"" << m_plcClient->getLastError() << "\"";
                appendPressureDebugLog(oss.str());
                continue;
            }

            float engineeringValue = mainValueReal;
            if (decimalOffset >= 0)
            {
                uint8_t rawDecimalBytes[2] = {0, 0};
                auto rDec = m_plcClient->readDB(sensorDbNumber, itemBase + decimalOffset, 2, rawDecimalBytes);
                if (rDec == S7PLCClient::Result::SUCCESS)
                {
                    // S7 大端序：高字节在前；MainDecimal 定义为 UInt
                    const uint16_t mainDecimalRaw = static_cast<uint16_t>((static_cast<uint16_t>(rawDecimalBytes[0]) << 8) |
                                                                          static_cast<uint16_t>(rawDecimalBytes[1]));
                    int decimals = static_cast<int>(mainDecimalRaw);
                    if (decimals < 0)
                        decimals = 0;
                    else if (decimals > 6)
                        decimals = 6;
                    engineeringValue = mainValueReal / std::pow(10.0f, static_cast<float>(decimals));
                }
                else
                {
                    std::ostringstream oss;
                    oss << "[DEVICE][PRESSURE][UDT] sensor=" << id
                        << " db=" << sensorDbNumber
                        << " decimalReadOffset=" << (itemBase + decimalOffset)
                        << " result=readDecimal_failed"
                        << " error=\"" << m_plcClient->getLastError() << "\"";
                    appendPressureDebugLog(oss.str());
                }
            }

            sensor.pressure = engineeringValue * sensorScale;
            sensor.status = DeviceStatus::ONLINE;
            sensor.timestamp = std::chrono::system_clock::now();
            anyUdtReadSuccess = true;

            std::ostringstream oss;
            oss << "[DEVICE][PRESSURE][UDT] sensor=" << id
                << " db=" << sensorDbNumber
                << " itemBase=" << itemBase
                << " realOffset=" << realOffset
                << " decimalOffset=" << decimalOffset
                << " rawBytes=0x"
                << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rawRealBytes[0])
                << std::setw(2) << static_cast<int>(rawRealBytes[1])
                << std::setw(2) << static_cast<int>(rawRealBytes[2])
                << std::setw(2) << static_cast<int>(rawRealBytes[3])
                << std::dec
                << " rawReal=" << mainValueReal
                << " engineering=" << engineeringValue
                << " scale=" << sensorScale
                << " pressureKPa=" << sensor.pressure
                << " status=" << static_cast<int>(sensor.status);
            appendPressureDebugLog(oss.str());
        }

        if (!anyUdtReadSuccess)
        {
            appendPressureDebugLog("[DEVICE][PRESSURE][UDT] all sensors read failed");
        }

        return anyUdtReadSuccess;
    }

    bool DeviceManager::readFlowMeters()
    {
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

            // --- 读取 AI 反馈（端子 16-17，4-20mA 位置反馈）---
            auto aiIt = m_valveAiByteOffset.find(id);
            if (aiIt != m_valveAiByteOffset.end())
            {
                int16_t rawAI = 0;
                if (m_plcClient->readPeripheralWord(aiIt->second, rawAI) == S7PLCClient::Result::SUCCESS)
                {
                    valve.openingPercent = aiToPercent(static_cast<int>(rawAI));
                    valve.deviceStatus   = DeviceStatus::ONLINE;
                }
            }

            // --- 读取压力反馈 AI（4-20mA，供 PID 闭环计算 actualPressure）---
            auto pressAiIt = m_valvePressureAiByteOffset.find(id);
            if (pressAiIt != m_valvePressureAiByteOffset.end())
            {
                int16_t rawPressAI = 0;
                if (m_plcClient->readPeripheralWord(pressAiIt->second, rawPressAI) == S7PLCClient::Result::SUCCESS)
                {
                    valve.actualPressure = aiToPressure(
                        static_cast<int>(rawPressAI),
                        m_valvePressureRangeMin.at(id),
                        m_valvePressureRangeMax.at(id));
                }
            }

            // --- 读取 DI：开到位（端子 12-13）、关到位（端子 14-15）---
            auto opByteIt = m_valveOpenLimitByte.find(id);
            auto opBitIt  = m_valveOpenLimitBit.find(id);
            if (opByteIt != m_valveOpenLimitByte.end() && opBitIt != m_valveOpenLimitBit.end())
            {
                bool openLim = false;
                uint8_t buf = 0;
                if (m_plcClient->readDB(0, opByteIt->second, 1, &buf) == S7PLCClient::Result::SUCCESS)
                    openLim = (buf >> opBitIt->second) & 1;
                valve.isOpenLimit = openLim;
            }

            auto clByteIt = m_valveCloseLimitByte.find(id);
            auto clBitIt  = m_valveCloseLimitBit.find(id);
            if (clByteIt != m_valveCloseLimitByte.end() && clBitIt != m_valveCloseLimitBit.end())
            {
                bool closeLim = false;
                uint8_t buf = 0;
                if (m_plcClient->readDB(0, clByteIt->second, 1, &buf) == S7PLCClient::Result::SUCCESS)
                    closeLim = (buf >> clBitIt->second) & 1;
                valve.isCloseLimit = closeLim;
            }

            // --- 读取 DI：综合报警（端子 22-23）---
            auto alByteIt = m_valveAlarmByte.find(id);
            auto alBitIt  = m_valveAlarmBit.find(id);
            if (alByteIt != m_valveAlarmByte.end() && alBitIt != m_valveAlarmBit.end())
            {
                uint8_t buf = 0;
                bool alarm = false;
                if (m_plcClient->readDB(0, alByteIt->second, 1, &buf) == S7PLCClient::Result::SUCCESS)
                    alarm = (buf >> alBitIt->second) & 1;
                valve.alarmActive = alarm;
                if (alarm)
                    valve.deviceStatus = DeviceStatus::FAULT;
            }

            // --- 更新阀门状态标志 ---
            if (valve.isOpenLimit)
                valve.status = ValveStatus::OPEN;
            else if (valve.isCloseLimit)
                valve.status = ValveStatus::CLOSED;
            else if (valve.openingPercent > 1.0f)
                valve.status = ValveStatus::OPENING;
            else
                valve.status = ValveStatus::CLOSED;

            valve.timestamp = std::chrono::system_clock::now();

            // --- 闭环 PID 控制：计算输出开度并写 AO ---
            if (valve.controlMode == ValveControlMode::CLOSED_LOOP_PRESSURE)
            {
                auto pidIt = m_valvePIDs.find(id);
                auto aoIt  = m_valveAoByteOffset.find(id);
                if (pidIt != m_valvePIDs.end() && aoIt != m_valveAoByteOffset.end()
                    && m_plcClient->isConnected())
                {
                    const double pidOut = pidIt->second.compute(
                        static_cast<double>(valve.setPressure),
                        static_cast<double>(valve.actualPressure));
                    const int16_t rawAO = static_cast<int16_t>(
                        percentToAO(static_cast<float>(pidOut)));
                    if (m_plcClient->writePeripheralWord(aoIt->second, rawAO) == S7PLCClient::Result::SUCCESS)
                        valve.openingSetpoint = static_cast<float>(pidOut);
                }
            }
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

} // namespace WaterTest
