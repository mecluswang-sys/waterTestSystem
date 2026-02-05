/**
 * @file PipelineTopology.h
 * @brief 水力管路拓扑结构
 * @description 定义水介质在物理管路中的流动路径和连接关系
 */

#ifndef PIPELINE_TOPOLOGY_H
#define PIPELINE_TOPOLOGY_H

#include "DeviceTypes.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>

namespace WaterTest
{

    // 设备类型枚举
    enum class PhysicalDeviceType
    {
        FREQUENCY_PUMP,   // 变频泵
        ELECTRIC_VALVE,   // 电动阀
        PRESSURE_SENSOR,  // 压力传感器
        FLOW_METER,       // 流量计
        REGULATING_VALVE, // 电动调压阀
        PIPE_SEGMENT,     // 管段
        TEST_POINT        // 测试点
    };

    // 管段信息
    struct PipeSegment
    {
        uint16_t id;      // 管段ID
        std::string name; // 管段名称
        float length;     // 管长(m)
        float diameter;   // 管径(mm) DN30/50/100/150
        float flowRate;   // 当前流量(L/min)
        float pressure;   // 当前压力(Pa)
        bool isActive;    // 是否有水流
        std::chrono::system_clock::time_point timestamp;

        PipeSegment() : id(0), length(0.0f), diameter(0.0f),
                        flowRate(0.0f), pressure(0.0f), isActive(false) {}
    };

    // 设备节点
    struct DeviceNode
    {
        PhysicalDeviceType type; // 设备类型
        uint16_t deviceId;       // 设备ID
        std::string name;        // 设备名称
        float x;                 // X坐标（用于可视化）
        float y;                 // Y坐标（用于可视化）
        bool isActive;           // 是否工作中

        DeviceNode() : type(PhysicalDeviceType::PIPE_SEGMENT),
                       deviceId(0), x(0.0f), y(0.0f), isActive(false) {}
    };

    // 设备连接
    struct DeviceConnection
    {
        uint16_t fromDeviceId;       // 上游设备ID
        PhysicalDeviceType fromType; // 上游设备类型
        uint16_t toDeviceId;         // 下游设备ID
        PhysicalDeviceType toType;   // 下游设备类型
        uint16_t pipeSegmentId;      // 连接管段ID
        bool isOpen;                 // 连接是否打开

        DeviceConnection() : fromDeviceId(0), toDeviceId(0),
                             pipeSegmentId(0), isOpen(false) {}
    };

    // 水流路径
    struct FlowPath
    {
        std::string pathName;                   // 路径名称
        std::vector<DeviceNode> deviceSequence; // 设备序列
        std::vector<uint16_t> pipeSegments;     // 经过的管段
        bool isActive;                          // 路径是否激活
        float totalFlowRate;                    // 总流量
        float inletPressure;                    // 入口压力
        float outletPressure;                   // 出口压力

        FlowPath() : isActive(false), totalFlowRate(0.0f),
                     inletPressure(0.0f), outletPressure(0.0f) {}
    };

    /**
     * @brief 管路拓扑管理器
     * @description 管理整个水力系统的物理连接和流动关系
     */
    class PipelineTopology
    {
    public:
        static PipelineTopology &getInstance();

        /**
         * @brief 初始化管路拓扑
         */
        void initialize();

        /**
         * @brief 获取所有设备节点
         */
        std::vector<DeviceNode> getAllDeviceNodes() const;

        /**
         * @brief 获取所有设备连接
         */
        std::vector<DeviceConnection> getAllConnections() const;

        /**
         * @brief 获取管段信息
         */
        PipeSegment getPipeSegment(uint16_t id) const;

        /**
         * @brief 更新管段状态
         */
        void updatePipeSegment(uint16_t id, float flowRate, float pressure);

        /**
         * @brief 计算水流路径
         * @param fromPumpId 起始泵ID
         * @return 水流经过的路径
         */
        FlowPath calculateFlowPath(uint16_t fromPumpId);

        /**
         * @brief 获取设备的下游设备
         * @param deviceId 设备ID
         * @param deviceType 设备类型
         * @return 下游设备列表
         */
        std::vector<DeviceNode> getDownstreamDevices(uint16_t deviceId,
                                                     PhysicalDeviceType deviceType) const;

        /**
         * @brief 获取设备的上游设备
         */
        std::vector<DeviceNode> getUpstreamDevices(uint16_t deviceId,
                                                   PhysicalDeviceType deviceType) const;

        /**
         * @brief 追踪水流从泵到测试点的完整路径
         */
        std::vector<FlowPath> traceWaterFlow();

        /**
         * @brief 获取活动的流动路径
         */
        std::vector<FlowPath> getActiveFlowPaths() const;

        /**
         * @brief 生成流程图描述（Mermaid格式）
         */
        std::string generateFlowDiagram() const;

    private:
        PipelineTopology();
        ~PipelineTopology();

        // 初始化各部分拓扑
        void initializeDeviceNodes();
        void initializeConnections();
        void initializePipeSegments();
        void initializeFlowPaths();

        // 内部数据
        std::map<uint16_t, DeviceNode> m_deviceNodes;
        std::vector<DeviceConnection> m_connections;
        std::map<uint16_t, PipeSegment> m_pipeSegments;
        std::vector<FlowPath> m_flowPaths;

        mutable std::mutex m_mutex;
    };

} // namespace WaterTest

#endif // PIPELINE_TOPOLOGY_H
