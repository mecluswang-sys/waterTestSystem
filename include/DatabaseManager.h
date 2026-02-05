/**
 * @file DatabaseManager.h
 * @brief 数据库管理器 - 负责数据库连接和数据存储
 */

#ifndef WATERTEST_DATABASE_MANAGER_H
#define WATERTEST_DATABASE_MANAGER_H

#include "DeviceTypes.h"
#include <QSqlDatabase>
#include <QString>
#include <memory>
#include <vector>

namespace WaterTest
{

    /**
     * @brief 数据库管理器类
     *
     * 负责：
     * 1. SQLite数据库的初始化和连接
     * 2. 创建数据表结构
     * 3. 存储设备历史数据
     * 4. 存储报警记录
     * 5. 数据查询和统计
     */
    class DatabaseManager
    {
    public:
        DatabaseManager();
        ~DatabaseManager();

        /**
         * @brief 初始化数据库
         * @param dbPath 数据库文件路径，默认为"data/system.db"
         * @return 成功返回true，失败返回false
         */
        bool initialize(const std::string &dbPath = "data/system.db");

        /**
         * @brief 关闭数据库连接
         */
        void close();

        /**
         * @brief 检查数据库是否已打开
         */
        bool isOpen() const;

        // ========== 数据插入接口 ==========

        /**
         * @brief 保存压力传感器数据
         */
        bool savePressureSensorData(const PressureSensor &sensor);

        /**
         * @brief 保存流量计数据
         */
        bool saveFlowMeterData(const FlowMeter &meter);

        /**
         * @brief 保存电动阀数据
         */
        bool saveValveData(const ElectricValve &valve);

        /**
         * @brief 保存变频泵数据
         */
        bool savePumpData(const FrequencyPump &pump);

        /**
         * @brief 保存报警信息
         */
        bool saveAlarmInfo(const AlarmInfo &alarm);

        /**
         * @brief 批量保存所有设备数据
         */
        bool saveAllDeviceData(
            const std::vector<PressureSensor> &sensors,
            const std::vector<FlowMeter> &meters,
            const std::vector<ElectricValve> &valves,
            const std::vector<FrequencyPump> &pumps);

        // ========== 数据查询接口 ==========

        /**
         * @brief 查询指定时间范围内的压力传感器历史数据
         * @param sensorId 传感器ID，0表示所有传感器
         * @param startTime 开始时间（Unix时间戳）
         * @param endTime 结束时间（Unix时间戳）
         */
        std::vector<PressureSensor> queryPressureHistory(
            uint16_t sensorId,
            int64_t startTime,
            int64_t endTime);

        /**
         * @brief 查询指定时间范围内的流量计历史数据
         */
        std::vector<FlowMeter> queryFlowHistory(
            uint16_t meterId,
            int64_t startTime,
            int64_t endTime);

        /**
         * @brief 查询报警历史记录
         * @param level 报警级别，-1表示所有级别
         * @param limit 返回记录数量限制
         */
        std::vector<AlarmInfo> queryAlarmHistory(
            int level = -1,
            int limit = 100);

        /**
         * @brief 获取数据库统计信息
         */
        struct DatabaseStats
        {
            int64_t totalRecords;      // 总记录数
            int64_t pressureRecords;   // 压力传感器记录数
            int64_t flowRecords;       // 流量计记录数
            int64_t valveRecords;      // 阀门记录数
            int64_t pumpRecords;       // 泵记录数
            int64_t alarmRecords;      // 报警记录数
            int64_t databaseSizeBytes; // 数据库文件大小（字节）
        };

        DatabaseStats getStatistics();

        /**
         * @brief 清理过期数据
         * @param daysToKeep 保留最近N天的数据
         */
        bool cleanOldData(int daysToKeep);

        /**
         * @brief 获取最后一次错误信息
         */
        std::string getLastError() const;

    private:
        /**
         * @brief 创建数据表
         */
        bool createTables();

        /**
         * @brief 执行SQL语句
         */
        bool executeSql(const QString &sql);

        QSqlDatabase m_database; // Qt数据库对象
        std::string m_lastError; // 最后的错误信息
        std::string m_dbPath;    // 数据库路径
    };

} // namespace WaterTest

#endif // WATERTEST_DATABASE_MANAGER_H
