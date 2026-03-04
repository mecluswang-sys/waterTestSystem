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

namespace WaterTest
{

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

        // 电动调压阀：2个
        for (int i = 1; i <= 2; ++i)
        {
            RegulatingValve regValve;
            regValve.id = i;
            regValve.name = "RegulatingValve" + std::to_string(i);
            m_regulatingValves[i] = regValve;
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
            if (tcount < 1)
                tcount = 1;

            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_pressureSensors.clear();
            for (int i = 1; i <= pcount; ++i)
            {
                PressureSensor sensor;
                sensor.id = static_cast<uint16_t>(i);
                m_pressureSensors[i] = sensor;
            }

            m_tempSensors.clear();
            for (int i = 1; i <= tcount; ++i)
            {
                TemperatureSensor temp;
                temp.id = static_cast<uint16_t>(i);
                m_tempSensors[i] = temp;
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
    bool DeviceManager::setRegulatingValvePressure(uint16_t id, float pressure)
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        // 写入目标压力到PLC（地址需要根据实际PLC程序调整）
        int offset = (id - 1) * 8; // 假设每个调压阀占用8字节
        auto result = m_plcClient->writeReal(DB_SYSTEM, offset + 20, pressure);

        if (result == S7PLCClient::Result::SUCCESS)
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            auto it = m_regulatingValves.find(id);
            if (it != m_regulatingValves.end())
            {
                it->second.setPressure = pressure;
            }
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
            return false;
        }

        if (index > 7)
        {
            return false;
        }

        auto res = m_plcClient->writeOutputBool(0, static_cast<int>(index), on);
        return res == S7PLCClient::Result::SUCCESS;
    }

    bool DeviceManager::getRelayState(uint8_t index, bool &on) const
    {
        if (!m_plcClient || !m_plcClient->isConnected())
        {
            return false;
        }

        if (index > 7)
        {
            return false;
        }

        bool value = false;
        auto res = m_plcClient->readOutputBool(0, static_cast<int>(index), value);
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
            return false;
        }

        auto &cfg = ConfigManager::getInstance();
        const int dbSensor = cfg.getInt("db.sensor.number", -1);
        const int mainValueRealOffset = cfg.getInt("db.sensor.main_value_real.offset", -1);

        // 如果配置了结构体字段映射（MainValue_Real/MainDecimal），优先按结构体读取
        if (dbSensor >= 0 && mainValueRealOffset >= 0)
        {
            const int baseOffset = cfg.getInt("db.sensor.base_offset", 0);
            const int itemSize = cfg.getInt("db.sensor.item_size", 0);
            const int mainDecimalOffset = cfg.getInt("db.sensor.main_decimal.offset", -1);
            const float scale = cfg.getFloat("db.pressure.scale", 1.0f); // 工程值→Pa（例如 kPa→Pa 乘1000）

            std::lock_guard<std::mutex> lock(m_dataMutex);
            for (auto &pair : m_pressureSensors)
            {
                uint16_t id = pair.first;
                PressureSensor &sensor = pair.second;

                // item_size=0 表示单结构体调试模式；若当前配置了多传感器，避免重复读取同一地址
                if (itemSize <= 0 && id != 1)
                {
                    continue;
                }

                const int itemBase = baseOffset + ((itemSize > 0) ? (static_cast<int>(id) - 1) * itemSize : 0);
                const int realOffset = itemBase + mainValueRealOffset;

                float mainValueReal = 0.0f;
                auto r = m_plcClient->readReal(dbSensor, realOffset, mainValueReal);
                if (r != S7PLCClient::Result::SUCCESS)
                {
                    continue;
                }

                float engineeringValue = mainValueReal;
                if (mainDecimalOffset >= 0)
                {
                    uint8_t rawDecimalBytes[2] = {0, 0};
                    auto rDec = m_plcClient->readDB(dbSensor, itemBase + mainDecimalOffset, 2, rawDecimalBytes);
                    if (rDec == S7PLCClient::Result::SUCCESS)
                    {
                        // S7 大端序：高字节在前；MainDecimal 定义为 UInt
                        const uint16_t mainDecimalRaw = static_cast<uint16_t>((static_cast<uint16_t>(rawDecimalBytes[0]) << 8) |
                                                                              static_cast<uint16_t>(rawDecimalBytes[1]));
                        const int decimals = std::max(0, std::min(6, static_cast<int>(mainDecimalRaw)));
                        engineeringValue = mainValueReal / std::pow(10.0f, static_cast<float>(decimals));
                    }
                }

                sensor.pressure = engineeringValue * scale;
                sensor.status = DeviceStatus::ONLINE;
                sensor.timestamp = std::chrono::system_clock::now();
            }

            return true;
        }

