/**
 * @file DataLogger.h
 * @brief 数据记录器
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "DeviceTypes.h"
#include <string>
#include <fstream>
#include <mutex>

namespace WaterTest
{

    class DataLogger
    {
    public:
        DataLogger();
        ~DataLogger();

        bool open(const std::string &filename);
        void close();

        void logPressure(const PressureSensor &sensor);
        void logFlow(const FlowMeter &meter);
        void logValve(const ElectricValve &valve);
        void logPump(const FrequencyPump &pump);
        void logAlarm(const AlarmInfo &alarm);
        void logSystemEvent(const std::string &event);

    private:
        std::ofstream m_logFile;
        std::mutex m_mutex;

        std::string getCurrentTimestamp();
    };

} // namespace WaterTest

#endif // DATA_LOGGER_H
