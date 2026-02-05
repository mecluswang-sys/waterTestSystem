/**
 * @file DataLogger.cpp
 * @brief Data Logger Implementation
 */

#include "DataLogger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace WaterTest
{

    DataLogger::DataLogger()
    {
    }

    DataLogger::~DataLogger()
    {
        close();
    }

    bool DataLogger::open(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile.close();
        }

        m_logFile.open(filename, std::ios::app);

        if (m_logFile.is_open())
        {
            m_logFile << "\n========== System Start " << getCurrentTimestamp() << " ==========\n";
            return true;
        }

        return false;
    }

    void DataLogger::close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << "========== System Shutdown " << getCurrentTimestamp() << " ==========\n\n";
            m_logFile.close();
        }
    }

    void DataLogger::logPressure(const PressureSensor &sensor)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp()
                      << " [PRESSURE] ID:" << sensor.id
                      << " Value:" << sensor.pressure << " Pa"
                      << " Status:" << static_cast<int>(sensor.status) << "\n";
        }
    }

    void DataLogger::logFlow(const FlowMeter &meter)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp()
                      << " [FLOW] ID:" << meter.id
                      << " Name:" << meter.name
                      << " FlowRate:" << meter.flowRate << " m³/h"
                      << " Total:" << meter.totalFlow << " m³"
                      << " Temp:" << meter.temperature << " ℃\n";
        }
    }

    void DataLogger::logValve(const ElectricValve &valve)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp()
                      << " [VALVE] ID:" << valve.id
                      << " Name:" << valve.name
                      << " Status:" << static_cast<int>(valve.status)
                      << " Opening:" << static_cast<int>(valve.openingDegree) << "%\n";
        }
    }

    void DataLogger::logPump(const FrequencyPump &pump)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp()
                      << " [PUMP] ID:" << pump.id
                      << " Name:" << pump.name
                      << " Running:" << (pump.isRunning ? "Yes" : "No")
                      << " Freq:" << pump.frequency << " Hz"
                      << " Power:" << pump.power << " kW\n";
        }
    }

    void DataLogger::logAlarm(const AlarmInfo &alarm)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            std::string levelStr;
            switch (alarm.level)
            {
            case AlarmLevel::INFO:
                levelStr = "INFO";
                break;
            case AlarmLevel::WARNING:
                levelStr = "WARNING";
                break;
            case AlarmLevel::FAULT:
                levelStr = "FAULT";
                break;
            case AlarmLevel::CRITICAL:
                levelStr = "CRITICAL";
                break;
            }

            m_logFile << getCurrentTimestamp()
                      << " [ALARM] Level:" << levelStr
                      << " Source:" << alarm.source
                      << " Message:" << alarm.message << "\n";
        }
    }

    void DataLogger::logSystemEvent(const std::string &event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp() << " [SYSTEM] " << event << "\n";
        }
    }

    std::string DataLogger::getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return ss.str();
    }

} // namespace WaterTest
