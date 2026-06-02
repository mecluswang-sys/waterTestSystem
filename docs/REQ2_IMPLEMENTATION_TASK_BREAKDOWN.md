# REQ2 实施任务向下拆分（可执行版）

## 1. 目标与范围

基于 `REQ2_REQUIREMENTS_SPEC_V1.0.md`，将需求拆解为可编码、可联调、可验收的任务包。

- 覆盖模块：流程状态机、判定算法、联锁安全、HMI、日志追溯、网络命令、配置治理
- 输出形式：任务包 -> 子任务 -> 文件级改动 -> 验收标准
- 估算单位：人日（1 人日 = 8 小时）

## 2. 里程碑与阶段

- M1（P0）：核心测试链路可跑通（状态机 + 判定 + 急停 + 泄压）
- M2（P1）：多工位、寿命测试、气路分控、HMI可用
- M3（P2）：追溯、协议扩展、配置规范、验收闭环

---

## 3. P0 任务包（必须先做）

### P0-1 流程状态机化（Station1）

- 工时：2.0 人日
- 负责人建议：GUI + 控制逻辑
- 涉及文件：
  - `include/gui/Station1Panel.h`
  - `src/gui/Station1Panel.cpp`
- 子任务：
  1. 新增状态枚举与上下文结构（当前步骤、计时、失败原因）
  2. 把现有 `onSelfCheck()` 拆为状态驱动函数
  3. 增加失败分支和恢复路径（FAULT_STOP -> DEPRESSURE -> IDLE）
  4. 增加状态切换日志
- 验收标准：
  - 手动触发一次完整流程可从 IDLE 回到 IDLE
  - 任一步骤失败后可进入故障停机并执行泄压

### P0-2 开阀/泄漏判定参数化（按 DN 分段）

- 工时：1.5 人日
- 涉及文件：
  - `src/gui/Station1Panel.cpp`
  - `config/system.conf`
- 子任务：
  1. 新增 DN 分段阈值配置（压降、窗口、超时）
  2. 抽离判定函数（开阀、泄漏）
  3. 对 UI 文案统一输出方向与绝对值
  4. 增加缺省参数和边界保护
- 验收标准：
  - 同一工况重复 20 次，判定结果一致
  - 参数改动无需改代码，重启后生效

### P0-3 急停与联锁优先级统一

- 工时：2.0 人日
- 涉及文件：
  - `include/DeviceManager.h`
  - `src/DeviceManager.cpp`
  - `include/DeviceTypes.h`
  - `src/gui/TestPanel.cpp`
  - `src/gui/Station1Panel.cpp`
- 子任务：
  1. 明确急停优先级为最高
  2. 输出统一安全位动作（关阀、停泵、切断水气源）
  3. 联锁失败转标准故障码
  4. 恢复流程加确认机制
- 验收标准：
  - 任意状态触发急停，500ms 内进入安全态
  - 急停事件有完整日志

### P0-4 水气切换失败处理与故障码体系

- 工时：1.5 人日
- 涉及文件：
  - `include/DeviceTypes.h`
  - `src/DeviceManager.cpp`
  - `src/gui/Station1Panel.cpp`
- 子任务：
  1. 定义故障码（切换失败、超时、传感器异常）
  2. 把现有 QMessageBox 文案映射到故障码
  3. 故障码写日志并显示在 HMI
- 验收标准：
  - 每类故障可复现并可追溯

### P0-5 开始/停止 + 物理按钮联动回归

- 工时：0.5 人日
- 涉及文件：
  - `include/gui/Station1Panel.h`
  - `src/gui/Station1Panel.cpp`
  - `include/DeviceManager.h`
  - `src/DeviceManager.cpp`
  - `config/system.conf`
- 子任务：
  1. 回归软按钮与 M 位边沿触发
  2. 增加防抖与误触保护测试
  3. 校验远程/本地模式下动作一致
- 验收标准：
  - 软硬按钮都能稳定控制同一动作链路

---

## 4. P1 任务包（联调期完成）

### P1-1 多工位隔离与并发控制

- 工时：2.5 人日
- 涉及文件：
  - `include/gui/Station2Panel.h`
  - `include/gui/Station3Panel.h`
  - `src/gui/Station2Panel.cpp`
  - `src/gui/Station3Panel.cpp`
  - `src/TerminalServer.cpp`
  - `include/TerminalServer.h`
