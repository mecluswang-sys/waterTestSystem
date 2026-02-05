# 测试准备区（PreparationPanel）按 Industrial SCADA Dashboard 风格重规划

目标：复用现有 QGraphicsScene 的 P&ID/拟物图元能力，但把整体信息架构与视觉语言对齐 `Industrial SCADA Monitoring Dashboard/`（黑底工业风、霓虹色状态、可选中设备+底部详情控制、告警浮层）。

## 1. Dashboard 风格要点（来自 React 工程）

参考：

- `Industrial SCADA Monitoring Dashboard/src/components/TopBar.tsx`
- `Industrial SCADA Monitoring Dashboard/src/components/BottomPanel.tsx`
- `Industrial SCADA Monitoring Dashboard/src/components/OverlayLayer.tsx`

视觉语言（核心 token）：

- 主背景：`#1a1a1a`
- 边框：`#444`
- 工业霓虹：
  - Online/OK：`#0f0` / `#0a0`
  - Warning：`#fa0`
  - Critical：`#f00` / `#a00`
  - Info/Cyan：`#0af`
- 数值显示：偏等宽字体（类似 `font-mono`）
- 卡片/参数块：`bg-[#2a2a2a] border-[#444]`，小标题 9~10px，值大号加粗

## 2. Qt 侧总体结构（建议）

现状：`src/gui/PreparationPanel.cpp` 使用 `QGraphicsView/QGraphicsScene` 绘制流程，属性 tag 通过 `QGraphicsProxyWidget` 贴在节点上，控制台也是场景内 `QGraphicsProxyWidget`。

建议分层：

1) **P&ID 场景层（Scene Layer）**
   - 继续使用 QGraphicsItem（泵/阀/传感器/分水罐/管线）
   - 增加“可选中状态”（边框高亮、发光描边）

2) **信息/交互层（UI Overlay Layer）**
   - 维持“控制台固定显示”（用户已确认不做拖拽/吸附）
   - 增加 **设备详情底栏（BottomPanel-like）**：选中设备后在底部显示参数与控制（泵/阀可控，传感器/安全阀只读）

3) **告警浮层（OverlayLayer-like，可选）**
   - 右上角显示活动告警列表（critical/warn/info），支持“确认/消音”

4) **顶部状态栏（TopBar-like）**
   - 主窗口已有连接按钮与状态，可在准备区顶部栏补充：PLC ONLINE/OFFLINE、模式（MANUAL/AUTO）、活动告警数量

## 3. 数据模型与映射

React 工程的类型：`SystemData`（pumps/valves/sensors/safetyValves/tank/alarms）。

Qt 建议建立轻量 ViewModel（不必大改 DeviceManager）：

- `struct PreparationSnapshot`：一次刷新所需的所有值（泵频率/运行/故障、阀开度、PS1/2/3、SV1/2、罐液位/罐压等）
- `PreparationPanel::onUpdateData()` 拉取 DeviceManager 数据，填充 snapshot，再一次性刷新 UI（避免分散更新造成闪烁）

点位约定：

- PS3 = 罐压
- SV1/SV2 = 继电器 Q0.0/Q0.1（暂定）
- 泄压阀 = 阀11

## 4. 交互规划（对齐 BottomPanel）

- 点击 PumpItem/ValveItem/SensorItem/TankItem/SV tag：选中设备
- 底部详情区显示：
  - 设备名 + 类型 + 数据质量（good/bad）
  - 参数卡片（频率、电流、功率、开度、压力等）
  - 控制按钮：START/STOP、OPEN/CLOSE、SET（频率/开度）
- 危险操作保持“长按 2 秒确认”（泄压阀开启、紧急停止）

## 5. 分阶段实施建议

Phase 1（低风险、最快见效）

- 把 Dashboard 的配色/卡片样式落到 Qt QSS（`resources/styles/industrial_10inch.qss`）
- 移除 `PreparationPanel` 里的 inline stylesheet，统一走 QSS，保证一致性

Phase 2（可用性提升）

- 增加“选中设备”能力（QGraphicsItem::mousePressEvent）
- 新增底部详情控制条（不必进 scene，可直接加在 PreparationPanel rootLayout 底部）

Phase 3（现场化）

- 告警浮层 + 告警确认
- 关键状态变更的事件记录/时间戳

---

如果你认可这个方向，我可以先把 Phase 1 的 QSS+控件 objectName 全部整理好；再进入 Phase 2 做“点击选中设备 + 底部详情条”。
