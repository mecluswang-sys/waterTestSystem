/**
 * @file PipelineTopology.cpp
 * @brief 管路拓扑实现
 */

#include "PipelineTopology.h"
#include <sstream>
#include <algorithm>

namespace WaterTest
{

    PipelineTopology &PipelineTopology::getInstance()
    {
        static PipelineTopology instance;
        return instance;
    }

    PipelineTopology::PipelineTopology()
    {
    }

    PipelineTopology::~PipelineTopology()
    {
    }

    void PipelineTopology::initialize()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        initializeDeviceNodes();
        initializeConnections();
        initializePipeSegments();
        initializeFlowPaths();
    }

    void PipelineTopology::initializeDeviceNodes()
    {
        // ========== 变频泵 (2个) ==========
        for (int i = 1; i <= 2; ++i)
        {
            DeviceNode pump;
            pump.type = PhysicalDeviceType::FREQUENCY_PUMP;
            pump.deviceId = i;
            pump.name = "变频泵" + std::to_string(i);
            pump.x = 50.0f;
            pump.y = 100.0f + (i - 1) * 200.0f;
            m_deviceNodes[i * 1000] = pump; // ID: 1000, 2000
        }

        // ========== 电动阀 (11个) ==========
        for (int i = 1; i <= 11; ++i)
        {
            DeviceNode valve;
            valve.type = PhysicalDeviceType::ELECTRIC_VALVE;
            valve.deviceId = i;
            valve.name = "电动阀" + std::to_string(i);
            valve.x = 200.0f;
            valve.y = 50.0f + (i - 1) * 40.0f;
            m_deviceNodes[i * 100] = valve; // ID: 100-1100
        }

        // ========== 压力传感器 (11个) ==========
        for (int i = 1; i <= 11; ++i)
        {
            DeviceNode sensor;
            sensor.type = PhysicalDeviceType::PRESSURE_SENSOR;
            sensor.deviceId = i;
            sensor.name = "压力传感器" + std::to_string(i);
            sensor.x = 350.0f;
            sensor.y = 50.0f + (i - 1) * 40.0f;
            m_deviceNodes[i * 10] = sensor; // ID: 10-110
        }

        // ========== 流量计 (1个) ==========
        {
            DeviceNode meter;
            meter.type = PhysicalDeviceType::FLOW_METER;
            meter.deviceId = 1;
            meter.name = "流量计1";
            meter.x = 500.0f;
            meter.y = 200.0f;
            m_deviceNodes[1] = meter; // ID: 1
        }

        // ========== 电动调压阀 (2个) ==========
        for (int i = 1; i <= 2; ++i)
        {
            DeviceNode regValve;
            regValve.type = PhysicalDeviceType::REGULATING_VALVE;
            regValve.deviceId = i;
            regValve.name = "电动调压阀" + std::to_string(i);
            regValve.x = 650.0f;
            regValve.y = 150.0f + (i - 1) * 100.0f;
            m_deviceNodes[i * 10000] = regValve; // ID: 10000, 20000
        }
    }

    void PipelineTopology::initializeConnections()
    {
        // 典型的水流路径示例：
        // 变频泵1 → 电动阀1 → 压力传感器1 → 流量计1 → 电动调压阀1 → 测试点

        // 连接1: 泵1 → 阀1
        DeviceConnection conn1;
        conn1.fromDeviceId = 1;
        conn1.fromType = PhysicalDeviceType::FREQUENCY_PUMP;
        conn1.toDeviceId = 1;
        conn1.toType = PhysicalDeviceType::ELECTRIC_VALVE;
        conn1.pipeSegmentId = 101;
        conn1.isOpen = true;
        m_connections.push_back(conn1);

        // 连接2: 阀1 → 压力传感器1
        DeviceConnection conn2;
        conn2.fromDeviceId = 1;
        conn2.fromType = PhysicalDeviceType::ELECTRIC_VALVE;
        conn2.toDeviceId = 1;
        conn2.toType = PhysicalDeviceType::PRESSURE_SENSOR;
        conn2.pipeSegmentId = 102;
        conn2.isOpen = true;
        m_connections.push_back(conn2);

        // 连接3: 压力传感器1 → 流量计1
        DeviceConnection conn3;
        conn3.fromDeviceId = 1;
        conn3.fromType = PhysicalDeviceType::PRESSURE_SENSOR;
        conn3.toDeviceId = 1;
        conn3.toType = PhysicalDeviceType::FLOW_METER;
        conn3.pipeSegmentId = 103;
        conn3.isOpen = true;
        m_connections.push_back(conn3);

        // 连接4: 流量计1 → 电动调压阀1
        DeviceConnection conn4;
        conn4.fromDeviceId = 1;
        conn4.fromType = PhysicalDeviceType::FLOW_METER;
        conn4.toDeviceId = 1;
        conn4.toType = PhysicalDeviceType::REGULATING_VALVE;
        conn4.pipeSegmentId = 104;
        conn4.isOpen = true;
        m_connections.push_back(conn4);

        // 可以继续添加更多连接...
        // 例如：泵2的路径、其他阀门的分支路径等
    }

    void PipelineTopology::initializePipeSegments()
    {
        // 管段101: 泵1到阀1
        PipeSegment pipe101;
        pipe101.id = 101;
        pipe101.name = "主管路-段1";
        pipe101.diameter = 100.0f; // DN100
        pipe101.length = 5.0f;     // 5米
        m_pipeSegments[101] = pipe101;

        // 管段102: 阀1到压力传感器1
        PipeSegment pipe102;
        pipe102.id = 102;
        pipe102.name = "主管路-段2";
        pipe102.diameter = 100.0f;
        pipe102.length = 3.0f;
        m_pipeSegments[102] = pipe102;

        // 管段103: 压力传感器1到流量计1
        PipeSegment pipe103;
        pipe103.id = 103;
        pipe103.name = "主管路-段3";
        pipe103.diameter = 100.0f;
        pipe103.length = 8.0f;
        m_pipeSegments[103] = pipe103;

        // 管段104: 流量计1到电动调压阀1
        PipeSegment pipe104;
        pipe104.id = 104;
        pipe104.name = "主管路-段4";
        pipe104.diameter = 100.0f;
        pipe104.length = 6.0f;
        m_pipeSegments[104] = pipe104;
    }

    void PipelineTopology::initializeFlowPaths()
    {
        // 主流路径1：泵1的完整路径
        FlowPath path1;
        path1.pathName = "主测试回路1";

        DeviceNode pump1;
        pump1.type = PhysicalDeviceType::FREQUENCY_PUMP;
        pump1.deviceId = 1;
        pump1.name = "变频泵1";
        path1.deviceSequence.push_back(pump1);

        DeviceNode valve1;
        valve1.type = PhysicalDeviceType::ELECTRIC_VALVE;
        valve1.deviceId = 1;
        valve1.name = "电动阀1";
        path1.deviceSequence.push_back(valve1);

        DeviceNode sensor1;
        sensor1.type = PhysicalDeviceType::PRESSURE_SENSOR;
        sensor1.deviceId = 1;
        sensor1.name = "压力传感器1";
        path1.deviceSequence.push_back(sensor1);

        DeviceNode meter1;
        meter1.type = PhysicalDeviceType::FLOW_METER;
        meter1.deviceId = 1;
        meter1.name = "流量计1";
        path1.deviceSequence.push_back(meter1);

        DeviceNode regValve1;
        regValve1.type = PhysicalDeviceType::REGULATING_VALVE;
        regValve1.deviceId = 1;
        regValve1.name = "电动调压阀1";
        path1.deviceSequence.push_back(regValve1);

        path1.pipeSegments = {101, 102, 103, 104};

        m_flowPaths.push_back(path1);
    }

    std::vector<DeviceNode> PipelineTopology::getAllDeviceNodes() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<DeviceNode> nodes;
        for (const auto &pair : m_deviceNodes)
        {
            nodes.push_back(pair.second);
        }
        return nodes;
    }

    std::vector<DeviceConnection> PipelineTopology::getAllConnections() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connections;
    }

    PipeSegment PipelineTopology::getPipeSegment(uint16_t id) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_pipeSegments.find(id);
        if (it != m_pipeSegments.end())
        {
            return it->second;
        }
        return PipeSegment();
    }

    void PipelineTopology::updatePipeSegment(uint16_t id, float flowRate, float pressure)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_pipeSegments.find(id);
        if (it != m_pipeSegments.end())
        {
            it->second.flowRate = flowRate;
            it->second.pressure = pressure;
            it->second.isActive = (flowRate > 0.1f);
            it->second.timestamp = std::chrono::system_clock::now();
        }
    }

    FlowPath PipelineTopology::calculateFlowPath(uint16_t fromPumpId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 简化版：返回预定义的路径
        if (fromPumpId == 1 && !m_flowPaths.empty())
        {
            return m_flowPaths[0];
        }

        return FlowPath();
    }

    std::vector<DeviceNode> PipelineTopology::getDownstreamDevices(
        uint16_t deviceId, PhysicalDeviceType deviceType) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<DeviceNode> downstream;

        for (const auto &conn : m_connections)
        {
            if (conn.fromDeviceId == deviceId && conn.fromType == deviceType && conn.isOpen)
            {
                // 查找下游设备
                for (const auto &pair : m_deviceNodes)
                {
                    if (pair.second.deviceId == conn.toDeviceId &&
                        pair.second.type == conn.toType)
                    {
                        downstream.push_back(pair.second);
                        break;
                    }
                }
            }
        }

        return downstream;
    }

    std::vector<DeviceNode> PipelineTopology::getUpstreamDevices(
        uint16_t deviceId, PhysicalDeviceType deviceType) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<DeviceNode> upstream;

        for (const auto &conn : m_connections)
        {
            if (conn.toDeviceId == deviceId && conn.toType == deviceType && conn.isOpen)
            {
                // 查找上游设备
                for (const auto &pair : m_deviceNodes)
                {
                    if (pair.second.deviceId == conn.fromDeviceId &&
                        pair.second.type == conn.fromType)
                    {
                        upstream.push_back(pair.second);
                        break;
                    }
                }
            }
        }

        return upstream;
    }

    std::vector<FlowPath> PipelineTopology::traceWaterFlow()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_flowPaths;
    }

    std::vector<FlowPath> PipelineTopology::getActiveFlowPaths() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<FlowPath> activePaths;
        for (const auto &path : m_flowPaths)
        {
            if (path.isActive)
            {
                activePaths.push_back(path);
            }
        }
        return activePaths;
    }

    std::string PipelineTopology::generateFlowDiagram() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::stringstream ss;
        ss << "graph LR\n";
        ss << "    % 水压检测系统物理流程图\n\n";

        // 添加所有连接
        for (const auto &conn : m_connections)
        {
            std::string fromName, toName;

            // 查找起始设备名称
            for (const auto &pair : m_deviceNodes)
            {
                if (pair.second.deviceId == conn.fromDeviceId &&
                    pair.second.type == conn.fromType)
                {
                    fromName = pair.second.name;
                }
                if (pair.second.deviceId == conn.toDeviceId &&
                    pair.second.type == conn.toType)
                {
                    toName = pair.second.name;
                }
            }

            if (!fromName.empty() && !toName.empty())
            {
                // 获取管段信息
                auto pipeIt = m_pipeSegments.find(conn.pipeSegmentId);
                std::string pipeInfo;
                if (pipeIt != m_pipeSegments.end())
                {
                    const auto &pipe = pipeIt->second;
                    pipeInfo = pipe.name + "<br/>DN" + std::to_string((int)pipe.diameter) +
                               "<br/>" + std::to_string(pipe.flowRate) + "L/min";
                }

                ss << "    " << fromName << " -->|" << pipeInfo << "| " << toName << "\n";
            }
        }

        return ss.str();
    }

} // namespace WaterTest
