/**
 * @file DeviceManager.h
 * @brief 设备管理器
 * @description 管理所有测试设备，处理数据采集和控制
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "DeviceTypes.h"
#include "S7PLCClient.h"
#include "DataLogger.h"
#include "PIDController.h"
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace WaterTest
{

    class DeviceManager
    {
    public:
        // 回调函数类型
        using AlarmCallback = std::function<void(const AlarmInfo &)>;
        using DataUpdateCallback = std::function<void()>;

        DeviceManager();
        ~DeviceManager();

        /**
         * @brief 初始化设备管理器
         * @param plcClient PLC客户端
         * @return 是否成功
         */
        bool initialize(std::shared_ptr<S7PLCClient> plcClient);

        /**
         * @brief 启动数据采集
         * @param intervalMs 采集间隔(毫秒)
         * @return 是否成功
         */
        bool startDataCollection(int intervalMs = 1000);

        /**
         * @brief 停止数据采集
         */
        void stopDataCollection();

        // ======== 运行状态查询（用于UI自检/诊断） ========
        bool hasPlcClient() const;
        bool isPlcConnected() const;
        bool isDataCollecting() const;
        std::string getPlcLastError() const;

        /**
         * @brief 更新所有设备数据
         * @return 是否成功
         */
        bool updateAllDevices();

        // ======== 压力传感器相关 ========
        /**
         * @brief 获取压力传感器数据
         * @param id 传感器ID (1-11)
         * @return 传感器数据
         */
        PressureSensor getPressureSensor(uint16_t id) const;

        /**
         * @brief 获取所有压力传感器
         * @return 传感器列表
         */
        std::vector<PressureSensor> getAllPressureSensors() const;

        // ======== 流量计相关 ========
        /**
         * @brief 获取流量计数据
         * @param id 流量计ID (1-4)
         * @return 流量计数据
         */
        FlowMeter getFlowMeter(uint16_t id) const;

        /**
         * @brief 获取所有流量计
         * @return 流量计列表
         */
        std::vector<FlowMeter> getAllFlowMeters() const;

        // ======== 温度传感器相关 ========
        /**
         * @brief 获取所有温度传感器
         * @return 温度传感器列表
         */
        std::vector<TemperatureSensor> getAllTemperatureSensors() const;

        /**
         * @brief 获取温度传感器数据
         * @param id 传感器ID
         * @return 传感器数据
         */
        TemperatureSensor getTemperatureSensor(uint16_t id) const;

        // ======== 电动阀相关 ========
        /**
         * @brief 获取电动阀数据
         * @param id 阀门ID
         * @return 阀门数据
         */
        ElectricValve getValve(uint16_t id) const;

        /**
         * @brief 获取所有电动阀
         * @return 阀门列表
         */
        std::vector<ElectricValve> getAllValves() const;

        /**
         * @brief 控制电动阀
         * @param id 阀门ID
         * @param open true=打开, false=关闭
         * @return 是否成功
         */
        bool controlValve(uint16_t id, bool open);

        // ======== 变频泵相关 ========
        /**
         * @brief 控制变频泵
         * @param id 泵ID (1-2)
         * @param start true=启动, false=停止
         * @return 是否成功
         */
        bool controlPump(uint16_t id, bool start);

        /**
         * @brief 设置变频泵频率
         * @param id 泵ID
         * @param frequency 频率 (Hz)
         * @return 是否成功
         */
        bool setPumpFrequency(uint16_t id, float frequency);

        /**
         * @brief 获取变频泵状态
         * @param id 泵ID
         * @return 泵数据
         */
        FrequencyPump getPump(uint16_t id) const;

        /**
         * @brief 获取所有变频泵
         * @return 泵列表
         */
        std::vector<FrequencyPump> getAllPumps() const;

        // ======== 直流电源（KEYSIGHT E3634A / RS232）相关 ========
        /**
         * @brief 设置 E3634A 输出开关
         * @param on true=开启输出, false=关闭输出
         * @return 是否成功
         */
        bool setDcPowerOutput(bool on);

        /**
         * @brief 设置 E3634A 电压/电流设定值
         * @param voltageV 电压设定值(V)
         * @param currentA 电流限值(A)
         * @return 是否成功
         */
        bool setDcPowerSetpoint(float voltageV, float currentA);

        /**
         * @brief 读取 E3634A 实测电压/电流
         * @param voltageV 输出实测电压(V)
         * @param currentA 输出实测电流(A)
         * @return 是否成功
         */
        bool readDcPowerMeasurements(float &voltageV, float &currentA);

        /**
         * @brief 读取 E3634A 设备识别字符串(*IDN?)
         * @param idn 输出设备识别字符串
         * @return 是否成功
         */
        bool getDcPowerIdentity(std::string &idn);

        /**
         * @brief 读取 E3634A 当前错误队列（SYST:ERR?）
         * @param errorText 输出错误文本
         * @return 是否成功
         */
        bool readDcPowerErrorCode(std::string &errorText);

        /**
         * @brief 获取最近一次 E3634A 操作错误（软件侧）
         * @return 错误文本，空表示无错误
         */
        std::string getDcPowerLastError() const;

        // ======== 电动调压阀相关 ========
        /**
         * @brief 开环模式：直接设定阀门开度，写入 AO (端子10-11, 4-20mA)
         * @param id      调压阀ID (1..N，N由 valve.count 配置)
         * @param percent 开度百分比 (0-100%)
         * @return 是否成功
         */
        bool setValveOpeningPercent(uint16_t id, float percent);

        /**
         * @brief 获取电动调压阀状态
         * @param id 调压阀ID
         * @return 调压阀数据
         */
        RegulatingValve getRegulatingValve(uint16_t id) const;

        /**
         * @brief 设置电动阀开度
         * @param id 阀门ID
         * @param degree 开度百分比(0-100)
         * @return 是否成功
         */
        bool setValveOpening(uint16_t id, uint8_t degree);

        /**
         * @brief 获取所有电动调压阀
         * @return 调压阀列表
         */
        std::vector<RegulatingValve> getAllRegulatingValves() const;

        // ======== 继电器/输出控制（含特殊映射） ========
        /**
         * @brief 控制继电器（DQ输出）通断
         * @param index 线性索引：
         *   - 0~3 特殊映射到 M100.0~M100.3
         *   - 5 特殊映射到 M100.4（站1电磁阀5）
         *   - 其余索引按 Q 区线性映射
         * @param on true=闭合(通/得电), false=断开(失电)
         * @return 是否成功
         */
        bool setRelay(uint8_t index, bool on);

        /**
         * @brief 读取继电器（DQ输出）当前状态
         * @param index 线性索引：
         *   - 0~3 特殊映射到 M100.0~M100.3
         *   - 5 特殊映射到 M100.4（站1电磁阀5）
         *   - 其余索引按 Q 区线性映射
         * @param on 输出参数，读取到的状态（true=通/得电）
         * @return 是否读取成功
         */
        bool getRelayState(uint8_t index, bool &on) const;

        /**
         * @brief 读取 M 区（Merker）位状态（用于物理按钮等离散输入映射）
         * @param byteOffset M 字节偏移（例如 M101.x 对应 101）
         * @param bit 位偏移（0-7）
         * @param on 输出参数，读取到的状态
         * @return 是否读取成功
         */
        bool readMerkerState(uint16_t byteOffset, uint8_t bit, bool &on) const;

        // ======== 系统控制 ========
        /**
         * @brief 设置系统运行模式
         * @param mode 运行模式
         * @return 是否成功
         */
        bool setSystemMode(SystemMode mode);

        /**
         * @brief 获取系统状态
         * @return 系统状态
         */
        SystemStatus getSystemStatus() const;

        /**
         * @brief 紧急停止
         * @return 是否成功
         */
        bool emergencyStop();

        /**
         * @brief 切换测试管路
         * @param line 管路类型
         * @return 是否成功
         */
        bool switchTestLine(TestLine line);

        // ======== PLC侧自检命令/状态 ========
        /**
         * @brief 使能/关闭 PLC 侧自检流程
         * @param enabled true=使能, false=关闭
         * @return 是否成功
         */
        bool setPlcSelfCheckEnable(bool enabled);

        /**
         * @brief 触发 PLC 侧自检开始（脉冲）
         * @return 是否成功
         */
        bool startPlcSelfCheck();

        /**
         * @brief 触发 PLC 侧自检中止（脉冲）
         * @return 是否成功
         */
        bool abortPlcSelfCheck();

        /**
         * @brief 触发 PLC 侧自检复位（脉冲）
         * @return 是否成功
         */
        bool resetPlcSelfCheck();

        /**
         * @brief 获取最近一次读取到的 PLC 自检状态
         * @return 自检状态快照
         */
        PlcSelfCheckStatus getPlcSelfCheckStatus() const;

        // ======== 数据保存相关 ========
        /**
         * @brief 初始化数据保存系统
         * @param logDir 日志目录（可选，默认为deploy/logs）
         * @return 是否成功
         */
        bool initializeDataLogging(const std::string &logDir = "");

        /**
         * @brief 启用/禁用数据保存
         * @param enabled 是否启用
         */
        void setDataLoggingEnabled(bool enabled);

        /**
         * @brief 是否启用了数据保存
         * @return true如果已启用
         */
        bool isDataLoggingEnabled() const;

        /**
         * @brief 强制刷新（将缓冲数据写入数据库和CSV）
         */
        void flushDataLogging();

        /**
         * @brief 导出数据为CSV格式
         * @param filename CSV文件路径
         * @return 是否成功
         */
        bool exportDataToCSV(const std::string &filename);

        /**
         * @brief 获取已保存的数据记录数
         * @return 记录数
         */
        int getDataRecordCount() const;

        /**
         * @brief 获取日志目录路径
         * @return 日志目录路径
         */
        std::string getLogDirectory() const;

        // ======== 报警管理 ========
        /**
         * @brief 获取活动报警列表
         * @return 报警列表
         */
        std::vector<AlarmInfo> getActiveAlarms() const;

        /**
         * @brief 确认报警
         * @param alarmId 报警ID
         * @return 是否成功
         */
        bool acknowledgeAlarm(uint32_t alarmId);

        /**
         * @brief 设置报警回调
         * @param callback 回调函数
         */
        void setAlarmCallback(AlarmCallback callback);

        /**
         * @brief 设置数据更新回调
         * @param callback 回调函数
         */
        void setDataUpdateCallback(DataUpdateCallback callback);

    private:
        std::shared_ptr<S7PLCClient> m_plcClient;

        // 设备数据
        std::map<uint16_t, PressureSensor> m_pressureSensors;
        std::map<uint16_t, FlowMeter> m_flowMeters;
        std::map<uint16_t, ElectricValve> m_valves;
        std::map<uint16_t, FrequencyPump> m_pumps;
        std::map<uint16_t, TemperatureSensor> m_tempSensors;
        std::map<uint16_t, RegulatingValve> m_regulatingValves;

        // 电动调压阀最小化配置：仅保留开度命令 MD 和开度反馈 MD。
        std::map<uint16_t, int>  m_valveAoMerkerByteOffset;   // id -> MD 偏移（命令）
        std::map<uint16_t, int>  m_valveAiMerkerByteOffset;   // id -> MD 偏移（反馈）

        SystemStatus m_systemStatus;
        std::vector<AlarmInfo> m_alarms;
        PlcSelfCheckStatus m_plcSelfCheckStatus;

        // 回调函数
        AlarmCallback m_alarmCallback;
        DataUpdateCallback m_dataUpdateCallback;

        // 数据保存相关
        std::unique_ptr<DataLogger> m_dataLogger;
        bool m_dataLoggingEnabled = false;

        // 数据采集线程相关
        bool m_running;
        std::shared_ptr<std::thread> m_collectionThread;
        mutable std::mutex m_dataMutex;
        std::string m_dcPowerLastError;

        // 私有方法
        void collectionThreadFunc(int intervalMs);
        bool readPressureSensors();
        bool readFlowMeters();
        bool readValves();
        bool readPumps();
        bool readTemperatureSensors();
        bool readRegulatingValves();
        bool readSystemStatus();
        bool readPlcSelfCheckStatus();
        bool writePlcSelfCheckCmdBit(const std::string &cmdKeyPrefix, bool pulse);
        void checkAlarms();
        void addAlarm(AlarmLevel level, const std::string &message, const std::string &source);
        void setDcPowerLastError(const std::string &errorText);

        // PLC数据地址定义（需要根据实际PLC程序调整）
        static constexpr int DB_PRESSURE_SENSORS = 1; // 压力传感器DB块
        static constexpr int DB_FLOW_METERS = 2;      // 流量计DB块
        static constexpr int DB_VALVES = 3;           // 电动阀DB块
        static constexpr int DB_PUMPS = 4;            // 变频泵DB块
        static constexpr int DB_SYSTEM = 5;           // 系统状态DB块
    };

} // namespace WaterTest

#endif // DEVICE_MANAGER_H
