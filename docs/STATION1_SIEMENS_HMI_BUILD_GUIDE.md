# 操作台 1 西门子 HMI 建屏作业书

文档编号: WTS-ST1-HMI-BUILD-20260908
版本: v1.0
日期: 2026-09-08

## 1. 目标和前置条件

本作业书用于在 TIA Portal/WinCC Unified 中搭建操作台 1（DN50）的现场 HMI 画面。对应工程为 `电磁阀测试1.ap20`，目标设备为 `HMI_RT_1 (MTP1000 Unified Basic)`；工程当前已有一个画面对象，后续在此工程内新建操作台 1 画面。HMI 的固定 IP 为 `192.168.33.2`，PLC IP 为 `192.168.33.1`，子网掩码为 `255.255.255.0`。

本步骤基于当前 PC 程序已使用的 PLC 映射。操作台 1 使用 PS3 至 PS8、5 路电磁阀、自检命令/状态和 2 路调压阀。HMI 只能向命令请求区写入；严禁把按钮直接绑定到物理 Q 输出、M100 位或 DB13 的基础控制位，直至 PLC 已实现来源仲裁。

开始前确认：

1. HMI 型号、固件和 WinCC 授权已确定，HMI 已接入与 PC、PLC 相同的工业以太网交换机。
2. PLC 工程在线、已完成编译，允许 HMI 访问的 DB 未启用阻断外部访问的优化访问方式。
3. 现场急停、超压、防夹等安全回路已通过硬件和 PLC 验证；HMI 按钮不是安全功能部件。
4. 在首次联调时断开执行器或使系统处于安全模拟状态。

## 2. TIA Portal 中新增 HMI 设备和网络

1. 在 TIA Portal V20 打开 `电磁阀测试1.ap20`。首次打开前将完整工程保留在原目录，避免手动修改 `IM/`、`System/` 和 `.rdf/.pbb` 文件。
2. 在项目树确认目标设备为 `HMI_RT_1 (MTP1000 Unified Basic)`；本项目已有 HMI 设备，禁止重复添加第二个 HMI。
3. 在“设备与网络”中选择 `HMI_RT_1` 的 PN/IE 接口，连接到 PLC 所在的 `PN/IE_1` 子网。
4. 设置 HMI IPv4 地址为 `192.168.33.2`，掩码为 `255.255.255.0`。不得与 PC 的 `192.168.33.100`、PLC 的 `192.168.33.1` 或其他设备重复。
5. 在 HMI 的“连接”下新建 HMI 到 PLC 的 S7 连接：连接名 `PLC_Station1`，通信伙伴为现场 PLC，接口均为 PN/IE。
6. 保存并编译网络组态。先下载 PLC 的硬件组态，再下载 HMI；下载时确认目标设备 MAC 地址，避免下载至其他现场面板。
7. 在 HMI 在线诊断中确认连接状态为绿色，并从工程电脑执行 `ping 192.168.33.2`。

## 3. 创建 PLC 对外接口

为避免 PC 和 HMI 双写，PLC 工程必须提供 `DB_WTS_ST1_HMI` 作为 HMI 命令接口。当前 PC 程序中的 DB11、DB13 和 M100 映射仅用于既有逻辑对照，不能直接用作 HMI 的手动控制写入目标。

建议 PLC 工程师在 `DB_WTS_ST1_HMI` 中建立以下符号结构，并让 PLC 周期程序将请求映射到安全仲裁逻辑：