        // 如果配置了新的 DB_Sensor 结构，则从 Pressur_kPa 数组读取
        if (dbSensor >= 0)
        {
            const int baseOffset = cfg.getInt("db.sensor.pressure_kpa.base_offset", 8); // 默认推断：Raw_Pressure(8B) 后为 Pressur_kPa
            const int elemSize = cfg.getInt("db.sensor.pressure_kpa.element_size", 4);  // REAL=4B
            const float scale = cfg.getFloat("db.pressure.scale", 1.0f);                // kPa→Pa 乘1000

            std::lock_guard<std::mutex> lock(m_dataMutex);
            for (auto &pair : m_pressureSensors)
            {
                uint16_t id = pair.first;
                PressureSensor &sensor = pair.second;

                const int offset = baseOffset + (static_cast<int>(id) - 1) * elemSize;
                float kpa = 0.0f;
                auto r = m_plcClient->readReal(dbSensor, offset, kpa);
                if (r == S7PLCClient::Result::SUCCESS)
                {
                    sensor.pressure = kpa * scale;
                    sensor.status = DeviceStatus::ONLINE;
                    sensor.timestamp = std::chrono::system_clock::now();
                }
            }

            return true;
        }

        // 否则，回退到旧的 db.pressure.* 配置
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);

            for (auto &pair : m_pressureSensors)
            {
                uint16_t id = pair.first;
                PressureSensor &sensor = pair.second;

                const std::string prefix = std::string("db.pressure.") + std::to_string(id) + ".";
                const int dbNumber = cfg.getInt(prefix + "number", cfg.getInt("db.pressure.number", DB_PRESSURE_SENSORS));
                const int itemSize = cfg.getInt(prefix + "item_size", cfg.getInt("db.pressure.item_size", 8));
                const int valueOffset = cfg.getInt(prefix + "value_offset", cfg.getInt("db.pressure.value_offset", 2));
                const float scale = cfg.getFloat(prefix + "scale", cfg.getFloat("db.pressure.scale", 1.0f));

                int statusOffset = 0;
                int pressureOffset = 0;
                if (itemSize > 0)
                {
                    const int base = (static_cast<int>(id) - 1) * itemSize;
                    statusOffset = base + 0;
                    pressureOffset = base + valueOffset;
                }
                else
                {
                    statusOffset = 0;
                    pressureOffset = valueOffset;
                }

                int16_t status = 0;
                float pressure = 0.0f;

                auto r1 = m_plcClient->readInt16(dbNumber, statusOffset, status);
                auto r2 = m_plcClient->readReal(dbNumber, pressureOffset, pressure);

                bool updated = false;
                if (r2 == S7PLCClient::Result::SUCCESS)
                {
                    sensor.pressure = pressure * scale;
                    updated = true;
                }
                if (r1 == S7PLCClient::Result::SUCCESS)
                {
                    sensor.status = static_cast<DeviceStatus>(status);
                    updated = true;
                }
                if (updated)
                {
                    sensor.timestamp = std::chrono::system_clock::now();
                }
            }

            return true;
        }
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
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_regulatingValves)
        {
            uint16_t id = pair.first;
            RegulatingValve &valve = pair.second;

            // DB block 6, 每个电动调压阀占用20字节
            int offset = (id - 1) * 20;

            float setPressure, actualPressure;
            int16_t valveStatus, deviceStatus;

            if (m_plcClient->readReal(DB_SYSTEM, offset + 40, setPressure) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readReal(DB_SYSTEM, offset + 44, actualPressure) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_SYSTEM, offset + 48, valveStatus) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_SYSTEM, offset + 50, deviceStatus) == S7PLCClient::Result::SUCCESS)
            {
                valve.setPressure = setPressure;
                valve.actualPressure = actualPressure;
                valve.status = static_cast<ValveStatus>(valveStatus);
                valve.deviceStatus = static_cast<DeviceStatus>(deviceStatus);
                valve.timestamp = std::chrono::system_clock::now();
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
