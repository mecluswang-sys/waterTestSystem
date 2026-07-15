# Station1 自检 HMI 绑定与仿真步骤

## 1. 目标

把 Station1 自检命令与状态在 TIA/WinCC 中完成绑定，并通过 PLCSIM 先跑通流程。

本指南对应文件:
- SCL: docs/STATION1_SELFCHECK_PLC_TIA_SCL_v1.0.scl

## 2. 绑定对象

### 2.1 HMI 命令位 (写入)

绑定到 DB_WTS_SelfCheck_HMI:

| HMI控件 | PLC符号地址 | 类型 | 说明 |
|---|---|---|---|
| 自检使能开关 | DB_WTS_SelfCheck_HMI.Cmd.Enable | BOOL | 1=允许自检 |
| 开始自检按钮 | DB_WTS_SelfCheck_HMI.Cmd.Start | BOOL | 按钮脉冲 |
| 中止按钮 | DB_WTS_SelfCheck_HMI.Cmd.Abort | BOOL | 按钮脉冲 |
| 复位按钮 | DB_WTS_SelfCheck_HMI.Cmd.Reset | BOOL | 按钮脉冲 |

按钮建议:
- Start/Abort/Reset 使用 Pressed=1, Released=0
- 避免保持置位导致重复触发

### 2.2 HMI 状态字段 (只读)

绑定到 DB_WTS_SelfCheck_St1:

| HMI控件 | PLC符号地址 | 类型 | 说明 |
|---|---|---|---|
| 运行中指示 | DB_WTS_SelfCheck_St1.Status.Busy | BOOL | 1=流程运行 |
| 完成指示 | DB_WTS_SelfCheck_St1.Status.Done | BOOL | 1=流程结束 |
| 通过指示 | DB_WTS_SelfCheck_St1.Status.Passed | BOOL | 1=判定通过 |
| 失败指示 | DB_WTS_SelfCheck_St1.Status.Failed | BOOL | 1=判定失败 |
| 当前步骤 | DB_WTS_SelfCheck_St1.Status.StepNo | INT | 0/5/10/20/30/40/90/99 |
| 故障码 | DB_WTS_SelfCheck_St1.Status.FaultCode | WORD | 16#0000=无故障 |
| PS4前值 | DB_WTS_SelfCheck_St1.Status.Ps4Before | REAL | kPa |
| PS4后值 | DB_WTS_SelfCheck_St1.Status.Ps4After | REAL | kPa |
| 建压PS4 | DB_WTS_SelfCheck_St1.Status.Ps4Build | REAL | kPa |
| 保压PS4 | DB_WTS_SelfCheck_St1.Status.Ps4Hold | REAL | kPa |
| 建压PS5 | DB_WTS_SelfCheck_St1.Status.Ps5Build | REAL | kPa |
| 保压PS5 | DB_WTS_SelfCheck_St1.Status.Ps5Hold | REAL | kPa |
| PS4压降 | DB_WTS_SelfCheck_St1.Status.Ps4Delta | REAL | kPa |
| PS5窜压 | DB_WTS_SelfCheck_St1.Status.Ps5Delta | REAL | kPa |

## 3. StepNo 建议文案映射

| StepNo | 文案 |
|---:|---|
| 0 | 空闲 |
| 5 | 前置检查 |
| 10 | 步骤1 三阀联动 |
| 20 | 步骤2 建压 |
| 30 | 步骤3 保压泄漏判定 |
| 40 | 步骤4 顺序动作 |
| 90 | 通过 |
| 99 | 失败 |

## 4. FaultCode 建议文案映射

| 故障码 | 文案 |
|---|---|
| 16#0000 | 无故障 |
| 16#0001 | 未使能 |
| 16#0002 | 急停触发 |
| 16#0003 | 人工中止 |
| 16#0101 | 联动压差不足 |
| 16#0201 | 建压超时 |
| 16#0202 | 建压不足 |
| 16#0301 | 外泄漏 |
| 16#0302 | 内泄漏 |
| 16#0401 | 状态机异常 |

## 5. PLCSIM 仿真步骤

1. 在 TIA 导入 SCL，确保以下对象存在:
- FB_WTS_Station1SelfCheck
- DB_WTS_SelfCheck_St1
- DB_WTS_SelfCheck_HMI

2. 在 OB1 或主循环 FB 中按 SCL文件末尾示例调用 FB。

3. 下载到 PLCSIM 并切换 RUN。

4. 打开 Watch Table，至少监视:
- DB_WTS_SelfCheck_HMI.Cmd.Enable/Start/Abort/Reset
- DB_WTS_SelfCheck_St1.Status.Busy/Done/Passed/Failed/StepNo/FaultCode
- 输入量 Ps4Kpa/Ps5Kpa 对应变量

5. 先跑成功路径:
- Enable=1
- 脉冲 Start
- 在 Step20 前后把 Ps4 调到 >= BuildMinKpa
- 在 Step30 让 Ps4Delta/ Ps5Delta 落在合格范围
- 预期: StepNo=90, Passed=1, FaultCode=0

6. 跑失败路径A（联动失败）:
- Step10 时保持 Ps4Before 与 Ps4After 变化量 < LinkageMinDeltaKpa
- 预期: StepNo=99, FaultCode=16#0101

7. 跑失败路径B（建压超时）:
- Step20 时保持 Ps4 < BuildMinKpa 到超时
- 预期: StepNo=99, FaultCode=16#0201

8. 跑失败路径C（中止）:
- 流程运行中脉冲 Abort
- 预期: StepNo=99, FaultCode=16#0003

9. 故障后复位:
- 脉冲 Reset
- 预期: StepNo=0, Busy=0, Done=0, FaultCode=0

## 6. HMI 画面最小化布局建议

1. 命令区: Enable开关 + Start/Abort/Reset 三按钮
2. 状态区: Busy/Done/Passed/Failed 灯 + StepNo + FaultCode
3. 过程区: Ps4Before/Ps4After/Ps4Delta/Ps5Delta 四个数值框
4. 参数区: Param 中阈值可读写（调试页）

## 7. 注意事项

1. Start/Abort/Reset 必须做脉冲，不建议常开保持。
2. 自检时输出命令由 FB 接管，避免与手动控制并发写冲突。
3. 若使用优化块访问，HMI 使用符号方式绑定，不要手填绝对地址。
4. 如果你现场需要按阀反馈到位判定，建议在 v1.1 增加每步动作到位超时。