| 符号路径 | 类型 | 方向 | 说明 |
|---|---|---|---|
| `Status.HmiOnline` | Bool | PLC -> HMI | HMI 心跳/连接诊断 |
| `Status.ControlSource` | USInt | PLC -> HMI | `0=无, 1=PC, 2=HMI, 3=PLC自动` |
| `Status.ManualAllowed` | Bool | PLC -> HMI | 允许 HMI 手动控制 |
| `Status.AutoBusy` | Bool | PLC -> HMI | PLC 自检或自动流程运行中 |
| `Status.CommandAccepted` | Bool | PLC -> HMI | 最近命令接受结果 |
| `Status.CommandRejectCode` | UInt | PLC -> HMI | 最近命令拒绝原因 |
| `Status.AckSequence` | UDInt | PLC -> HMI | 已处理命令序号 |
| `CmdFromHmi.Sequence` | UDInt | HMI -> PLC | 每次新命令递增一次 |
| `CmdFromHmi.Command` | USInt | HMI -> PLC | 命令类型 |
| `CmdFromHmi.Index` | USInt | HMI -> PLC | 阀/设备序号 |
| `CmdFromHmi.Action` | Bool | HMI -> PLC | `1=开/启动, 0=关/停止` |
| `CmdFromHmi.Reset` | Bool | HMI -> PLC | 复位请求 |
| `CmdFromHmi.AlarmAck` | Bool | HMI -> PLC | 报警确认请求 |

命令值建议：`1=电磁阀`、`2=调压阀`、`3=自检`、`4=系统启停`。PLC 只在 `Sequence` 变化时消费命令一次，处理后更新 `AckSequence`、`CommandAccepted` 与 `CommandRejectCode`。

### 3.1 "全部打开/全部关闭"批量阀门接口（仅 PLC 侧参考设计，C++ 尚未接入）

`docs/PLC/STATION1_BATCH_VALVES_PLC_TIA_SCL_v1.0.scl` 提供了一份可选的 PLC 侧批量阀门顺序执行参考实现（`DB14_WTS_ST1_BatchValves` + `FB_ST1_BatchAllValves`），把"全部打开/全部关闭"的顺序时序放在 PLC 内部执行。**当前 C++ 程序未做任何改动**，操作台1工具栏的"全部打开"“全部关闭”按钮仍然是上位机按固定顺序、间隔 800ms 逐个调用现有阀门/调压阀点位（`controlAllRelayValves()`），与本节的 DB14 接口无关。

若后续需要把该批量逻辑真正下沉到 PLC 并让 C++/HMI 共用，需要另行评审并修改 C++ 代码（`DeviceManager`、`Station1Panel`、`NetworkProtocol`），本文档只保留 PLC 侧的接口设计供参考：

| 符号路径 | 类型 | 方向 | 说明 |
|---|---|---|---|
| `Cmd.OpenAll` | Bool | HMI -> PLC | 上升沿触发"全部打开"，建议 100 ms 脉冲 |
| `Cmd.CloseAll` | Bool | HMI -> PLC | 上升沿触发"全部关闭"，建议 100 ms 脉冲 |
| `Cmd.Abort` | Bool | HMI -> PLC | 上升沿中止当前批量流程 |
| `Status.Busy` | Bool | PLC -> HMI | 批量流程运行中 |
| `Status.Done` | Bool | PLC -> HMI | 上一次批量流程正常完成 |
| `Status.Failed` | Bool | PLC -> HMI | 上一次批量流程被中止/失败 |
| `Status.Direction` | Int | PLC -> HMI | `0=无, 1=全部打开, 2=全部关闭` |
| `Status.Step` | Int | PLC -> HMI | 当前/最近一次步骤序号 `0..6` |


## 4. HMI 变量表

在“HMI 变量”下新建变量表 `ST1_Operation`，连接全部选择 `PLC_Station1`。优先以 PLC 符号路径绑定；下表的绝对地址仅作为现有 PC 映射核对，不能取代 PLC 仲裁接口。

### 4.1 可立即绑定的只读过程量