- 子任务：
  1. 定义工位资源边界与互斥规则
  2. 增加并发冲突处理
  3. 工位间互不影响验证
- 验收标准：
  - 2-3 工位并行运行，无交叉动作

### P1-2 寿命测试闭环

- 工时：2.0 人日
- 涉及文件：
  - `include/gui/AutoTestPanel.h`
  - `src/gui/AutoTestPanel.cpp`
  - `src/DeviceManager.cpp`
  - `config/system.conf`
- 子任务：
  1. 循环计数与目标阈值配置
  2. 不合格自动停机与泄压
  3. 记录循环次数与失败原因
- 验收标准：
  - 可稳定执行长循环并自动收敛

### P1-3 吹气排液判据落地

- 工时：1.0 人日
- 涉及文件：
  - `src/gui/Station1Panel.cpp`
  - `src/DeviceManager.cpp`
  - `config/system.conf`
- 子任务：
  1. 吹气时长、等待时间参数化
  2. 增加吹气完成判据
  3. 输出吹气结果字段
- 验收标准：
  - 吹气步骤具备 PASS/FAIL 结论

### P1-4 HMI 统一状态显示

- 工时：1.5 人日
- 涉及文件：
  - `include/gui/MainWindow.h`
  - `src/gui/MainWindow.cpp`
  - `src/gui/MonitorPanel.cpp`
  - `src/gui/Station1Panel.cpp`
- 子任务：
  1. 状态字典统一（步骤、故障码、联锁状态）
  2. 显示寿命件剩余寿命与更换提醒
  3. 界面提示一致化
- 验收标准：
  - 不同面板对同一状态显示一致

### P1-5 高/低压两路气源分控

- 工时：1.5 人日
- 涉及文件：
  - `src/DeviceManager.cpp`
  - `src/gui/PreparationPanel.cpp`
  - `config/system.conf`
- 子任务：
  1. 建立高压/低压通道配置
  2. 增加切换联锁与互斥
  3. 增加面板操作与状态反馈
- 验收标准：
  - 两路气源可控、可见、可追溯

---

## 5. P2 任务包（验收前完善）

### P2-1 追溯字段补齐

- 工时：2.0 人日
- 涉及文件：
  - `include/DataLogger.h`
  - `src/DataLogger.cpp`
  - `include/DatabaseManager.h`
  - `src/DatabaseManager.cpp`
  - `include/DeviceTypes.h`
- 子任务：
  1. 新增工位号、配方版本、失败原因字段
  2. 数据库与CSV对齐
  3. 查询接口补齐
- 验收标准：
  - 任意一条测试记录可完整回放关键信息

### P2-2 网络命令扩展（流程级）

- 工时：1.5 人日
- 涉及文件：
  - `include/NetworkProtocol.h`
  - `src/NetworkProtocol.cpp`
  - `include/StationClient.h`
  - `src/StationClient.cpp`
  - `include/TerminalServer.h`
  - `src/TerminalServer.cpp`
- 子任务：
  1. 扩展命令类型（启动测试、停止测试、复位、泄压）
  2. 增加 ACK/错误码
  3. 加入超时与重试策略
- 验收标准：
  - 流程级命令远程可控且有回执

### P2-3 配置治理与校验

- 工时：1.0 人日
- 涉及文件：
  - `include/ConfigManager.h`
  - `src/ConfigManager.cpp`
  - `config/system.conf`
- 子任务：
  1. 规格书参数全量落配置
  2. 增加范围校验（阈值、时间、位号）
  3. 配置版本化
- 验收标准：
  - 非法配置可被拦截并提示

---

## 6. 任务执行顺序（建议）

1. P0-1 -> P0-2 -> P0-3 -> P0-4 -> P0-5
2. P1-1 -> P1-2 -> P1-3 -> P1-4 -> P1-5
3. P2-1 -> P2-2 -> P2-3

## 7. 总工时预估

- P0：7.5 人日
- P1：8.5 人日
- P2：4.5 人日
- 总计：20.5 人日（不含现场联调等待）

## 8. 每包完成定义（DoD）

1. 功能可跑通且有失败分支
2. 参数可配置
3. 日志可追溯
4. 面板可视化可确认
5. 至少 1 条联调记录（步骤、截图、结论）
