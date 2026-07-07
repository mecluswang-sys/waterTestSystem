# 水测试系统 × 论文内容集成方案

## 📊 整体规划

### 当前系统现状
- **框架**：Terminal-Station分布式架构 ✅
- **通信**：PLC S7协议 ✅
- **存储**：SQLite数据库 ✅
- **界面**：Qt 6个标签页 ✅
- **功能**：基础的数据采集和显示 ⚠️

### 论文贡献（东北大学硕士论文：水介质电磁阀测试系统的研究）
- **传感器标定**：完整的标定方法 📚
- **多工位管理**：支持多条测试管路 📚
- **测试流程**：自动化的4步测试模式 📚
- **报表生成**：标准化检测结果单 📚
- **采集卡配置**：详细的通道配置 📚

---

## 🎯 分阶段实施计划

### Phase 1: 传感器标定模块（1.5天）

**目标**：完整实现传感器标定算法

**改动文件**：
- `include/DeviceManager.h` - 添加标定参数结构
- `src/DeviceManager.cpp` - 实现标定算法
- `src/ConfigManager.cpp` - 添加标定参数配置
- `config/system.conf` - 新增标定参数配置项

**关键改进**：

```cpp
// 论文方法：标定公式
// 工程值 = (电压 - Offset) * Gain + Zero_Offset
// 
// 当前实现：只有简单scale
// 改进后：支持完整的4参数标定模型

struct SensorCalibration {
    float offset;        // 偏移量
    float gain;          // 增益
    float zero_offset;   // 零点偏移
    float min_value;     // 最小值
    float max_value;     // 最大值
    bool enabled;
};

std::map<int, SensorCalibration> m_sensorCalibrations;
```

**配置示例**：
```properties
# 传感器标定参数（阀前大压力传感器）
sensor.1.offset = 0.0
sensor.1.gain = 25.0
sensor.1.zero_offset = 0.0
sensor.1.min_value = 0.0
sensor.1.max_value = 25.0

# 标定过程记录
sensor.1.calib_date = 2026-07-06
sensor.1.calib_person = Engineer
```

---

### Phase 2: 电磁阀参数库（1天）

**目标**：建立完整的阀门参数管理系统

**新建文件**：
- `src/ValveParameterManager.cpp`
- `include/ValveParameterManager.h`
- 数据库表：valve_models, valve_parameters

**数据库表结构**：

```sql
-- 电磁阀型号库
CREATE TABLE IF NOT EXISTS valve_models (
    id INTEGER PRIMARY KEY,
    model_name TEXT UNIQUE,           -- 型号：如 DN25-2/2
    dn_size INTEGER,                  -- DN尺寸
    rated_pressure REAL,              -- 额定压力 (Mpa)
    rated_power REAL,                 -- 额定功率 (W)
    response_time REAL,               -- 响应时间 (ms)
    leakage_limit REAL,               -- 泄漏限度 (ml/min)
    material TEXT,                    -- 材质
    manufacturer TEXT,                -- 制造商
    test_date TEXT,
    test_person TEXT,
    status TEXT                       -- 可用/待验证
);

-- 电磁阀参数（每次测试）
CREATE TABLE IF NOT EXISTS valve_test_results (
    id INTEGER PRIMARY KEY,
    valve_model_id INTEGER,
    test_date TEXT,
    low_pressure_action REAL,         -- 低压动作压差
    high_pressure_action REAL,        -- 高压动作压差
    low_pressure_leakage REAL,        -- 低压泄漏
    high_pressure_leakage REAL,       -- 高压泄漏
    pressure_strength TEXT,           -- 耐压强度
    insulation_resistance REAL,       -- 绝缘电阻
    appearance_quality TEXT,          -- 外观质量
    test_result TEXT,                 -- 合格/不合格
    remarks TEXT,
    FOREIGN KEY(valve_model_id) REFERENCES valve_models(id)
);
```

**API接口**：

```cpp
class ValveParameterManager {
public:
    // 查询
    bool queryValveModel(const QString &modelName, ValveModel &model);
    QList<ValveModel> getAllModels();
    QList<TestResult> getTestHistory(int valve_id);
    
    // 添加/更新
    bool addValveModel(const ValveModel &model);
    bool updateTestResult(const TestResult &result);
    
    // 检测判定
    bool validateTestResult(int valve_id, TestResult &result);
};
```

---

### Phase 3: 完整的测试流程控制（2.5天）

**目标**：实现论文中的4种测试模式

**新建文件**：
- `src/TestCaseManager.cpp`
- `include/TestCaseManager.h`
- `include/TestState.h`

**测试流程状态机**：

```cpp
enum class TestMode {
    SELF_CHECK,        // 自检（气体）
    MANUAL_TEST,       // 手动测试（水介质）
    AIR_TEST,          // 气压测试
    DURABILITY_TEST    // 寿命测试
};

enum class TestPhase {
    IDLE,
    SELF_CHECK_START,
    PRESSURE_SETUP,
    WATER_OPEN_CLOSE,  // 水开闭测试
    MEDIA_SWITCH,      // 介质切换
    AIR_OPEN_CLOSE,    // 气开闭测试
    LEAKAGE_TEST,      // 泄漏测试
    PRESSURE_RELIEF,   // 泄压
    COMPLETED
};

class TestCaseManager {
public:
    // 测试控制
    bool startTest(TestMode mode);
    bool stopTest();
    TestPhase getCurrentPhase();
    
    // 数据检测
    bool isValveOpened();      // 压力传感器判断
    bool hasLeakage();         // 根据泄漏值判断
    int getLifecycleCount();   // 寿命计数
    
    // 报告生成
    bool generateReport(const QString &outputPath);
};
```

