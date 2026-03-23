/**
 * @file DataPipelineManager.cpp
 * @brief 数据流水线管理器实现
 */

#include "DataPipelineManager.h"
#include <iostream>
#include <iomanip>

namespace WaterTest
{

    DataPipelineManager &DataPipelineManager::getInstance()
    {
        static DataPipelineManager instance;
        return instance;
    }

    DataPipelineManager::DataPipelineManager()
        : m_loggingEnabled(false), m_logFilename("logs/pipeline.log")
    {
    }

    DataPipelineManager::~DataPipelineManager()
    {
    }

    void DataPipelineManager::initialize()
    {
        createPressurePipeline();
        createFlowMeterPipeline();
        createValvePipeline();
        createPumpPipeline();
        createRegulatingValvePipeline();

        writeLog("DataPipelineManager initialized");
    }

    void DataPipelineManager::createPressurePipeline()
    {
        m_pressurePipeline = std::make_unique<Pipeline<PressureSensor>>("PressureSensor");

        // 1. 数据验证阶段
        auto validator = std::make_shared<ValidationProcessor<PressureSensor>>(
            [](const PressureSensor &sensor)
            {
                // 验证压力值是否在合理范围内
                if (sensor.pressure < 0 || sensor.pressure > 2000.0f) // 0-2000kPa
                {
                    return false;
                }
                // 验证设备状态
                if (sensor.status == DeviceStatus::OFFLINE)
                {
                    return false;
                }
                return true;
            });
        m_pressurePipeline->addProcessor(validator);

        // 2. 数据过滤阶段（移除异常值）
        auto filter = std::make_shared<FilterProcessor<PressureSensor>>(
            [](PressureSensor &sensor)
            {
                // 可以添加平滑滤波等算法
                // 例如：简单的限幅
                if (sensor.pressure > sensor.maxPressure)
                {
                    sensor.pressure = sensor.maxPressure;
                }
                if (sensor.pressure < sensor.minPressure)
                {
                    sensor.pressure = sensor.minPressure;
                }
            });
        m_pressurePipeline->addProcessor(filter);

        // 3. 数据转换阶段（单位转换等）
        auto converter = std::make_shared<ConversionProcessor<PressureSensor>>(
            [](PressureSensor &sensor)
            {
                // kPa 转换为其他单位的逻辑可以在这里
                // 当前保持原始单位
            });
        m_pressurePipeline->addProcessor(converter);

        // 4. 设置回调
        m_pressurePipeline->setCompletionCallback(
            [this](const DataPacket<PressureSensor> &packet)
            {
                if (m_loggingEnabled)
                {
                    writeLog("Pressure sensor " + std::to_string(packet.data.id) +
                             " processed: " + std::to_string(packet.data.pressure) + " kPa");
                }
            });

        m_pressurePipeline->setErrorCallback(
            [this](const DataPacket<PressureSensor> &packet, const std::string &error)
            {
                writeLog("ERROR: Pressure sensor " + std::to_string(packet.data.id) +
                         " - " + error);
            });
    }

    void DataPipelineManager::createFlowMeterPipeline()
    {
        m_flowMeterPipeline = std::make_unique<Pipeline<FlowMeter>>("FlowMeter");

        // 1. 验证
        auto validator = std::make_shared<ValidationProcessor<FlowMeter>>(
            [](const FlowMeter &meter)
            {
                if (meter.flowRate < 0 || meter.flowRate > 200.0f) // 0-200 L/min
                {
                    return false;
                }
                if (meter.status == DeviceStatus::OFFLINE)
                {
                    return false;
                }
                return true;
            });
        m_flowMeterPipeline->addProcessor(validator);

        // 2. 过滤
        auto filter = std::make_shared<FilterProcessor<FlowMeter>>(
            [](FlowMeter &meter)
            {
                // 流量平滑处理
                if (meter.flowRate < 0.1f) // 忽略小于0.1的噪声
                {
                    meter.flowRate = 0.0f;
                }
            });
        m_flowMeterPipeline->addProcessor(filter);

        // 3. 回调
        m_flowMeterPipeline->setCompletionCallback(
            [this](const DataPacket<FlowMeter> &packet)
            {
                if (m_loggingEnabled)
                {
                    writeLog("FlowMeter " + std::to_string(packet.data.id) +
                             " processed: " + std::to_string(packet.data.flowRate) + " L/min");
                }
            });
    }

    void DataPipelineManager::createValvePipeline()
    {
        m_valvePipeline = std::make_unique<Pipeline<ElectricValve>>("ElectricValve");

        // 验证阀门状态
        auto validator = std::make_shared<ValidationProcessor<ElectricValve>>(
            [](const ElectricValve &valve)
            {
                if (valve.openingDegree > 100)
                {
                    return false;
                }
                if (valve.deviceStatus == DeviceStatus::OFFLINE)
                {
                    return false;
                }
                return true;
            });
        m_valvePipeline->addProcessor(validator);

        m_valvePipeline->setCompletionCallback(
            [this](const DataPacket<ElectricValve> &packet)
            {
                if (m_loggingEnabled)
                {
                    writeLog("Valve " + std::to_string(packet.data.id) +
                             " processed: " + std::to_string(packet.data.openingDegree) + "%");
                }
            });
    }

