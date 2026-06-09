# 水压检测系统 - 数据流水线架构

## 流水线概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                      数据流水线处理流程                               │
└─────────────────────────────────────────────────────────────────────┘

PLC设备 → Snap7读取 → DeviceManager → DataPipeline → 处理结果
   ↓          ↓            ↓              ↓            ↓
 传感器    原始数据     数据采集      流水线处理    GUI显示
 阀门泵    网络通信     集中管理      分阶段验证    数据存储
```

## 流水线处理阶段

### 完整数据流转路径

```
┌────────────┐
│  1. 采集   │  PLC → Snap7 → DeviceManager::readXXX()
│ RAW_DATA   │  获取原始设备数据
└─────┬──────┘
      │
      ▼
┌────────────┐
│  2. 验证   │  ValidationProcessor
│ VALIDATION │  • 数值范围检查
└─────┬──────┘  • 状态有效性验证
      │         • 离线设备过滤
      ▼
┌────────────┐
│  3. 过滤   │  FilterProcessor
│ FILTERING  │  • 异常值处理
└─────┬──────┘  • 噪声过滤
      │         • 数据平滑
      ▼
┌────────────┐
│  4. 转换   │  ConversionProcessor
│ CONVERSION │  • 单位转换
└─────┬──────┘  • 格式标准化
      │         • 数据归一化
      ▼
┌────────────┐
│  5. 聚合   │  AggregationProcessor (可选)
│ AGGREGATION│  • 多传感器融合
└─────┬──────┘  • 统计计算
      │
      ▼
┌────────────┐
│  6. 存储   │  StorageProcessor (可选)
│  STORAGE   │  • 数据库写入
└─────┬──────┘  • 历史记录
      │
      ▼
┌────────────┐
│  7. 通知   │  NotificationProcessor
│NOTIFICATION│  • GUI回调
└─────┬──────┘  • 事件触发
      │         • 报警推送
      ▼
┌────────────┐
│  8. 显示   │  GUI更新
│VISUALIZATION│  • 表格刷新
└────────────┘  • 图表绘制
```

## 设备类型流水线

### 压力传感器流水线

```
PressureSensor → 验证(0-2000kPa) → 限幅过滤 → 结果输出
                 ✓ 在线状态
                 ✓ 合理范围
```

### 流量计流水线

```
FlowMeter → 验证(0-200L/min) → 噪声过滤(<0.1归零) → 结果输出
            ✓ 在线状态
            ✓ 正数检查
```

### 电动阀流水线

```
ElectricValve → 验证(开度0-100%) → 状态同步 → 结果输出
                ✓ 在线状态
                ✓ 开度合法性
```

### 变频泵流水线

```
FrequencyPump → 验证(频率0-60Hz) → 电流检查(0-100A) → 结果输出
                ✓ 在线状态
                ✓ 参数范围
```

### 电动调压阀流水线

```
RegulatingValve → 验证(压力0-2000kPa) → 偏差检测 → 结果输出
                  ✓ 设定值检查
                  ✓ 实际值检查
```

## 单位兼容说明（显示/导出 vs 内部存储）

- UI 显示与 CSV 导出压力单位统一为 `kPa`。
- 设备采集与数据库内部字段统一使用 `kPa`，避免二次换算误差。
- 若新增接口字段，请在接口文档中明确字段单位，避免跨端歧义。

## 使用示例

### 1. 初始化流水线

```cpp
// 在应用启动时初始化
DataPipelineManager::getInstance().initialize();
DataPipelineManager::getInstance().enableLogging(true);
DataPipelineManager::getInstance().setLogFile("logs/pipeline.log");
```

### 2. 在DeviceManager中集成流水线

```cpp
bool DeviceManager::readPressureSensors()
{
    // ... 从PLC读取数据 ...
    
    for (auto &pair : m_pressureSensors)
    {
        PressureSensor &sensor = pair.second;
        
        // 原始数据读取
        // ... PLC通信代码 ...
        
        // 通过流水线处理
        if (DataPipelineManager::getInstance().processPressureSensor(sensor))
        {
            // 处理成功，数据已验证和过滤
        }
        else
        {
            // 处理失败，数据可能无效
            sensor.status = DeviceStatus::FAULT;
        }
    }
    
    return true;
}
```

### 3. 查看流水线统计

```cpp
auto stats = DataPipelineManager::getInstance().getStatistics("PressureSensor");
std::cout << "总处理: " << stats.totalProcessed << std::endl;
std::cout << "成功: " << stats.successCount << std::endl;
std::cout << "失败: " << stats.failureCount << std::endl;
std::cout << "平均耗时: " << stats.averageProcessingTime << " ms" << std::endl;
```

## 流水线优势

### 1. **清晰的数据流向**

- 每个阶段职责明确
- 易于理解和维护
- 便于追踪数据处理过程

### 2. **灵活的处理链**

- 可动态添加/删除处理器
- 支持自定义处理逻辑
- 易于扩展新功能

### 3. **完整的日志追踪**

- 记录每个处理阶段
- 时间戳精确到毫秒
- 错误信息详细记录

### 4. **性能监控**

- 实时统计处理数量
- 成功/失败率统计
- 平均处理时间计算

### 5. **数据质量保证**

- 多层验证机制
- 异常值自动过滤
- 状态一致性检查

## 日志输出示例

```
2025-12-23 10:15:30 - DataPipelineManager initialized
2025-12-23 10:15:31 - Pressure sensor 1 processed: 850000 Pa
2025-12-23 10:15:31 - Pressure sensor 2 processed: 920000 Pa
2025-12-23 10:15:31 - ERROR: Pressure sensor 3 - Validation failed
2025-12-23 10:15:31 - FlowMeter 1 processed: 45.5 L/min
2025-12-23 10:15:31 - Valve 1 processed: 75%
2025-12-23 10:15:31 - Pump 1 processed: 50.0 Hz
```

## 扩展建议

### 1. 添加数据库存储处理器

```cpp
auto storage = std::make_shared<StorageProcessor<PressureSensor>>(
    [](const PressureSensor& sensor) {
        // 写入数据库
        DatabaseManager::getInstance().savePressureData(sensor);
        return true;
    });
m_pressurePipeline->addProcessor(storage);
```

### 2. 添加报警处理器

```cpp
auto alarm = std::make_shared<NotificationProcessor<PressureSensor>>(
    [](const PressureSensor& sensor) {
        if (sensor.pressure > sensor.maxPressure * 0.9)
        {
            AlarmManager::getInstance().raiseAlarm("压力过高");
        }
    });
m_pressurePipeline->addProcessor(alarm);
```

### 3. 添加数据聚合处理器

```cpp
// 计算平均值、趋势分析等
auto aggregator = std::make_shared<AggregationProcessor<PressureSensor>>(...);
```

## 性能考虑

- 流水线处理通常耗时 < 1ms
- 支持并发处理多个设备
- 线程安全的统计数据
- 异步日志写入避免阻塞
