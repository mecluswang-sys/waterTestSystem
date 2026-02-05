# 水压检测系统 - 物理管路流程图

## 系统概览

这是水介质在物理管路中的实际流动路径，展示了从变频泵驱动到测试点的完整过程。

## 主测试回路流程

```mermaid
graph LR
    %% 定义样式
    classDef pumpStyle fill:#e1f5ff,stroke:#01579b,stroke-width:3px
    classDef valveStyle fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    classDef sensorStyle fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef meterStyle fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
    classDef regValveStyle fill:#ffe0b2,stroke:#e65100,stroke-width:2px

    %% 主回路1
    P1[变频泵1<br/>50Hz<br/>45A]:::pumpStyle
    V1[电动阀1<br/>开度:100%]:::valveStyle
    PS1[压力传感器1<br/>0.85MPa]:::sensorStyle
    FM1[流量计1<br/>85L/min<br/>25°C]:::meterStyle
    RV1[电动调压阀1<br/>设定:0.8MPa]:::regValveStyle
    TP1[测试点1]

    P1 -->|DN100<br/>5m<br/>0.9MPa| V1
    V1 -->|DN100<br/>3m<br/>0.87MPa| PS1
    PS1 -->|DN100<br/>8m<br/>0.85MPa| FM1
    FM1 -->|DN100<br/>6m<br/>0.82MPa| RV1
    RV1 -->|DN50<br/>4m<br/>0.8MPa| TP1

    %% 标注水流方向
    P1 -.水流方向.-> TP1
```

## 完整系统拓扑

```mermaid
graph TB
    %% 水源和泵站
    WS[水源水箱]
    P1[变频泵1]
    P2[变频泵2]

    %% 主干管路
    V1[电动阀1-主进]
    V2[电动阀2-分支1]
    V3[电动阀3-分支2]
    V4[电动阀4-DN30]
    V5[电动阀5-DN50]
    V6[电动阀6-DN100]
    V7[电动阀7-DN150]
    V8[电动阀8-回流]
    V9[电动阀9-排空]
    V10[电动阀10-旁路]
    V11[电动阀11-泄压]

    %% 测量设备
    PS1[压力传感器1]
    PS2[压力传感器2]
    PS3[压力传感器3]
    PS4[压力传感器4]
    PS5[压力传感器5]
    PS6[压力传感器6]
    PS7[压力传感器7]
    PS8[压力传感器8]
    PS9[压力传感器9]
    PS10[压力传感器10]
    PS11[压力传感器11]

    FM[流量计1]

    %% 调压设备
    RV1[电动调压阀1-高压]
    RV2[电动调压阀2-低压]

    %% 测试点
    T1[DN30测试点]
    T2[DN50测试点]
    T3[DN100测试点]
    T4[DN150测试点]

    %% 回流
    RT[回流水箱]

    %% 连接关系
    WS --> P1
    WS --> P2
    P1 --> V1
    P2 --> V1

    V1 --> PS1
    PS1 --> V2
    PS1 --> V3

    V2 --> PS2
    PS2 --> V4
    V4 --> PS4
    PS4 --> T1

    V3 --> PS3
    PS3 --> V5
    V5 --> PS5
    PS5 --> T2

    V1 --> PS6
    PS6 --> V6
    V6 --> PS7
    PS7 --> FM
    FM --> PS8
    PS8 --> RV1
    RV1 --> T3

    V1 --> PS9
    PS9 --> V7
    V7 --> PS10
    PS10 --> RV2
    RV2 --> T4

    T1 --> V8
    T2 --> V8
    T3 --> V8
    T4 --> V8
    V8 --> RT

    PS11 --> V9
    V9 --> RT
    V10 --> RT
    V11 --> RT

    %% 样式
    classDef pumpClass fill:#e1f5ff,stroke:#01579b,stroke-width:3px
    classDef valveClass fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    classDef sensorClass fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef meterClass fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
    classDef regValveClass fill:#ffe0b2,stroke:#e65100,stroke-width:2px
    classDef testClass fill:#ffebee,stroke:#b71c1c,stroke-width:3px

    class P1,P2 pumpClass
    class V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11 valveClass
    class PS1,PS2,PS3,PS4,PS5,PS6,PS7,PS8,PS9,PS10,PS11 sensorClass
    class FM meterClass
    class RV1,RV2 regValveClass
    class T1,T2,T3,T4 testClass
```

## 设备分布统计

