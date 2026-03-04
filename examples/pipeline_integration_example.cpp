/**
 * @file pipeline_integration_example.cpp
 * @brief 数据流水线集成示例
 * @description 展示如何在DeviceManager中集成数据流水线
 */

#include "DeviceManager.h"
#include "DataPipelineManager.h"

namespace WaterTest
{

    // ========================================
    // 示例1: 在应用启动时初始化流水线
    // ========================================
    void initializeApplication()
    {
        // 初始化流水线管理器
        auto &pipelineMgr = DataPipelineManager::getInstance();
        pipelineMgr.initialize();

        // 启用日志记录（可选）
        pipelineMgr.enableLogging(true);
        pipelineMgr.setLogFile("logs/data_pipeline.log");

        std::cout << "数据流水线已初始化" << std::endl;
    }

    // ========================================
    // 示例2: 修改DeviceManager集成流水线
    // ========================================

    // 原始方法（不使用流水线）
    bool DeviceManager::readPressureSensors_Original()
    {
        if (!m_plcClient)
            return false;

        std::lock_guard<std::mutex> lock(m_dataMutex);

        for (auto &pair : m_pressureSensors)
        {
            uint16_t id = pair.first;
            PressureSensor &sensor = pair.second;

            // 从PLC读取数据
            int offset = (id - 1) * 20;
            float pressure;
            int16_t status;

            if (m_plcClient->readReal(DB_PRESSURE_SENSORS, offset, pressure) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_PRESSURE_SENSORS, offset + 4, status) == S7PLCClient::Result::SUCCESS)
            {
                sensor.pressure = pressure;
                sensor.status = static_cast<DeviceStatus>(status);
                sensor.timestamp = std::chrono::system_clock::now();
            }
        }