**测试流程图**：

```
启动系统
  ↓
气体自检 (100ms, 5次循环确认)
  ├─ 检查管路密封
  ├─ 检查各部件状态
  └─ 异常→报警
  ↓
选择测试模式 (UI按钮)
  ├─ 手动测试
  │  ├─ 夹装阀门
  │  ├─ 水压开闭 (N次)
  │  ├─ 合格检查
  │  ├─ 介质切换 (3通电磁阀)
  │  ├─ 气压开闭 (N次)
  │  ├─ 泄漏检测 (高/低压)
  │  └─ 取下阀门
  │
  └─ 寿命测试
     ├─ 自动循环 (水介质)
     ├─ 记录开关次数
     ├─ 实时显示计数
     └─ 一旦失败立即停止
  ↓
泄压 → 排空管路 → 完成
```

---

### Phase 4: 报表生成模块（1.5天）

**目标**：自动生成标准检测报告

**新建文件**：
- `src/ReportGenerator.cpp`
- `include/ReportGenerator.h`

**生成格式**：

```
═══════════════════════════════════════════════════════════
                    电磁阀检测结果一览表
═══════════════════════════════════════════════════════════

阀门型号：          DN25-2/2 (流量2L/min)
检测日期：          2026-07-06
检测人员：          张三
检测编号：          TEST-2026-001

───────────────────────────────────────────────────────────
测试项目              技术要求          测试结果      判定
───────────────────────────────────────────────────────────
低压动作实验         最小压差 ≤ 0.5    0.42 Mpa     ✓合格
高压动作实验         最大压差 ≤ 2.0    1.85 Mpa     ✓合格
密封性(水介质)       倍公称压力        无渗漏       ✓合格
低压泄漏             ≤ 5 ml/min        2.1          ✓合格
高压泄漏             ≤ 2 ml/min        0.8          ✓合格
耐压强度             8倍公称压力       无损坏       ✓合格
绝缘电阻             ≥ 10MΩ           >100 MΩ      ✓合格
打火现象             不应出现          无           ✓合格
外观检查             表面光洁          良好         ✓合格
───────────────────────────────────────────────────────────

总体判定：           ✓✓✓ 合格

制造单位：          鞍山XX公司
验收人：            李四
═══════════════════════════════════════════════════════════
```

**输出格式**：
- Excel (.xlsx)
- PDF (.pdf)  
- HTML (.html)
- CSV (.csv)

---

### Phase 5: 采集卡配置优化（1天）

**目标**：完善采集卡通道管理

**改动文件**：
- `src/DeviceManager.cpp` - 通道初始化
- `src/ConfigManager.cpp` - 配置管理
- `config/system.conf` - 新增采集卡配置

**采集卡配置项**：

```properties
# 采集卡类型和驱动
acquisition.card.model = NI-USB6009
acquisition.card.driver_path = /usr/lib/libdaqmx.so

# 模拟输入通道 (0-10V, 16位)
ain.channel.0 = 阀前大压力      # DB6对应
ain.channel.1 = 阀前小压力
ain.channel.2 = 阀后大压力
ain.channel.3 = 阀后小压力
ain.channel.4 = 流量计
ain.voltage_range = 0-10
ain.resolution = 16

# 开关量输入通道 (24V)
din.channel.0 = 合格标志
din.channel.1 = 不合格标志
din.channel.2 = 报警信号
din.channel.3 = 阀已夹好

# 采样率和滤波
acquisition.sample_rate = 1000    # Hz
acquisition.filter_size = 5       # 移动平均窗口
```

---

## 📈 工作量评估

| Phase | 任务 | 工作量 | 难度 |
|-------|------|--------|------|
| 1 | 传感器标定 | 1.5天 | 中 |
| 2 | 参数数据库 | 1天 | 低 |
| 3 | 测试流程控制 | 2.5天 | 高 |
| 4 | 报表生成 | 1.5天 | 中 |
| 5 | 采集卡配置 | 1天 | 低 |
| **合计** | | **7.5天** | |

---

## 🔗 集成点检查清单

- [ ] Phase 1: 传感器标定模块与DeviceManager集成
- [ ] Phase 2: 电磁阀参数数据库与GUI查询界面集成
- [ ] Phase 3: 测试流程管理与Station GUI集成
- [ ] Phase 4: 报表生成与导出功能集成
- [ ] Phase 5: 采集卡配置与系统启动集成

---

## 📝 优先级建议

**必做（P0）**：
1. Phase 1 - 传感器标定（直接影响测试准度）
2. Phase 3 - 测试流程控制（核心功能）
3. Phase 4 - 报表生成（交付物）

**应做（P1）**：
1. Phase 2 - 参数数据库（支撑测试管理）

**可做（P2）**：
1. Phase 5 - 采集卡配置优化（完整性）

---

**创建时间**: 2026-07-06  
**参考文献**: 东北大学硕士论文《水介质电磁阀测试系统的研究》- 曹洋  
**集成状态**: 规划中，待审批