| 设备类型 | 数量 | 作用 |
|---------|------|------|
| 变频泵 | 2 | 提供动力和流量 |
| 电动阀 | 11 | 控制水流方向和通断 |
| 压力传感器 | 11 | 监测各点压力 |
| 流量计 | 1 | 测量流量和温度 |
| 电动调压阀 | 2 | 精确调节压力 |

## 典型测试流程

### DN100管路测试流程

```
1. 启动阶段
   变频泵1启动 → 30Hz → 逐渐升至50Hz
   ↓
2. 管路充水
   电动阀1打开(主进) → 电动阀6打开(DN100支路)
   ↓
3. 压力建立
   压力传感器6检测 → 压力传感器7检测 → 压力传感器8检测
   ↓
4. 流量稳定
   流量计1监测 → 达到目标流量(85L/min)
   ↓
5. 压力调节
   电动调压阀1工作 → 设定压力0.8MPa → 实际压力稳定在0.8±0.02MPa
   ↓
6. 数据采集
   持续记录: 流量、压力、温度 → 1秒1次 → 持续60秒
   ↓
7. 测试结束
   电动调压阀1回零 → 电动阀6关闭 → 变频泵1降速 → 停止
```

## 水流状态监控

### 实时参数显示

```
┌─────────────────────────────────────────────────────┐
│              主回路实时状态                          │
├─────────────────────────────────────────────────────┤
│ 变频泵1:    运行中    50.0 Hz    45.2 A    15.8 kW │
│ ├─ 出口压力: 0.90 MPa                               │
│ └─ 流量: 85.3 L/min                                 │
│                                                      │
│ 电动阀1:    已开启    开度 100%                     │
│ └─ 压力降: 0.03 MPa                                 │
│                                                      │
│ 压力传感器1: 0.87 MPa  ✓正常                       │
│                                                      │
│ 流量计1:    85.3 L/min                              │
│ ├─ 累计流量: 142.5 L                                │
│ └─ 水温: 25.3 °C                                    │
│                                                      │
│ 电动调压阀1: 工作中                                 │
│ ├─ 设定: 0.80 MPa                                   │
│ ├─ 实际: 0.80 MPa                                   │
│ └─ 偏差: 0.00 MPa  ✓                                │
│                                                      │
│ 测试点1:    0.80 MPa    85.3 L/min                 │
└─────────────────────────────────────────────────────┘
```

## 压力分布曲线

```
压力(MPa)
1.0  │  ●变频泵出口
     │  │
0.9  │  ├──●阀1后
     │     │
0.85 │     └──●传感器1
     │        │
0.85 │        ├──●流量计1
     │        │
0.82 │        └──●调压阀前
     │           │
0.80 │           └──●测试点
     │              
0.0  └────────────────────────→ 距离(m)
     0   5   8   16  22  26
```

## 管路控制逻辑

### 阀门控制策略

```python
# 伪代码示例
def start_dn100_test():
    # 1. 关闭所有阀门
    close_all_valves()
    
    # 2. 打开DN100测试路径
    open_valve(1)   # 主进阀
    open_valve(6)   # DN100支路阀
    open_valve(8)   # 回流阀
    
    # 3. 启动泵
    start_pump(1, initial_frequency=30)
    
    # 4. 等待管路充满
    wait_until(pressure_sensor_6 > 0.1)  # 等待0.1MPa
    
    # 5. 逐步增加频率
    ramp_up_pump(1, target_frequency=50, step=2, interval=1)
    
    # 6. 等待流量稳定
    wait_until(flow_meter_1.stable_for(5))  # 稳定5秒
    
    # 7. 调节压力
    regulating_valve_1.set_pressure(0.8)  # MPa
    
    # 8. 开始记录数据
    start_data_logging()
```

## 应用场景

1. **系统初始化时** - 建立管路拓扑模型
2. **启动测试前** - 验证流动路径是否正确
3. **实时监控中** - 显示当前激活的水流路径
4. **故障诊断时** - 追踪问题设备位置
5. **GUI可视化** - 动态显示水流流动动画

## 代码集成

```cpp
// 初始化管路拓扑
PipelineTopology::getInstance().initialize();

// 获取水流路径
auto paths = PipelineTopology::getInstance().traceWaterFlow();

// 生成流程图
std::string diagram = PipelineTopology::getInstance().generateFlowDiagram();

// 更新管段状态
PipelineTopology::getInstance().updatePipeSegment(101, 85.3f, 850000.0f);
```
