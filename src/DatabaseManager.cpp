/**
 * @file DatabaseManager.cpp
 * @brief 数据库管理器实现
 */

#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <chrono>
#include <iostream>

namespace WaterTest
{

    DatabaseManager::DatabaseManager()
        : m_lastError("")
    {
    }

    DatabaseManager::~DatabaseManager()
    {
        close();
    }

    bool DatabaseManager::initialize(const std::string &dbPath)
    {
        m_dbPath = dbPath;

        // 确保数据目录存在
        QFileInfo fileInfo(QString::fromStdString(dbPath));
        QDir dir = fileInfo.dir();
        if (!dir.exists())
        {
            if (!dir.mkpath("."))
            {
                m_lastError = "无法创建数据库目录: " + dir.absolutePath().toStdString();
                return false;
            }
        }

        // 创建SQLite数据库连接
        m_database = QSqlDatabase::addDatabase("QSQLITE");
        m_database.setDatabaseName(QString::fromStdString(dbPath));

        // 打开数据库
        if (!m_database.open())
        {
            m_lastError = "无法打开数据库: " + m_database.lastError().text().toStdString();
            return false;
        }

        std::cout << "数据库已打开: " << dbPath << std::endl;

        // 创建数据表
        if (!createTables())
        {
            m_lastError = "创建数据表失败";
            return false;
        }

        std::cout << "数据库初始化成功" << std::endl;
        return true;
    }

    void DatabaseManager::close()
    {
        if (m_database.isOpen())
        {
            m_database.close();
            std::cout << "数据库已关闭" << std::endl;
        }
    }

    bool DatabaseManager::isOpen() const
    {
        return m_database.isOpen();
    }

    bool DatabaseManager::createTables()
    {
        // 压力传感器历史数据表
        QString createPressureTable = R"(
        CREATE TABLE IF NOT EXISTS pressure_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sensor_id INTEGER NOT NULL,
            pressure REAL NOT NULL,
            max_pressure REAL,
            min_pressure REAL,
            status INTEGER,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

        // 流量计历史数据表
        QString createFlowTable = R"(
        CREATE TABLE IF NOT EXISTS flow_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            meter_id INTEGER NOT NULL,
            name TEXT,
            flow_rate REAL NOT NULL,
            total_flow REAL,
            temperature REAL,
            status INTEGER,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

        // 电动阀历史数据表
        QString createValveTable = R"(
        CREATE TABLE IF NOT EXISTS valve_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            valve_id INTEGER NOT NULL,
            name TEXT,
            valve_status INTEGER,
            opening_degree INTEGER,
            operation_time REAL,
            operation_count INTEGER,
            device_status INTEGER,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

        // 变频泵历史数据表
        QString createPumpTable = R"(
        CREATE TABLE IF NOT EXISTS pump_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            pump_id INTEGER NOT NULL,
            name TEXT,
            is_running INTEGER,
            frequency REAL,
            current REAL,
            power REAL,
            speed REAL,
            status INTEGER,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

        // 报警记录表
        QString createAlarmTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            alarm_id INTEGER,
            level INTEGER NOT NULL,
            message TEXT NOT NULL,
            source TEXT,
            is_active INTEGER,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

        // 创建索引以提高查询性能
        QString createPressureIndex = "CREATE INDEX IF NOT EXISTS idx_pressure_timestamp ON pressure_history(timestamp)";
        QString createFlowIndex = "CREATE INDEX IF NOT EXISTS idx_flow_timestamp ON flow_history(timestamp)";
        QString createValveIndex = "CREATE INDEX IF NOT EXISTS idx_valve_timestamp ON valve_history(timestamp)";
        QString createPumpIndex = "CREATE INDEX IF NOT EXISTS idx_pump_timestamp ON pump_history(timestamp)";
        QString createAlarmIndex = "CREATE INDEX IF NOT EXISTS idx_alarm_timestamp ON alarm_history(timestamp)";

        // 执行所有SQL语句
        QStringList sqlStatements = {
            createPressureTable,
            createFlowTable,
            createValveTable,
            createPumpTable,
            createAlarmTable,
            createPressureIndex,
            createFlowIndex,
            createValveIndex,
            createPumpIndex,
            createAlarmIndex};

        for (const QString &sql : sqlStatements)
        {
            if (!executeSql(sql))
            {
                return false;
            }
        }

        return true;
    }