| HMI 变量名 | PLC 地址 | 类型 | 说明/格式 |
|---|---|---|---|
| `ST1_PS3_kPa` | `DB1.DBD636` | Real | PS3，`0.0 kPa` |
| `ST1_PS4_kPa` | `DB1.DBD640` | Real | PS4，`0.0 kPa` |
| `ST1_PS5_kPa` | `DB1.DBD644` | Real | PS5，`0.0 kPa` |
| `ST1_PS6_kPa` | `DB1.DBD648` | Real | PS6，`0.0 kPa` |
| `ST1_PS7_kPa` | `DB1.DBD652` | Real | PS7，`0.0 kPa` |
| `ST1_PS8_kPa` | `DB1.DBD656` | Real | PS8，`0.0 kPa` |
| `ST1_SelfCheckBusy` | `DB6.DBX0.0` | Bool | 自检运行中 |
| `ST1_SelfCheckDone` | `DB6.DBX0.1` | Bool | 自检结束 |
| `ST1_SelfCheckPassed` | `DB6.DBX0.2` | Bool | 自检通过 |
| `ST1_SelfCheckFailed` | `DB6.DBX0.3` | Bool | 自检失败 |
| `ST1_SelfCheckStep` | `DB6.DBW2` | Int | 当前步骤号 |
| `ST1_SelfCheckFault` | `DB6.DBW4` | Int | 故障码，以十六进制显示 |
| `ST1_PS4Before_kPa` | `DB6.DBD6` | Real | 自检快照 |
| `ST1_PS4After_kPa` | `DB6.DBD10` | Real | 自检快照 |
| `ST1_PS4Build_kPa` | `DB6.DBD14` | Real | 自检快照 |
| `ST1_PS4Hold_kPa` | `DB6.DBD18` | Real | 自检快照 |
| `ST1_PS5Build_kPa` | `DB6.DBD22` | Real | 自检快照 |
| `ST1_PS5Hold_kPa` | `DB6.DBD26` | Real | 自检快照 |
| `ST1_PS4Delta_kPa` | `DB6.DBD30` | Real | 自检快照 |
| `ST1_PS5Delta_kPa` | `DB6.DBD34` | Real | 自检快照 |

`DB1.DBD628` 是 PS1，PS3 的计算式为 `628 + (3 - 1) * 4 = 636`。所有压力统一使用 PLC 工程单位 `kPa`；HMI 不增加二次比例或单位换算。

### 4.2 既有 PC 控制点核对表

以下变量仅用于在线监视和 PLC 接口迁移核对：

| 对象 | 既有点位 | 当前 PC 用途 |
|---|---|---|
| 电磁阀 1 | `M100.0` / `DB13.DBX0.0` | PC 手动控制/PLC 自检接管 |
| 电磁阀 2 | `M100.1` / `DB13.DBX0.1` | PC 手动控制/PLC 自检接管 |
| 电磁阀 3 | `M100.2` / `DB13.DBX0.2` | PC 手动控制/PLC 自检接管 |
| 电磁阀 4 | `M100.3` / `DB13.DBX0.3` | PC 手动控制/PLC 自检接管 |
| 电磁阀 5 | `M100.4` / `DB13.DBX0.4` | PC 手动控制/PLC 自检接管 |
| 调压阀 1 给定 | `MD26` | PC 写入开度百分比 |
| 调压阀 1 反馈 | `MD21` | PC 读取开度百分比 |
| 调压阀 2 给定 | `MD36` | PC 写入开度百分比 |
| 调压阀 2 反馈 | `MD31` | PC 读取开度百分比 |
| 自检 Enable | `DB11.DBX0.0` | PC 自检使能 |
| 自检 Start | `DB11.DBX0.1` | PC 100 ms 启动脉冲 |
| 自检 Abort | `DB11.DBX0.2` | PC 100 ms 中止脉冲 |
| 自检 Reset | `DB11.DBX0.3` | PC 100 ms 复位脉冲 |

## 5. 画面结构

创建以下画面：

| 画面名称 | 编号 | 内容 |
|---|---:|---|
| `ST1_Overview` | 1 | 工艺总览、PS3..PS8、阀门/调压阀状态、控制源和通信状态 |
| `ST1_SelfCheck` | 2 | 自检启动/中止/复位、步骤、结果、压力快照 |
| `ST1_Manual` | 3 | 带权限和联锁的手动阀门/调压阀请求 |
| `ST1_Trend` | 4 | PS3..PS8 实时趋势与故障码 |
| `ST1_Diagnostics` | 5 | PLC/HMI 连接、DB 版本、命令序号和拒绝码 |