    void DataPipelineManager::createPumpPipeline()
    {
        m_pumpPipeline = std::make_unique<Pipeline<FrequencyPump>>("FrequencyPump");

        // 验证泵运行参数
        auto validator = std::make_shared<ValidationProcessor<FrequencyPump>>(
            [](const FrequencyPump &pump)
            {
                if (pump.frequency < 0 || pump.frequency > 60.0f) // 0-60Hz
                {
                    return false;
                }
                if (pump.current < 0 || pump.current > 100.0f) // 0-100A
                {
                    return false;
                }
                if (pump.status == DeviceStatus::OFFLINE)
                {
                    return false;
                }
                return true;
            });
        m_pumpPipeline->addProcessor(validator);

        m_pumpPipeline->setCompletionCallback(
            [this](const DataPacket<FrequencyPump> &packet)
            {
                if (m_loggingEnabled)
                {
                    writeLog("Pump " + std::to_string(packet.data.id) +
                             " processed: " + std::to_string(packet.data.frequency) + " Hz");
                }
            });
    }

    void DataPipelineManager::createRegulatingValvePipeline()
    {
        m_regulatingValvePipeline = std::make_unique<Pipeline<RegulatingValve>>("RegulatingValve");

        // 验证调压阀参数
        auto validator = std::make_shared<ValidationProcessor<RegulatingValve>>(
            [](const RegulatingValve &valve)
            {
                if (valve.setPressure < 0 || valve.setPressure > 2000000.0f)
                {
                    return false;
                }
                if (valve.actualPressure < 0 || valve.actualPressure > 2000000.0f)
                {
                    return false;
                }
                if (valve.deviceStatus == DeviceStatus::OFFLINE)
                {
                    return false;
                }
                return true;
            });
        m_regulatingValvePipeline->addProcessor(validator);

        m_regulatingValvePipeline->setCompletionCallback(
            [this](const DataPacket<RegulatingValve> &packet)
            {
                if (m_loggingEnabled)
                {
                    writeLog("RegulatingValve " + std::to_string(packet.data.id) +
                             " processed: Set=" + std::to_string(packet.data.setPressure) +
                             " Pa, Actual=" + std::to_string(packet.data.actualPressure) + " Pa");
                }
            });
    }

    bool DataPipelineManager::processPressureSensor(const PressureSensor &sensor)
    {
        auto start = std::chrono::high_resolution_clock::now();

        DataPacket<PressureSensor> packet;
        packet.data = sensor;
        packet.source = "PressureSensor-" + std::to_string(sensor.id);

        bool success = m_pressurePipeline->execute(packet);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

        updateStatistics("PressureSensor", success, elapsed);

        return success;
    }

    bool DataPipelineManager::processFlowMeter(const FlowMeter &meter)
    {
        auto start = std::chrono::high_resolution_clock::now();

        DataPacket<FlowMeter> packet;
        packet.data = meter;
        packet.source = "FlowMeter-" + std::to_string(meter.id);

        bool success = m_flowMeterPipeline->execute(packet);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

        updateStatistics("FlowMeter", success, elapsed);

        return success;
    }

    bool DataPipelineManager::processValve(const ElectricValve &valve)
    {
        auto start = std::chrono::high_resolution_clock::now();

        DataPacket<ElectricValve> packet;
        packet.data = valve;
        packet.source = "ElectricValve-" + std::to_string(valve.id);

        bool success = m_valvePipeline->execute(packet);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

        updateStatistics("ElectricValve", success, elapsed);

        return success;
    }

    bool DataPipelineManager::processPump(const FrequencyPump &pump)
    {
        auto start = std::chrono::high_resolution_clock::now();

        DataPacket<FrequencyPump> packet;
        packet.data = pump;
        packet.source = "FrequencyPump-" + std::to_string(pump.id);

        bool success = m_pumpPipeline->execute(packet);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

        updateStatistics("FrequencyPump", success, elapsed);

        return success;
    }

    bool DataPipelineManager::processRegulatingValve(const RegulatingValve &valve)
    {
        auto start = std::chrono::high_resolution_clock::now();

        DataPacket<RegulatingValve> packet;
        packet.data = valve;
        packet.source = "RegulatingValve-" + std::to_string(valve.id);

        bool success = m_regulatingValvePipeline->execute(packet);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

        updateStatistics("RegulatingValve", success, elapsed);

        return success;
    }

    DataPipelineManager::Statistics DataPipelineManager::getStatistics(const std::string &pipelineName) const
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);

        auto it = m_statistics.find(pipelineName);
        if (it != m_statistics.end())
        {
            return it->second;
        }

        return Statistics{0, 0, 0, 0.0};
    }

    void DataPipelineManager::enableLogging(bool enable)
    {
        m_loggingEnabled = enable;
        if (enable)
        {
            writeLog("Pipeline logging enabled");
        }
    }

    void DataPipelineManager::setLogFile(const std::string &filename)
    {
        m_logFilename = filename;
    }

    void DataPipelineManager::clearStatistics()
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.clear();
        writeLog("Statistics cleared");
    }

    void DataPipelineManager::writeLog(const std::string &message)
    {
        if (!m_loggingEnabled)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_logMutex);

        try
        {
            std::ofstream logFile(m_logFilename, std::ios::app);
            if (logFile.is_open())
            {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);

                logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                        << " - " << message << std::endl;
            }
        }
        catch (...)
        {
            // 忽略日志写入错误
        }
    }

    void DataPipelineManager::updateStatistics(const std::string &pipelineName,
                                               bool success,
                                               double processingTime)
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);

        auto &stats = m_statistics[pipelineName];
        stats.totalProcessed++;

        if (success)
        {
            stats.successCount++;
        }
        else
        {
            stats.failureCount++;
        }

        // 更新平均处理时间（移动平均）
        if (stats.totalProcessed == 1)
        {
            stats.averageProcessingTime = processingTime;
        }
        else
        {
            stats.averageProcessingTime =
                (stats.averageProcessingTime * (stats.totalProcessed - 1) + processingTime) / stats.totalProcessed;
        }
    }

} // namespace WaterTest
