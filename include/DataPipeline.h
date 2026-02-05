/**
 * @file DataPipeline.h
 * @brief 数据处理流水线
 * @description 实现数据从采集到显示的流水线处理架构
 */

#ifndef DATA_PIPELINE_H
#define DATA_PIPELINE_H

#include "DeviceTypes.h"
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace WaterTest
{

    // 数据处理阶段枚举
    enum class PipelineStage
    {
        RAW_DATA = 0,     // 原始数据采集
        VALIDATION = 1,   // 数据验证
        FILTERING = 2,    // 数据过滤
        CONVERSION = 3,   // 数据转换
        AGGREGATION = 4,  // 数据聚合
        STORAGE = 5,      // 数据存储
        NOTIFICATION = 6, // 通知发送
        VISUALIZATION = 7 // 可视化显示
    };

    // 数据包装器（携带元数据）
    template <typename T>
    struct DataPacket
    {
        T data;                                          // 实际数据
        PipelineStage currentStage;                      // 当前处理阶段
        std::chrono::system_clock::time_point timestamp; // 时间戳
        std::string source;                              // 数据源
        bool isValid;                                    // 是否有效
        std::vector<std::string> processingLog;          // 处理日志

        DataPacket() : currentStage(PipelineStage::RAW_DATA),
                       timestamp(std::chrono::system_clock::now()),
                       isValid(true) {}

        void addLog(const std::string &message)
        {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count();
            processingLog.push_back(std::to_string(ms) + ": " + message);
        }
    };

    // 处理器接口
    template <typename T>
    class DataProcessor
    {
    public:
        virtual ~DataProcessor() = default;
        virtual bool process(DataPacket<T> &packet) = 0;
        virtual std::string getName() const = 0;
        virtual PipelineStage getStage() const = 0;
    };

    // 流水线管理器
    template <typename T>
    class Pipeline
    {
    public:
        using ProcessorPtr = std::shared_ptr<DataProcessor<T>>;
        using CompletionCallback = std::function<void(const DataPacket<T> &)>;
        using ErrorCallback = std::function<void(const DataPacket<T> &, const std::string &)>;

        Pipeline(const std::string &name) : m_name(name) {}

        // 添加处理器
        void addProcessor(ProcessorPtr processor)
        {
            m_processors.push_back(processor);
        }

        // 执行流水线
        bool execute(DataPacket<T> &packet)
        {
            packet.addLog("Pipeline [" + m_name + "] started");

            for (auto &processor : m_processors)
            {
                packet.currentStage = processor->getStage();
                packet.addLog("Stage: " + processor->getName());

                try
                {
                    if (!processor->process(packet))
                    {
                        packet.isValid = false;
                        packet.addLog("ERROR: " + processor->getName() + " failed");

                        if (m_errorCallback)
                        {
                            m_errorCallback(packet, processor->getName() + " failed");
                        }
                        return false;
                    }

                    packet.addLog("Stage: " + processor->getName() + " completed");
                }
                catch (const std::exception &e)
                {
                    packet.isValid = false;
                    packet.addLog("EXCEPTION: " + std::string(e.what()));

                    if (m_errorCallback)
                    {
                        m_errorCallback(packet, e.what());
                    }
                    return false;
                }
            }

            packet.addLog("Pipeline [" + m_name + "] completed successfully");

            if (m_completionCallback)
            {
                m_completionCallback(packet);
            }

            return true;
        }

        // 设置完成回调
        void setCompletionCallback(CompletionCallback callback)
        {
            m_completionCallback = callback;
        }

        // 设置错误回调
        void setErrorCallback(ErrorCallback callback)
        {
            m_errorCallback = callback;
        }

        // 获取流水线名称
        std::string getName() const { return m_name; }

        // 获取处理器数量
        size_t getProcessorCount() const { return m_processors.size(); }

    private:
        std::string m_name;
        std::vector<ProcessorPtr> m_processors;
        CompletionCallback m_completionCallback;
        ErrorCallback m_errorCallback;
    };

    // ======== 具体处理器实现 ========

    // 数据验证处理器
    template <typename T>
    class ValidationProcessor : public DataProcessor<T>
    {
    public:
        using ValidatorFunc = std::function<bool(const T &)>;

        ValidationProcessor(ValidatorFunc validator)
            : m_validator(validator) {}

        bool process(DataPacket<T> &packet) override
        {
            if (!m_validator(packet.data))
            {
                packet.addLog("Validation failed");
                return false;
            }
            return true;
        }

        std::string getName() const override { return "Validation"; }
        PipelineStage getStage() const override { return PipelineStage::VALIDATION; }

    private:
        ValidatorFunc m_validator;
    };

    // 数据过滤处理器
    template <typename T>
    class FilterProcessor : public DataProcessor<T>
    {
    public:
        using FilterFunc = std::function<void(T &)>;

        FilterProcessor(FilterFunc filter)
            : m_filter(filter) {}

        bool process(DataPacket<T> &packet) override
        {
            m_filter(packet.data);
            return true;
        }

        std::string getName() const override { return "Filter"; }
        PipelineStage getStage() const override { return PipelineStage::FILTERING; }

    private:
        FilterFunc m_filter;
    };

    // 数据转换处理器
    template <typename T>
    class ConversionProcessor : public DataProcessor<T>
    {
    public:
        using ConversionFunc = std::function<void(T &)>;

        ConversionProcessor(ConversionFunc converter)
            : m_converter(converter) {}

        bool process(DataPacket<T> &packet) override
        {
            m_converter(packet.data);
            return true;
        }

        std::string getName() const override { return "Conversion"; }
        PipelineStage getStage() const override { return PipelineStage::CONVERSION; }

    private:
        ConversionFunc m_converter;
    };

    // 数据存储处理器
    template <typename T>
    class StorageProcessor : public DataProcessor<T>
    {
    public:
        using StorageFunc = std::function<bool(const T &)>;

        StorageProcessor(StorageFunc storage)
            : m_storage(storage) {}

        bool process(DataPacket<T> &packet) override
        {
            return m_storage(packet.data);
        }

        std::string getName() const override { return "Storage"; }
        PipelineStage getStage() const override { return PipelineStage::STORAGE; }

    private:
        StorageFunc m_storage;
    };

    // 通知处理器
    template <typename T>
    class NotificationProcessor : public DataProcessor<T>
    {
    public:
        using NotificationFunc = std::function<void(const T &)>;

        NotificationProcessor(NotificationFunc notifier)
            : m_notifier(notifier) {}

        bool process(DataPacket<T> &packet) override
        {
            m_notifier(packet.data);
            return true;
        }

        std::string getName() const override { return "Notification"; }
        PipelineStage getStage() const override { return PipelineStage::NOTIFICATION; }

    private:
        NotificationFunc m_notifier;
    };

} // namespace WaterTest

#endif // DATA_PIPELINE_H
