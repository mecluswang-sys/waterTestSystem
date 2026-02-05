/**
 * @file DataPipelineManager.h
 * @brief 数据流水线管理器
 * @description 管理所有设备类型的数据处理流水线
 */

#ifndef DATA_PIPELINE_MANAGER_H
#define DATA_PIPELINE_MANAGER_H

#include "DataPipeline.h"
#include "DeviceTypes.h"
#include <memory>
#include <map>
#include <mutex>
#include <fstream>

namespace WaterTest
{

    class DataPipelineManager
    {
    public:
        static DataPipelineManager &getInstance();

        // 初始化流水线
        void initialize();

        // 处理不同类型的设备数据
        bool processPressureSensor(const PressureSensor &sensor);
        bool processFlowMeter(const FlowMeter &meter);
        bool processValve(const ElectricValve &valve);
        bool processPump(const FrequencyPump &pump);
        bool processRegulatingValve(const RegulatingValve &valve);

        // 获取处理统计
        struct Statistics
        {
            uint64_t totalProcessed;
            uint64_t successCount;
            uint64_t failureCount;
            double averageProcessingTime; // 毫秒
        };

        Statistics getStatistics(const std::string &pipelineName) const;

        // 启用/禁用流水线日志
        void enableLogging(bool enable);
        void setLogFile(const std::string &filename);

        // 清空统计
        void clearStatistics();

    private:
        DataPipelineManager();
        ~DataPipelineManager();

        // 创建各类设备的流水线
        void createPressurePipeline();
        void createFlowMeterPipeline();
        void createValvePipeline();
        void createPumpPipeline();
        void createRegulatingValvePipeline();

        // 流水线实例
        std::unique_ptr<Pipeline<PressureSensor>> m_pressurePipeline;
        std::unique_ptr<Pipeline<FlowMeter>> m_flowMeterPipeline;
        std::unique_ptr<Pipeline<ElectricValve>> m_valvePipeline;
        std::unique_ptr<Pipeline<FrequencyPump>> m_pumpPipeline;
        std::unique_ptr<Pipeline<RegulatingValve>> m_regulatingValvePipeline;

        // 统计数据
        mutable std::mutex m_statsMutex;
        std::map<std::string, Statistics> m_statistics;

        // 日志相关
        bool m_loggingEnabled;
        std::string m_logFilename;
        mutable std::mutex m_logMutex;

        void writeLog(const std::string &message);
        void updateStatistics(const std::string &pipelineName, bool success, double processingTime);
    };

} // namespace WaterTest

#endif // DATA_PIPELINE_MANAGER_H