在模板页放置标题“操作台 1 - DN50”、当前时间、主页按钮、报警指示器和通信状态指示器。使用底部导航按钮进入 1 至 5 号画面；紧急停止只显示状态和进入安全说明页，不在 HMI 上模拟硬急停。

## 6. 逐步搭建 `ST1_Overview`

1. 新建画面，命名 `ST1_Overview`，采用现场 HMI 的原生分辨率。将画面更新周期设为不大于 `200 ms`。
2. 左侧绘制与实际管路一致的工艺图。可将 `PLC-HMI/` 中的 `VALVE-open.svg`、`VALVE-close.svg`、`REGULATING-VALVE-open.svg`、`REGULATING-VALVE-close.svg`、`PRESSURE-SENSOR.svg`、`FLOWMETER.svg` 作为图元源；图元仅表达基础状态，不包含实时文字。
3. 在工艺图旁按工艺顺序放置 6 个 I/O 域，分别绑定 `ST1_PS3_kPa` 至 `ST1_PS8_kPa`。格式设为 `0.0`，固定显示单位 `kPa`，量程按工艺上限配置，当前配置的报警上限为 `950 kPa`。
4. 为 5 路电磁阀各放置一个状态符号。状态颜色应绑定 PLC 的最终反馈点，而不是 `CmdFromHmi.Action`、M100 或 DB13 请求位。未提供最终反馈符号前显示“反馈待接入”，不得以命令状态代替阀位反馈。
5. 放置两个调压阀状态卡，展示 PLC 的最终开度反馈。若目前只具备 `MD21` 与 `MD31`，先只读显示；调压阀给定需要接入 `DB_WTS_ST1_HMI.CmdFromHmi` 后才允许写入。
6. 右上角增加运行状态区：`ST1_SelfCheckBusy` 显示“自检运行中”，`ST1_SelfCheckPassed` 显示绿色“通过”，`ST1_SelfCheckFailed` 显示红色“失败”，`ST1_SelfCheckStep` 显示步骤号。
7. 将步骤号配置为文本列表：`0=空闲`、`5=预检`、`10=联通性检查`、`20=建压`、`30=泄漏判定`、`40=脉冲测试`、`90=完成`、`99=失败`。未知值显示“未知”。
8. 在页脚显示 `Status.ControlSource` 和 `Status.ManualAllowed`。控制源不是 HMI 时，所有手动入口显示禁用状态。

## 7. 设备与管线状态表达

HMI 不应只用一个命令位改变图元颜色。对于每台设备和每段管线，至少区分“请求”“最终反馈”和“工艺状态”；尤其不能把阀门开命令或调压阀给定值当作实际通流。

### 7.1 统一状态枚举

由 PLC 在 `DB_WTS_ST1_HMI.Process` 中新增以下只读变量。HMI 只读取这些状态变量，不自行推导安全或工艺结论。

| 变量模式 | 类型 | 枚举 | HMI 表现 |
|---|---|---|---|
| `Valve[n].State` | USInt | `0=未知, 1=关闭, 2=开启中, 3=已开启, 4=关闭中, 5=故障, 6=联锁` | 阀体颜色和状态文字 |
| `RegValve[n].State` | USInt | `0=未知, 1=关闭, 2=调节中, 3=调节就绪, 4=故障, 5=联锁` | 阀体颜色和开度文字 |
| `Pipe[n].State` | USInt | `0=未知, 1=阻断, 2=已连通无流动, 3=介质流动, 4=报警` | 管线底色、流动叠加层和告警闪烁 |
| `Pipe[n].FlowDirection` | SInt | `-1=反向, 0=无, 1=正向` | 流动箭头或虚线移动方向 |
| `Pipe[n].AlarmCode` | UInt | `0=无，其他=PLC 定义` | 管线报警提示和诊断页代码 |