        return true;
    }

    // 改进方法（使用流水线）
    bool DeviceManager::readPressureSensors_WithPipeline()
    {
        if (!m_plcClient)
            return false;

        std::lock_guard<std::mutex> lock(m_dataMutex);
        auto &pipelineMgr = DataPipelineManager::getInstance();

        for (auto &pair : m_pressureSensors)
        {
            uint16_t id = pair.first;
            PressureSensor &sensor = pair.second;

            // 从PLC读取原始数据
            int offset = (id - 1) * 20;
            float pressure;
            int16_t status;

            if (m_plcClient->readReal(DB_PRESSURE_SENSORS, offset, pressure) == S7PLCClient::Result::SUCCESS &&
                m_plcClient->readInt16(DB_PRESSURE_SENSORS, offset + 4, status) == S7PLCClient::Result::SUCCESS)
            {
                // 填充传感器数据
                sensor.pressure = pressure;
                sensor.status = static_cast<DeviceStatus>(status);
                sensor.timestamp = std::chrono::system_clock::now();

                // 通过流水线处理数据
                // 流水线会自动进行：验证、过滤、转换等操作
                if (!pipelineMgr.processPressureSensor(sensor))
                {
                    // 如果流水线处理失败（例如数据超出范围），标记为故障
                    sensor.status = DeviceStatus::FAULT;

                    // 记录日志
                    std::cerr << "压力传感器 " << id << " 数据处理失败: "
                              << pressure << " Pa" << std::endl;
                }
            }
        }

        return true;
    }

    // ========================================
    // 示例3: 查看流水线处理统计
    // ========================================
    void printPipelineStatistics()
    {
        auto &pipelineMgr = DataPipelineManager::getInstance();

        std::cout << "\n========== 数据流水线统计 ==========" << std::endl;

        // 压力传感器统计
        auto pressureStats = pipelineMgr.getStatistics("PressureSensor");
        std::cout << "\n【压力传感器】" << std::endl;
        std::cout << "  总处理数: " << pressureStats.totalProcessed << std::endl;
        std::cout << "  成功: " << pressureStats.successCount << std::endl;
        std::cout << "  失败: " << pressureStats.failureCount << std::endl;
        std::cout << "  成功率: " << std::fixed << std::setprecision(2)
                  << (pressureStats.totalProcessed > 0 ? 100.0 * pressureStats.successCount / pressureStats.totalProcessed : 0.0)
                  << "%" << std::endl;
        std::cout << "  平均处理时间: " << std::fixed << std::setprecision(3)
                  << pressureStats.averageProcessingTime << " ms" << std::endl;

        // 流量计统计
        auto flowStats = pipelineMgr.getStatistics("FlowMeter");
        std::cout << "\n【流量计】" << std::endl;
        std::cout << "  总处理数: " << flowStats.totalProcessed << std::endl;
        std::cout << "  成功: " << flowStats.successCount << std::endl;
        std::cout << "  失败: " << flowStats.failureCount << std::endl;
        std::cout << "  平均处理时间: " << std::fixed << std::setprecision(3)
                  << flowStats.averageProcessingTime << " ms" << std::endl;

        // 电动阀统计
        auto valveStats = pipelineMgr.getStatistics("ElectricValve");
        std::cout << "\n【电动阀】" << std::endl;
        std::cout << "  总处理数: " << valveStats.totalProcessed << std::endl;
        std::cout << "  成功: " << valveStats.successCount << std::endl;
        std::cout << "  失败: " << valveStats.failureCount << std::endl;

        // 变频泵统计
        auto pumpStats = pipelineMgr.getStatistics("FrequencyPump");
        std::cout << "\n【变频泵】" << std::endl;
        std::cout << "  总处理数: " << pumpStats.totalProcessed << std::endl;
        std::cout << "  成功: " << pumpStats.successCount << std::endl;
        std::cout << "  失败: " << pumpStats.failureCount << std::endl;

        // 电动调压阀统计
        auto regValveStats = pipelineMgr.getStatistics("RegulatingValve");
        std::cout << "\n【电动调压阀】" << std::endl;
        std::cout << "  总处理数: " << regValveStats.totalProcessed << std::endl;
        std::cout << "  成功: " << regValveStats.successCount << std::endl;
        std::cout << "  失败: " << regValveStats.failureCount << std::endl;

        std::cout << "\n===================================" << std::endl;
    }

    // ========================================
    // 示例4: 完整的main函数示例
    // ========================================
    int main_with_pipeline(int argc, char *argv[])
    {
        // 1. 初始化流水线
        initializeApplication();

        // 2. 创建设备管理器
        auto deviceMgr = std::make_shared<DeviceManager>();
        auto plcClient = std::make_shared<S7PLCClient>();

        // 3. 连接PLC
        if (plcClient->connect("192.168.0.1", 0, 1))
        {
            std::cout << "已连接到PLC" << std::endl;

            deviceMgr->initialize(plcClient);

            // 4. 启动数据采集（每1秒采集一次）
            deviceMgr->startDataCollection(1000);

            // 5. 运行一段时间
            std::this_thread::sleep_for(std::chrono::seconds(10));

            // 6. 查看统计信息
            printPipelineStatistics();

            // 7. 停止采集
            deviceMgr->stopDataCollection();
            plcClient->disconnect();
        }
        else
        {
            std::cerr << "无法连接到PLC" << std::endl;
            return 1;
        }

        return 0;
    }

    // ========================================
    // 示例5: 在GUI中显示流水线状态
    // ========================================
    void updatePipelineStatusInGUI()
    {
        auto &pipelineMgr = DataPipelineManager::getInstance();

        // 获取各类设备的统计信息
        auto pressureStats = pipelineMgr.getStatistics("PressureSensor");
        auto flowStats = pipelineMgr.getStatistics("FlowMeter");
        auto valveStats = pipelineMgr.getStatistics("ElectricValve");
        auto pumpStats = pipelineMgr.getStatistics("FrequencyPump");

        // 更新GUI状态栏
        QString statusText = QString("数据处理: 压力传感器(%1/%2) 流量计(%3/%4) 阀门(%5/%6) 泵(%7/%8)")
                                 .arg(pressureStats.successCount)
                                 .arg(pressureStats.totalProcessed)
                                 .arg(flowStats.successCount)
                                 .arg(flowStats.totalProcessed)
                                 .arg(valveStats.successCount)
                                 .arg(valveStats.totalProcessed)
                                 .arg(pumpStats.successCount)
                                 .arg(pumpStats.totalProcessed);

        // statusBar()->showMessage(statusText);
    }

    // ========================================
    // 示例6: 数据流转完整示例
    // ========================================
    void completeDataFlowExample()
    {
        std::cout << "\n========== 完整数据流转示例 ==========" << std::endl;

        // 模拟一个压力传感器数据的完整流转过程
        PressureSensor sensor;
        sensor.id = 1;
        sensor.pressure = 850000.0f; // 850 kPa
        sensor.maxPressure = 1000000.0f;
        sensor.minPressure = 0.0f;
        sensor.status = DeviceStatus::ONLINE;
        sensor.timestamp = std::chrono::system_clock::now();

        std::cout << "\n1. 原始数据 (RAW_DATA):" << std::endl;
        std::cout << "   传感器ID: " << sensor.id << std::endl;
        std::cout << "   压力值: " << sensor.pressure << " Pa" << std::endl;
        std::cout << "   状态: ONLINE" << std::endl;

        std::cout << "\n2. 进入流水线处理..." << std::endl;

        auto &pipelineMgr = DataPipelineManager::getInstance();
        bool success = pipelineMgr.processPressureSensor(sensor);

        if (success)
        {
            std::cout << "\n3. 流水线处理成功!" << std::endl;
            std::cout << "   ✓ 验证阶段: 数据范围有效" << std::endl;
            std::cout << "   ✓ 过滤阶段: 无异常值" << std::endl;
            std::cout << "   ✓ 转换阶段: 单位转换完成" << std::endl;
            std::cout << "   ✓ 通知阶段: GUI更新触发" << std::endl;

            std::cout << "\n4. 最终数据:" << std::endl;
            std::cout << "   传感器ID: " << sensor.id << std::endl;
            std::cout << "   压力值: " << sensor.pressure << " Pa" << std::endl;
            std::cout << "   状态: ONLINE" << std::endl;
        }
        else
        {
            std::cout << "\n3. 流水线处理失败!" << std::endl;
            std::cout << "   ✗ 数据验证未通过或处理出错" << std::endl;
        }

        std::cout << "\n===================================" << std::endl;
    }

} // namespace WaterTest
