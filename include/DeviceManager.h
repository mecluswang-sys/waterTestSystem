/**
 * @file DeviceManager.h
 * @brief 设备管理器
 * @description 管理所有测试设备，处理数据采集和控制
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "DeviceTypes.h"
#include "S7PLCClient.h"
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
         * @brief 获取温度传感器数据
         * @param id 传感器ID
         * @return 温度传感器数据
         */
        TemperatureSensor getTemperatureSensor(uint16_t id) const;

        /**
         * @brief 获取所有温度传感器
         * @return 温度传感器列表
         */
        std::vector<TemperatureSensor> getAllTemperatureSensors() const;

        // ======== 电动阀相关 ========
        /**
         * @brief 控制电动阀
         * @param id 阀门ID (1-11)
         * @param open true=开启, false=关闭
         * @return 是否成功
         */
        bool controlValve(uint16_t id, bool open);

        /**
         * @brief 设置阀门开度
         * @param id 阀门ID
         * @param degree 开度 (0-100%)
         * @return 是否成功
         */
        bool setValveOpening(uint16_t id, uint8_t degree);

        /**
         * @brief 获取电动阀状态
         * @param id 阀门ID
         * @return 阀门数据
         */
        ElectricValve getValve(uint16_t id) const;

        /**
         * @brief 获取所有电动阀
         * @return 阀门列表
         */
        std::vector<ElectricValve> getAllValves() const;

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

        // ======== 电动调压阀相关 ========
        /**
         * @brief 设置调压阀控制模式
         * @param id           调压阀ID (1-2)
         * @param mode         开环 / 闭环压力模式
         * @return 是否成功
         */
        bool setValveControlMode(uint16_t id, ValveControlMode mode);

        /**
         * @brief 开环模式：直接设定阀门开度，写入 AO (端子10-11, 4-20mA)
         * @param id      调压阀ID (1-2)
         * @param percent 开度百分比 (0-100%)
         * @return 是否成功
         */
        bool setValveOpeningPercent(uint16_t id, float percent);

        /**
         * @brief 闭环模式：设置目标压力，由 PID 自动调节开度
         * @param id       调压阀ID (1-2)
         * @param pressure 目标压力 (kPa)
         * @return 是否成功
         */
        bool setRegulatingValvePressure(uint16_t id, float pressure);

        /**
         * @brief 设置 PID 参数（现场调试用）
         * @param id  调压阀ID
         * @param kp  比例增益
         * @param ki  积分增益
         * @param kd  微分增益
         */
        void setValvePIDGains(uint16_t id, double kp, double ki, double kd);

        /**
         * @brief 获取电动调压阀状态
         * @param id 调压阀ID
         * @return 调压阀数据
         */
        RegulatingValve getRegulatingValve(uint16_t id) const;

        /**
         * @brief 获取所有电动调压阀
         * @return 调压阀列表
         */
        std::vector<RegulatingValve> getAllRegulatingValves() const;

        // ======== 继电器/输出控制(Q区) ========
        /**
         * @brief 控制继电器（DQ输出）通断，支持 Q0.0-Q1.7
         * @param index 线性索引：0-7 → Q0.0-Q0.7，8-15 → Q1.0-Q1.7
         * @param on true=闭合(通/得电), false=断开(失电)
         * @return 是否成功
         */
        bool setRelay(uint8_t index, bool on);

        /**
         * @brief 读取继电器（DQ输出）当前状态，支持 Q0.0-Q1.7
         * @param index 线性索引：0-7 → Q0.0-Q0.7，8-15 → Q1.0-Q1.7
         * @param on 输出参数，读取到的状态（true=通/得电）
         * @return 是否读取成功
         */
        bool getRelayState(uint8_t index, bool &on) const;

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

        // 电动调压阀闭环控制相关
        std::map<uint16_t, PIDController> m_valvePIDs;  // 每个调压阀一个 PID 实例
        // AO/AI 外设地址（字节偏移，从配置文件加载，默认值仅供展示）
        // S7-1200 SM1232 AO: QW80/QW82..., SM1231 AI: IW96/IW98...
        std::map<uint16_t, int> m_valveAoByteOffset; // id -> AO 字节偏移
        std::map<uint16_t, int> m_valveAiByteOffset; // id -> AI 字节偏移（位置反馈）
        // 压力反馈 AI（4-20mA 直接输入，供 PID 闭环使用）
        std::map<uint16_t, int>   m_valvePressureAiByteOffset; // id -> 压力 AI 字节偏移
        std::map<uint16_t, float> m_valvePressureRangeMin;     // id -> 量程下限 kPa
        std::map<uint16_t, float> m_valvePressureRangeMax;     // id -> 量程上限 kPa
        // DI 字节/位描述（限位开关、报警）
        std::map<uint16_t, int> m_valveOpenLimitByte; // 开到位 DI 字节
        std::map<uint16_t, int> m_valveOpenLimitBit;  // 开到位 DI 位
        std::map<uint16_t, int> m_valveCloseLimitByte;
        std::map<uint16_t, int> m_valveCloseLimitBit;
        std::map<uint16_t, int> m_valveAlarmByte;
        std::map<uint16_t, int> m_valveAlarmBit;

        SystemStatus m_systemStatus;
        std::vector<AlarmInfo> m_alarms;

        // 回调函数
        AlarmCallback m_alarmCallback;
        DataUpdateCallback m_dataUpdateCallback;

        // 数据采集线程相关
        bool m_running;
        std::shared_ptr<std::thread> m_collectionThread;
        mutable std::mutex m_dataMutex;

        // 私有方法
        void collectionThreadFunc(int intervalMs);
        bool readPressureSensors();
        bool readFlowMeters();
        bool readValves();
        bool readPumps();
        bool readTemperatureSensors();
        bool readRegulatingValves();
        bool readSystemStatus();
        void checkAlarms();
        void addAlarm(AlarmLevel level, const std::string &message, const std::string &source);

        // PLC数据地址定义（需要根据实际PLC程序调整）
        static constexpr int DB_PRESSURE_SENSORS = 1; // 压力传感器DB块
        static constexpr int DB_FLOW_METERS = 2;      // 流量计DB块
        static constexpr int DB_VALVES = 3;           // 电动阀DB块
        static constexpr int DB_PUMPS = 4;            // 变频泵DB块
        static constexpr int DB_SYSTEM = 5;           // 系统状态DB块
    };

} // namespace WaterTest

#endif // DEVICE_MANAGER_H