状态语义和颜色固定如下，避免不同画面出现相互冲突的解释：

| 状态 | 颜色 | 图形行为 | 含义 |
|---|---|---|---|
| 未知/离线 | 深灰 | 无动画 | PLC 未刷新、通信异常或反馈不可用 |
| 阻断/关闭 | 灰色 | 无动画 | 阀门关闭、联锁禁止或通路不成立 |
| 已连通无流动 | 蓝色 | 实线 | 通路已具备，但流量/压差未达到流动判据 |
| 开启中/关闭中/调节中 | 琥珀色 | 1 Hz 闪烁 | PLC 正在等待执行器反馈；超时必须转故障 |
| 介质流动/已开启/调节就绪 | 绿色 | 管线流动虚线或箭头 | 最终反馈成立，且管段满足 PLC 工艺判据 |
| 故障/报警 | 红色 | 2 Hz 闪烁 | 执行器反馈超时、流量异常、超压或联锁故障 |
| 联锁 | 橙色 | 无动画，显示锁定图标 | PLC 主动拒绝动作，需排除条件后重试 |

### 7.2 PLC 的判定职责

PLC 程序应将当前分散的阀门请求、最终到位信号、调压阀反馈、压力和流量统一计算为上述状态字。建议逻辑如下：

1. HMI 或 PC 仅写请求，PLC 对请求进行安全和控制源仲裁。
2. PLC 使用输出反馈或经过确认的阀位状态确定 `Valve[n].State`，反馈超时写入 `5=故障`，禁止 HMI 以 `CmdFromHmi.Action` 或 `M100.*` 判断开闭。
3. PLC 按管段两端的阀位反馈、调压阀开度、联锁条件计算 `Pipe[n].State=1` 或 `2`。
4. 只有该段通路成立，并且流量或压差满足现场阈值时，PLC 才输出 `Pipe[n].State=3`。例如可使用 `ABS(FlowRate) >= FlowStartThreshold`，或使用该段上游/下游压力差的工程阈值。
5. 传感器异常、空管、超压、执行器反馈超时或相关联锁有效时，PLC 输出 `Pipe[n].State=4` 并提供 `AlarmCode`。

现有 Qt 界面中 1 至 9 号管段的“通路成立”判断可作为 PLC 首版逻辑基线。`V1=电磁阀1`、`V2=电磁阀2`、`VT=电磁阀3`、`V4=电磁阀4`、`V5=电磁阀5`、`RV1/RV2=调压阀1/2`：

| 管段 | 连通条件 | HMI 状态来源 |
|---:|---|---|
| 1 | 固定主干 | PLC 根据供介质/安全条件输出 |
| 2 | `V1` 已开启 | `Pipe[2].State` |
| 3 | `V1 AND V2` 已开启 | `Pipe[3].State` |
| 4、5 | `V1 AND V2 AND RV1` 已开启 | `Pipe[4/5].State` |
| 6 | `V1 AND V2 AND RV1 AND VT` 已开启 | `Pipe[6].State` |
| 7 | `V1 AND V2 AND RV1 AND VT AND V4` 已开启 | `Pipe[7].State` |
| 8 | `V1 AND V2 AND RV1 AND VT AND V4 AND RV2` 已开启 | `Pipe[8].State` |
| 9 | `V1 AND V2 AND RV1 AND VT AND V4 AND RV2 AND V5` 已开启 | `Pipe[9].State` |

上述“开启”均指最终反馈成立；`Pipe[n].State=3` 还必须满足流量或压差判据。若现场未提供单管段流量计，先以总流量和工艺步骤作为首版判据，并在诊断页明确显示为“推定流动”。

### 7.3 在 WinCC Unified 中配置图元状态

对每一台阀门、调压阀和每一段管线按以下步骤配置，画面对象可保留已有的流程图布局：

