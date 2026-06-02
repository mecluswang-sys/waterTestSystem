/**
 * @file DataLogger.cpp
 * @brief Data Logger Implementation with SQLite and CSV support
 */

#include "DataLogger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <sqlite3.h>

namespace fs = std::filesystem;

namespace WaterTest
{
    namespace
    {
        constexpr double kKPaPerKgfCm2 = 98.0665;

        static double kPaToKgfCm2(double kpa)
        {
            return kpa / kKPaPerKgfCm2;
        }
    }

    DataLogger::DataLogger()
    {
    }

    DataLogger::~DataLogger()
    {
        shutdown();
    }

    bool DataLogger::initialize(const std::string &logDir)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 已初始化时直接复用，避免重连时重复创建后台线程导致崩溃。
        if (m_initialized)
        {
            return true;
        }

        // 设置日志目录
        if (logDir.empty())
        {
            m_logDirectory = "deploy/logs/" + getCurrentDate();
        }
        else
        {
            m_logDirectory = logDir + "/" + getCurrentDate();
        }

        // 创建目录
        try
        {
            if (!fs::exists(m_logDirectory))
            {
                fs::create_directories(m_logDirectory);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[DataLogger] Failed to create directory: " << e.what() << "\n";
            return false;
        }

        // 初始化数据库
        if (!openDatabase())
        {
            return false;
        }

        if (!createTables())
        {
            closeDatabase();
            return false;
        }

        // 初始化CSV文件
        m_csvFilePath = m_logDirectory + "/sensor_data.csv";

        // 启动后台保存线程
        m_threadRunning = true;
        m_saveThread = std::make_unique<std::thread>(&DataLogger::autoSaveThread, this);

        m_initialized = true;
        std::cout << "[DataLogger] Initialized successfully. DB: " << m_dbFilePath 
                  << ", CSV: " << m_csvFilePath << "\n";
        return true;
    }

    void DataLogger::shutdown()
    {
        // 停止后台线程
        m_threadRunning.store(false);

        if (m_saveThread && m_saveThread->joinable())
        {
            m_saveThread->join();
        }
        m_saveThread.reset();

        // 刷新所有数据
        flush();

        // 关闭资源
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            closeDatabase();
            if (m_logFile.is_open())
            {
                m_logFile << "========== System Shutdown " << getCurrentTimestamp() << " ==========\n\n";
                m_logFile.close();
            }
        }

        m_initialized = false;
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

    bool DataLogger::openDatabase()
    {
        if (m_logDirectory.empty())
        {
            return false;
        }

        m_dbFilePath = m_logDirectory + "/sensor_data.db";

        int rc = sqlite3_open(m_dbFilePath.c_str(), &m_db);
        if (rc != SQLITE_OK)
        {
            std::cerr << "[DataLogger] Cannot open database: " << sqlite3_errmsg(m_db) << "\n";
            m_db = nullptr;
            return false;
        }

        // 使用WAL模式提高并发性能
        sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        
        return true;
    }

    void DataLogger::closeDatabase()
    {
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    bool DataLogger::createTables()
    {
        if (!m_db)
        {
            return false;
        }

        const char *sql = R"(
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                pressure1_kpa REAL,
                pressure2_kpa REAL,
                pressure3_kpa REAL,
                pressure1_status INTEGER,
                pressure2_status INTEGER,
                pressure3_status INTEGER,
                flow_rate REAL,
                total_flow REAL,
                temperature REAL,
                flow_unit_code INTEGER,
                flow_unit_label TEXT,
                empty_pipe_alarm INTEGER,
                excitation_alarm INTEGER
            );
            
            CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_data(timestamp);
        )";

