/**
 * @file DataLogger.h
 * @brief 数据记录器 - 支持SQLite数据库和CSV备份
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "DeviceTypes.h"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <deque>
#include <chrono>
#include <thread>
#include <atomic>

class sqlite3;

namespace WaterTest
{
    // 传感器数据快照（用于批量保存）
    struct DataSnapshot
    {
        std::chrono::system_clock::time_point timestamp;
        
        // 压力数据
        double pressure1_kpa = 0;
        double pressure2_kpa = 0;
        double pressure3_kpa = 0;
        uint16_t pressure1_status = 0;
        uint16_t pressure2_status = 0;
        uint16_t pressure3_status = 0;
        
        // 流量数据
        double flowRate = 0;
        double totalFlow = 0;
        double temperature = 0;
        uint16_t flowUnitCode = 0;
        std::string flowUnitLabel;
        uint16_t emptyPipeAlarm = 0;
        uint16_t excitationAlarm = 0;
    };

    class DataLogger
    {
    public:
        DataLogger();
        ~DataLogger();

        // 初始化数据库和日志文件
        bool initialize(const std::string &logDir = "");
        void shutdown();

        // 检查是否已初始化
        bool isInitialized() const { return m_initialized; }

        // 获取当前日志目录
        std::string getLogDirectory() const { return m_logDirectory; }

        // 文本日志（兼容旧接口）
        bool open(const std::string &filename);
        void close();

        void logPressure(const PressureSensor &sensor);
        void logFlow(const FlowMeter &meter);
        void logValve(const ElectricValve &valve);
        void logPump(const FrequencyPump &pump);
        void logAlarm(const AlarmInfo &alarm);
        void logSystemEvent(const std::string &event);

        // 新增：数据库保存接口
        void recordSensorData(const PressureSensor &p1, const PressureSensor &p2, 
                            const PressureSensor &p3, const FlowMeter &flow);
        
        // 强制刷新（将内存中的数据写入数据库和CSV）
        void flush();
        
        // 导出数据为CSV格式
        bool exportToCSV(const std::string &filename);
        
        // 查询数据库中的记录数
        int getRecordCount() const;

    private:
        // 数据库操作
        bool openDatabase();
        void closeDatabase();
        bool createTables();
        bool insertSensorData(const DataSnapshot &snapshot);
        
        // CSV操作
        bool writeCSVHeader(std::ofstream &file);
        bool writeCSVRecord(std::ofstream &file, const DataSnapshot &snapshot);
        
        // 时间戳和目录
        std::string getCurrentTimestamp();
        std::string getCurrentDate(); // YYYY-MM-DD格式
        std::string getDataDirectory();
        
        // 后台保存线程
        void autoSaveThread();
        
        // 成员变量
        std::ofstream m_logFile;
        mutable std::mutex m_mutex;
        
        sqlite3 *m_db = nullptr;
        std::string m_logDirectory;
        std::string m_dbFilePath;
        std::string m_csvFilePath;
        
        bool m_initialized = false;
        
        // 内存缓冲区（用于批量保存）
        std::deque<DataSnapshot> m_dataBuffer;
        const size_t BUFFER_SIZE = 100; // 缓冲100条记录后自动保存
        const int AUTO_SAVE_INTERVAL_MS = 5000; // 5秒自动保存一次
        
        // 后台线程
        std::unique_ptr<std::thread> m_saveThread;
        std::atomic<bool> m_threadRunning{false};
        std::mutex m_threadMutex;
        std::string m_writeThreadName = "SaveThread";
    };

} // namespace WaterTest

#endif // DATA_LOGGER_H