1. 选中图元并在属性中命名，例如 `V1_Symbol`、`Pipe_02_Base`、`Pipe_02_Flow`。使用固定对象名，便于事件和后续维护。
2. 对阀门图元使用“外观/填充颜色”的动态属性，绑定 `Valve[n].State`。设置离散值映射：`1=灰色`、`2/4=琥珀色`、`3=绿色`、`5=红色`、`6=橙色`、其他=深灰色。
3. 在阀门旁新增只读文本域，显示“关闭、开启中、已开启、关闭中、故障、联锁”；文字值由状态文本列表或 Unified 脚本根据同一状态字转换，不能显示 HMI 自己的按钮状态。
4. 每段管道保留一个常显底层对象 `Pipe_nn_Base`，其线条颜色动态绑定 `Pipe[n].State`：`1=灰色`、`2=蓝色`、`3=绿色`、`4=红色`、其他=深灰色。
5. 在底层上方复制一段透明背景的流动虚线或箭头对象 `Pipe_nn_Flow`。其“可见性”仅在 `Pipe[n].State=3` 时为真；在其效果/动画中启用沿线移动。`FlowDirection=1` 时正向移动，`-1` 时反向移动，`0` 时隐藏。
6. 对 `State=2/4` 的设备使用 1 Hz 闪烁，对 `State=4` 的管线和 `State=5` 的设备使用 2 Hz 红色闪烁。闪烁由动态外观或 Unified 的循环动画配置，不能用不断写 PLC 变量实现。
7. 对 `State=4` 的管线和 `State=5/6` 的设备，在对象上叠加报警/锁定图标；图标可见性绑定相应状态。点击图标跳转到 `ST1_Diagnostics` 并显示 `AlarmCode`。
8. 为所有动画增加通信保护条件：当 PLC 连接或 `Status.HmiOnline` 无效时，隐藏流动层、停止闪烁并强制显示灰色“通信未知”状态。

### 7.4 画面验证

在执行器断开或安全仿真条件下，依次强制 PLC 输出每个状态值，核对同一对象在 `未知 -> 阻断 -> 已连通无流动 -> 介质流动 -> 报警` 间的颜色、文字和动画均一致。再验证阀门处于“开启中”但反馈未到时管线不能显示绿色流动；这是区分命令状态和实际工艺状态的最低验收点。

## 8. 逐步搭建 `ST1_SelfCheck`

1. 放置“启动自检”“中止自检”“复位”三个按钮，均配置为“按下时置位、松开时复位”的瞬时操作，脉冲宽度 `100 ms`。
2. 按钮不得直接写 `DB11.DBX0.1..0.3`。通过脚本或事件依次写入 `CmdFromHmi.Command=3`、对应 `Action/Index`，最后将 `CmdFromHmi.Sequence` 加 1。PLC 只消费新序号。
3. “启动自检”仅在 `Status.ManualAllowed=1`、`Status.AutoBusy=0`、无安全故障且 `ST1_SelfCheckBusy=0` 时可用。中止在自检运行中可用；复位仅在 Done 或 Failed 时可用。
4. 放置步骤号、步骤文字、故障码、通过/失败指示。故障码显示为 4 位十六进制。
5. 使用 8 个只读 I/O 域显示 `ST1_PS4Before_kPa`、`ST1_PS4After_kPa`、`ST1_PS4Build_kPa`、`ST1_PS4Hold_kPa`、`ST1_PS5Build_kPa`、`ST1_PS5Hold_kPa`、`ST1_PS4Delta_kPa`、`ST1_PS5Delta_kPa`。
6. 在页面底部固定显示 `AckSequence`、`CommandAccepted`、`CommandRejectCode`。拒绝时不要只弹出通用提示，应显示 PLC 给出的拒绝原因。

## 9. 逐步搭建 `ST1_Manual`