        char *errMsg = nullptr;
        int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK)
        {
            std::cerr << "[DataLogger] SQL error: " << errMsg << "\n";
            sqlite3_free(errMsg);
            return false;
        }

        return true;
    }

    bool DataLogger::insertSensorData(const DataSnapshot &snapshot)
    {
        if (!m_db)
        {
            return false;
        }

        const char *sql = R"(
            INSERT INTO sensor_data (
                timestamp, pressure1_kpa, pressure2_kpa, pressure3_kpa,
                pressure1_status, pressure2_status, pressure3_status,
                flow_rate, total_flow, temperature, flow_unit_code,
                flow_unit_label, empty_pipe_alarm, excitation_alarm
            ) VALUES (datetime(?1, 'unixepoch', '+8 hours'), ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14);
        )";

        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

        if (rc != SQLITE_OK)
        {
            std::cerr << "[DataLogger] Prepare statement error: " << sqlite3_errmsg(m_db) << "\n";
            return false;
        }

        // 绑定参数
        auto timestamp_sec = std::chrono::system_clock::to_time_t(snapshot.timestamp);
        
        sqlite3_bind_int64(stmt, 1, timestamp_sec);
        sqlite3_bind_double(stmt, 2, snapshot.pressure1_kpa);
        sqlite3_bind_double(stmt, 3, snapshot.pressure2_kpa);
        sqlite3_bind_double(stmt, 4, snapshot.pressure3_kpa);
        sqlite3_bind_int(stmt, 5, snapshot.pressure1_status);
        sqlite3_bind_int(stmt, 6, snapshot.pressure2_status);
        sqlite3_bind_int(stmt, 7, snapshot.pressure3_status);
        sqlite3_bind_double(stmt, 8, snapshot.flowRate);
        sqlite3_bind_double(stmt, 9, snapshot.totalFlow);
        sqlite3_bind_double(stmt, 10, snapshot.temperature);
        sqlite3_bind_int(stmt, 11, snapshot.flowUnitCode);
        sqlite3_bind_text(stmt, 12, snapshot.flowUnitLabel.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 13, snapshot.emptyPipeAlarm);
        sqlite3_bind_int(stmt, 14, snapshot.excitationAlarm);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE)
        {
            std::cerr << "[DataLogger] Insert error: " << sqlite3_errmsg(m_db) << "\n";
            return false;
        }

        return true;
    }

    bool DataLogger::writeCSVHeader(std::ofstream &file)
    {
        file << "Timestamp,Pressure1(kgf/cm^2),Pressure2(kgf/cm^2),Pressure3(kgf/cm^2),"
             << "P1_Status,P2_Status,P3_Status,"
             << "FlowRate,TotalFlow,Temperature(°C),"
             << "FlowUnitCode,FlowUnit,EmptyPipeAlarm,ExcitationAlarm\n";
        return file.good();
    }

    bool DataLogger::writeCSVRecord(std::ofstream &file, const DataSnapshot &snapshot)
    {
        // 格式化时间戳
        auto time = std::chrono::system_clock::to_time_t(snapshot.timestamp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

        file << ss.str() << ","
               << kPaToKgfCm2(snapshot.pressure1_kpa) << ","
               << kPaToKgfCm2(snapshot.pressure2_kpa) << ","
               << kPaToKgfCm2(snapshot.pressure3_kpa) << ","
             << snapshot.pressure1_status << ","
             << snapshot.pressure2_status << ","
             << snapshot.pressure3_status << ","
             << snapshot.flowRate << ","
             << snapshot.totalFlow << ","
             << snapshot.temperature << ","
             << snapshot.flowUnitCode << ","
             << snapshot.flowUnitLabel << ","
             << snapshot.emptyPipeAlarm << ","
             << snapshot.excitationAlarm << "\n";

        return file.good();
    }

    void DataLogger::recordSensorData(const PressureSensor &p1, const PressureSensor &p2,
                                     const PressureSensor &p3, const FlowMeter &flow)
    {
        if (!m_initialized)
        {
            return;
        }

        DataSnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::now();
        snapshot.pressure1_kpa = p1.pressure;
        snapshot.pressure2_kpa = p2.pressure;
        snapshot.pressure3_kpa = p3.pressure;
        snapshot.pressure1_status = static_cast<uint16_t>(p1.status);
        snapshot.pressure2_status = static_cast<uint16_t>(p2.status);
        snapshot.pressure3_status = static_cast<uint16_t>(p3.status);
        snapshot.flowRate = flow.flowRate;
        snapshot.totalFlow = flow.totalFlow;
        snapshot.temperature = flow.temperature;
        snapshot.flowUnitCode = flow.unitCode;
        snapshot.flowUnitLabel = flow.unitLabel;
        snapshot.emptyPipeAlarm = flow.emptyPipeAlarm;
        snapshot.excitationAlarm = flow.excitationAlarm;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_dataBuffer.push_back(snapshot);

            // 缓冲区满则立即保存
            if (m_dataBuffer.size() >= BUFFER_SIZE)
            {
                // 批量保存数据库
                if (m_db)
                {
                    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
                    
                    for (const auto &data : m_dataBuffer)
                    {
                        insertSensorData(data);
                    }
                    
                    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
                }

                m_dataBuffer.clear();
            }
        }
    }

    void DataLogger::flush()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 保存剩余的缓冲数据到数据库
        if (m_db && !m_dataBuffer.empty())
        {
            sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            
            for (const auto &data : m_dataBuffer)
            {
                insertSensorData(data);
            }
            
            sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
            m_dataBuffer.clear();
        }

        // 刷新文本日志
        if (m_logFile.is_open())
        {
            m_logFile.flush();
        }
    }

    bool DataLogger::exportToCSV(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_db)
        {
            return false;
        }

        std::ofstream csvFile(filename);
        if (!csvFile.is_open())
        {
            std::cerr << "[DataLogger] Cannot open CSV file: " << filename << "\n";
            return false;
        }

        // 写入CSV头
        if (!writeCSVHeader(csvFile))
        {
            std::cerr << "[DataLogger] Failed to write CSV header\n";
            return false;
        }

        // 从数据库查询数据
        const char *sql = "SELECT timestamp, pressure1_kpa, pressure2_kpa, pressure3_kpa, "
                         "pressure1_status, pressure2_status, pressure3_status, "
                         "flow_rate, total_flow, temperature, flow_unit_code, flow_unit_label, "
                         "empty_pipe_alarm, excitation_alarm FROM sensor_data ORDER BY id;";

        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

        if (rc != SQLITE_OK)
        {
            std::cerr << "[DataLogger] Query error: " << sqlite3_errmsg(m_db) << "\n";
            csvFile.close();
            return false;
        }

        // 逐行读取并写入CSV
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            DataSnapshot snapshot;
            // 从stmt读取数据并写入CSV
            // 这里简化处理，直接从SQL结果读取
            const char *ts = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            csvFile << ts << ","
                   << sqlite3_column_double(stmt, 1) << ","
                   << sqlite3_column_double(stmt, 2) << ","
                   << sqlite3_column_double(stmt, 3) << ","
                   << sqlite3_column_int(stmt, 4) << ","
                   << sqlite3_column_int(stmt, 5) << ","
                   << sqlite3_column_int(stmt, 6) << ","
                   << sqlite3_column_double(stmt, 7) << ","
                   << sqlite3_column_double(stmt, 8) << ","
                   << sqlite3_column_double(stmt, 9) << ","
                   << sqlite3_column_int(stmt, 10) << ","
                   << sqlite3_column_text(stmt, 11) << ","
                   << sqlite3_column_int(stmt, 12) << ","
                   << sqlite3_column_int(stmt, 13) << "\n";
        }

        sqlite3_finalize(stmt);
        csvFile.close();

        std::cout << "[DataLogger] Data exported to CSV: " << filename << "\n";
        return true;
    }

    int DataLogger::getRecordCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_db)
        {
            return 0;
        }

        const char *sql = "SELECT COUNT(*) FROM sensor_data;";
        sqlite3_stmt *stmt = nullptr;

        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            return 0;
        }

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return count;
    }

    void DataLogger::autoSaveThread()
    {
        while (m_threadRunning.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(AUTO_SAVE_INTERVAL_MS));

            if (!m_threadRunning.load())
            {
                break;
            }

            try
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_dataBuffer.empty() && m_db)
                {
                    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

                    for (const auto &data : m_dataBuffer)
                    {
                        insertSensorData(data);
                    }

                    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
                    m_dataBuffer.clear();
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[DataLogger] autoSaveThread exception: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "[DataLogger] autoSaveThread unknown exception" << std::endl;
            }
        }
    }

    std::string DataLogger::getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string DataLogger::getCurrentDate()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d");
        return ss.str();
    }

    std::string DataLogger::getDataDirectory()
    {
        return m_logDirectory;
    }

    void DataLogger::logPressure(const PressureSensor &sensor)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_logFile.is_open())
        {
            m_logFile << getCurrentTimestamp()
                      << " [PRESSURE] ID:" << sensor.id
                      << " Value:" << kPaToKgfCm2(sensor.pressure) << " kgf/cm^2"
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
                      << " Temp:" << meter.temperature << " ℃"
                      << " Unit:" << meter.unitLabel
                      << " EmptyAlm:" << meter.emptyPipeAlarm
                      << " ExcitAlm:" << meter.excitationAlarm << "\n";
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

} // namespace WaterTest