    bool DatabaseManager::executeSql(const QString &sql)
    {
        QSqlQuery query(m_database);
        if (!query.exec(sql))
        {
            m_lastError = "SQL执行失败: " + query.lastError().text().toStdString();
            std::cerr << m_lastError << std::endl;
            std::cerr << "SQL: " << sql.toStdString() << std::endl;
            return false;
        }
        return true;
    }

    bool DatabaseManager::savePressureSensorData(const PressureSensor &sensor)
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
        INSERT INTO pressure_history 
        (sensor_id, pressure, max_pressure, min_pressure, status, timestamp)
        VALUES (?, ?, ?, ?, ?, ?)
    )");

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             sensor.timestamp.time_since_epoch())
                             .count();

        query.addBindValue(sensor.id);
        query.addBindValue(sensor.pressure);
        query.addBindValue(sensor.maxPressure);
        query.addBindValue(sensor.minPressure);
        query.addBindValue(static_cast<int>(sensor.status));
        query.addBindValue(static_cast<qint64>(timestamp));

        if (!query.exec())
        {
            m_lastError = "保存压力传感器数据失败: " + query.lastError().text().toStdString();
            return false;
        }

        return true;
    }

    bool DatabaseManager::saveFlowMeterData(const FlowMeter &meter)
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
        INSERT INTO flow_history 
        (meter_id, name, flow_rate, total_flow, temperature, status, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             meter.timestamp.time_since_epoch())
                             .count();

        query.addBindValue(meter.id);
        query.addBindValue(QString::fromStdString(meter.name));
        query.addBindValue(meter.flowRate);
        query.addBindValue(meter.totalFlow);
        query.addBindValue(meter.temperature);
        query.addBindValue(static_cast<int>(meter.status));
        query.addBindValue(static_cast<qint64>(timestamp));

        if (!query.exec())
        {
            m_lastError = "保存流量计数据失败: " + query.lastError().text().toStdString();
            return false;
        }

        return true;
    }

    bool DatabaseManager::saveValveData(const ElectricValve &valve)
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
        INSERT INTO valve_history 
        (valve_id, name, valve_status, opening_degree, operation_time, 
         operation_count, device_status, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             valve.timestamp.time_since_epoch())
                             .count();

        query.addBindValue(valve.id);
        query.addBindValue(QString::fromStdString(valve.name));
        query.addBindValue(static_cast<int>(valve.status));
        query.addBindValue(valve.openingDegree);
        query.addBindValue(valve.operationTime);
        query.addBindValue(valve.operationCount);
        query.addBindValue(static_cast<int>(valve.deviceStatus));
        query.addBindValue(static_cast<qint64>(timestamp));

        if (!query.exec())
        {
            m_lastError = "保存阀门数据失败: " + query.lastError().text().toStdString();
            return false;
        }

        return true;
    }

    bool DatabaseManager::savePumpData(const FrequencyPump &pump)
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
        INSERT INTO pump_history 
        (pump_id, name, is_running, frequency, current, power, speed, status, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             pump.timestamp.time_since_epoch())
                             .count();

        query.addBindValue(pump.id);
        query.addBindValue(QString::fromStdString(pump.name));
        query.addBindValue(pump.isRunning ? 1 : 0);
        query.addBindValue(pump.frequency);
        query.addBindValue(pump.current);
        query.addBindValue(pump.power);
        query.addBindValue(pump.speed);
        query.addBindValue(static_cast<int>(pump.status));
        query.addBindValue(static_cast<qint64>(timestamp));

        if (!query.exec())
        {
            m_lastError = "保存泵数据失败: " + query.lastError().text().toStdString();
            return false;
        }

        return true;
    }

    bool DatabaseManager::saveAlarmInfo(const AlarmInfo &alarm)
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
        INSERT INTO alarm_history 
        (alarm_id, level, message, source, is_active, timestamp)
        VALUES (?, ?, ?, ?, ?, ?)
    )");

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             alarm.timestamp.time_since_epoch())
                             .count();

        query.addBindValue(alarm.id);
        query.addBindValue(static_cast<int>(alarm.level));
        query.addBindValue(QString::fromStdString(alarm.message));
        query.addBindValue(QString::fromStdString(alarm.source));
        query.addBindValue(alarm.isActive ? 1 : 0);
        query.addBindValue(static_cast<qint64>(timestamp));

        if (!query.exec())
        {
            m_lastError = "保存报警信息失败: " + query.lastError().text().toStdString();
            return false;
        }

        return true;
    }

    bool DatabaseManager::saveAllDeviceData(
        const std::vector<PressureSensor> &sensors,
        const std::vector<FlowMeter> &meters,
        const std::vector<ElectricValve> &valves,
        const std::vector<FrequencyPump> &pumps)
    {
        // 开始事务以提高性能
        m_database.transaction();

        bool success = true;

        // 保存所有压力传感器数据
        for (const auto &sensor : sensors)
        {
            if (!savePressureSensorData(sensor))
            {
                success = false;
                break;
            }
        }

        // 保存所有流量计数据
        if (success)
        {
            for (const auto &meter : meters)
            {
                if (!saveFlowMeterData(meter))
                {
                    success = false;
                    break;
                }
            }
        }

        // 保存所有阀门数据
        if (success)
        {
            for (const auto &valve : valves)
            {
                if (!saveValveData(valve))
                {
                    success = false;
                    break;
                }
            }
        }

        // 保存所有泵数据
        if (success)
        {
            for (const auto &pump : pumps)
            {
                if (!savePumpData(pump))
                {
                    success = false;
                    break;
                }
            }
        }

        // 提交或回滚事务
        if (success)
        {
            m_database.commit();
        }
        else
        {
            m_database.rollback();
        }

        return success;
    }

    std::vector<PressureSensor> DatabaseManager::queryPressureHistory(
        uint16_t sensorId,
        int64_t startTime,
        int64_t endTime)
    {
        std::vector<PressureSensor> result;

        QSqlQuery query(m_database);
        QString sql = R"(
        SELECT sensor_id, pressure, max_pressure, min_pressure, status, timestamp
        FROM pressure_history
        WHERE timestamp BETWEEN ? AND ?
    )";

        if (sensorId > 0)
        {
            sql += " AND sensor_id = ?";
        }

        sql += " ORDER BY timestamp DESC LIMIT 1000";

        query.prepare(sql);
        query.addBindValue(static_cast<qint64>(startTime));
        query.addBindValue(static_cast<qint64>(endTime));
        if (sensorId > 0)
        {
            query.addBindValue(sensorId);
        }

        if (!query.exec())
        {
            m_lastError = "查询压力历史失败: " + query.lastError().text().toStdString();
            return result;
        }

        while (query.next())
        {
            PressureSensor sensor;
            sensor.id = query.value(0).toUInt();
            sensor.pressure = query.value(1).toFloat();
            sensor.maxPressure = query.value(2).toFloat();
            sensor.minPressure = query.value(3).toFloat();
            sensor.status = static_cast<DeviceStatus>(query.value(4).toInt());

            qint64 timestamp = query.value(5).toLongLong();
            sensor.timestamp = std::chrono::system_clock::from_time_t(timestamp);

            result.push_back(sensor);
        }

        return result;
    }

    std::vector<FlowMeter> DatabaseManager::queryFlowHistory(
        uint16_t meterId,
        int64_t startTime,
        int64_t endTime)
    {
        std::vector<FlowMeter> result;

        QSqlQuery query(m_database);
        QString sql = R"(
        SELECT meter_id, name, flow_rate, total_flow, temperature, status, timestamp
        FROM flow_history
        WHERE timestamp BETWEEN ? AND ?
    )";

        if (meterId > 0)
        {
            sql += " AND meter_id = ?";
        }

        sql += " ORDER BY timestamp DESC LIMIT 1000";

        query.prepare(sql);
        query.addBindValue(static_cast<qint64>(startTime));
        query.addBindValue(static_cast<qint64>(endTime));
        if (meterId > 0)
        {
            query.addBindValue(meterId);
        }

        if (!query.exec())
        {
            m_lastError = "查询流量历史失败: " + query.lastError().text().toStdString();
            return result;
        }

        while (query.next())
        {
            FlowMeter meter;
            meter.id = query.value(0).toUInt();
            meter.name = query.value(1).toString().toStdString();
            meter.flowRate = query.value(2).toFloat();
            meter.totalFlow = query.value(3).toFloat();
            meter.temperature = query.value(4).toFloat();
            meter.status = static_cast<DeviceStatus>(query.value(5).toInt());

            qint64 timestamp = query.value(6).toLongLong();
            meter.timestamp = std::chrono::system_clock::from_time_t(timestamp);

            result.push_back(meter);
        }

        return result;
    }

    std::vector<AlarmInfo> DatabaseManager::queryAlarmHistory(int level, int limit)
    {
        std::vector<AlarmInfo> result;

        QSqlQuery query(m_database);
        QString sql = R"(
        SELECT alarm_id, level, message, source, is_active, timestamp
        FROM alarm_history
    )";

        if (level >= 0)
        {
            sql += " WHERE level = ?";
        }

        sql += " ORDER BY timestamp DESC LIMIT ?";

        query.prepare(sql);
        if (level >= 0)
        {
            query.addBindValue(level);
        }
        query.addBindValue(limit);

        if (!query.exec())
        {
            m_lastError = "查询报警历史失败: " + query.lastError().text().toStdString();
            return result;
        }

        while (query.next())
        {
            AlarmInfo alarm;
            alarm.id = query.value(0).toUInt();
            alarm.level = static_cast<AlarmLevel>(query.value(1).toInt());
            alarm.message = query.value(2).toString().toStdString();
            alarm.source = query.value(3).toString().toStdString();
            alarm.isActive = query.value(4).toBool();

            qint64 timestamp = query.value(5).toLongLong();
            alarm.timestamp = std::chrono::system_clock::from_time_t(timestamp);

            result.push_back(alarm);
        }

        return result;
    }

    DatabaseManager::DatabaseStats DatabaseManager::getStatistics()
    {
        DatabaseStats stats = {};

        QSqlQuery query(m_database);

        // 获取各表记录数
        query.exec("SELECT COUNT(*) FROM pressure_history");
        if (query.next())
            stats.pressureRecords = query.value(0).toLongLong();

        query.exec("SELECT COUNT(*) FROM flow_history");
        if (query.next())
            stats.flowRecords = query.value(0).toLongLong();

        query.exec("SELECT COUNT(*) FROM valve_history");
        if (query.next())
            stats.valveRecords = query.value(0).toLongLong();

        query.exec("SELECT COUNT(*) FROM pump_history");
        if (query.next())
            stats.pumpRecords = query.value(0).toLongLong();

        query.exec("SELECT COUNT(*) FROM alarm_history");
        if (query.next())
            stats.alarmRecords = query.value(0).toLongLong();

        stats.totalRecords = stats.pressureRecords + stats.flowRecords +
                             stats.valveRecords + stats.pumpRecords + stats.alarmRecords;

        // 获取数据库文件大小
        QFileInfo fileInfo(QString::fromStdString(m_dbPath));
        if (fileInfo.exists())
        {
            stats.databaseSizeBytes = fileInfo.size();
        }

        return stats;
    }

    bool DatabaseManager::cleanOldData(int daysToKeep)
    {
        // 计算截止时间戳
        auto now = std::chrono::system_clock::now();
        auto cutoffTime = now - std::chrono::hours(24 * daysToKeep);
        qint64 cutoffTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                     cutoffTime.time_since_epoch())
                                     .count();

        QStringList tables = {
            "pressure_history",
            "flow_history",
            "valve_history",
            "pump_history",
            "alarm_history"};

        m_database.transaction();

        bool success = true;
        for (const QString &table : tables)
        {
            QString sql = QString("DELETE FROM %1 WHERE timestamp < ?").arg(table);
            QSqlQuery query(m_database);
            query.prepare(sql);
            query.addBindValue(cutoffTimestamp);

            if (!query.exec())
            {
                m_lastError = "清理旧数据失败: " + query.lastError().text().toStdString();
                success = false;
                break;
            }
        }

        if (success)
        {
            // 优化数据库（回收空间）
            executeSql("VACUUM");
            m_database.commit();
            std::cout << "已清理" << daysToKeep << "天前的旧数据" << std::endl;
        }
        else
        {
            m_database.rollback();
        }

        return success;
    }

    std::string DatabaseManager::getLastError() const
    {
        return m_lastError;
    }

} // namespace WaterTest