1. 启用 WinCC 用户管理，至少建立 `Operator` 和 `Engineer` 两个角色。`Operator` 可启动/中止/复位；`Engineer` 才能操作单阀和调压阀。
2. 每个电磁阀仅提供“开请求”和“关请求”命令。按键事件写入 `CmdFromHmi.Command=1`、对应 `Index=1..5`、`Action=1/0`，最后递增 `Sequence`。
3. 对调压阀设置 `Command=2`，并以数值输入框输入 `0.0..100.0 %` 的目标开度。PLC 应增加 `SetpointPercent: Real` 字段，进行范围校验后再输出。
4. 所有手动控件的“允许操作”动态属性绑定 `Status.ManualAllowed AND NOT Status.AutoBusy AND Status.ControlSource=2`。同时在触发前显示确认对话框，内容包含对象名称和目标状态。
5. 每个对象旁边同时展示“请求状态”“PLC 接受结果”“最终反馈”。请求成功不等于阀门动作成功；反馈超时应由 PLC 生成报警。

## 10. 逐步搭建 `ST1_Trend` 和 `ST1_Diagnostics`

1. 在 `ST1_Trend` 添加趋势视图，记录 `ST1_PS3_kPa` 至 `ST1_PS8_kPa`，采样周期 `100 ms`，显示窗口建议 `10 min`。按现场存储容量设置归档周期与保留时间。
2. 增加游标和缩放，不使用写变量修改压力显示倍率。
3. 在 `ST1_Diagnostics` 放置 PLC 连接状态、`Status.HmiOnline`、控制源、当前序号、确认序号、最近接受结果和拒绝码。
4. 建立周期心跳：HMI 每 `1 s` 翻转或递增 `CmdFromHmi.Heartbeat`；PLC 监视超时并更新 `Status.HmiOnline`。该心跳字段需在 `DB_WTS_ST1_HMI` 中由 PLC 工程师增加。
5. 增加 `DB_VERSION` 与 `DB_LAYOUT_ID` 只读字段。任一值不匹配时，显示“接口版本不匹配”，禁用全部写控件。

## 11. 编译、下载和联调顺序

1. 在 TIA Portal 编译 PLC 和 HMI，修复所有变量类型、连接和脚本错误。
2. 在线监视 `DB_WTS_ST1_HMI`、DB1、DB6，核实 HMI 压力和自检状态与 PLC 在线值一致。
3. 不连接执行器时，依次验证启动、中止、复位和每路手动阀请求。每次仅递增一次 `Sequence`，确认 PLC 只执行一次并回写 `AckSequence`。
4. 验证 PC 与 HMI 同时发起同一请求。PLC 必须只接受已获得控制权的一端，另一端收到拒绝码；绝不允许两端同时直接写同一输出。
5. 恢复执行器后，逐阀验证最终反馈、反馈超时、通讯中断、HMI 重启和 PLC 重启。
6. 记录 HMI 项目版本、PLC 程序版本、`DB_VERSION`、`DB_LAYOUT_ID`、HMI IP 和 MAC 地址，归档变量表及画面截图。

## 12. 操作台 1 验收清单

- [ ] HMI 的 IP 为 `192.168.33.2`，可与 PLC `192.168.33.1` 和 PC `192.168.33.100` 连通。
- [ ] 总览正确显示 PS3 至 PS8，且与 PLC 在线值误差不超过显示精度。
- [ ] 自检状态、步骤、故障码和 8 个压力快照正确刷新。
- [ ] 自检和手动按钮只写 HMI 命令请求区，未直接写 Q、M100、DB11 或 DB13。
- [ ] PLC 拒绝的命令可在 HMI 看到原因，HMI 不显示伪成功。
- [ ] 自动流程中手动控件被禁用，PC 与 HMI 并发请求经过 PLC 仲裁。
- [ ] HMI 断网、重启、掉电均不改变 PLC 输出和安全状态。
- [ ] 压力趋势采样周期为 `100 ms`，画面状态刷新周期不大于 `200 ms`。
- [ ] 阀门处于“开启中”或反馈异常时，管线未显示为绿色流动。
- [ ] 每段管线可正确显示未知、阻断、已连通无流动、介质流动和报警状态。
