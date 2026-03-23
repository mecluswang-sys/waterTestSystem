# PLC DB地址映射表 v1.0

文档编号: WTS-DB-MAP-20260318
版本: v1.0
日期: 2026-03-18
适用范围: 一期联调与二期扩展基线

---

## 1. 使用说明

本表定义PLC DB字段与上位机变量映射关系，用于:

1. PLC程序开发
2. 上位机采集与控制实现
3. 联调与验收比对

映射规则（当前压力链路）:

- 通道偏移 = base_offset + (sensor_id - 1) * item_size
- 主值地址 = 通道偏移 + main_value_real_offset

当前配置:

- db.sensor.number = 6
- db.sensor.base_offset = 0
- db.sensor.item_size = 108
- db.sensor.main_value_real.offset = 46
- db.sensor.main_decimal.offset = -1
- db.pressure.scale = 1

---

## 2. DB6 传感器结构映射（已确认）

### 2.1 UDT关键字段（单结构体内）

| 字段名 | 类型 | 偏移（Byte） | 说明 |
|---|---|---:|---|
| MainValue_Real | REAL | 46 | 压力主值（当前读取源） |
| Decimal | UINT | 58 | 小数位（当前上位机禁用） |
| Offset | REAL | 76 | 偏移量参数 |
| Gain | REAL | 96 | 增益参数 |

### 2.2 三路压力通道地址（已投产配置）

| 传感器ID | 计算公式 | PLC地址 |
|---:|---|---|
| 1 | 0 + (1-1)*108 + 46 | DB6.DBD46 |
| 2 | 0 + (2-1)*108 + 46 | DB6.DBD154 |
| 3 | 0 + (3-1)*108 + 46 | DB6.DBD262 |

### 2.3 上位机变量映射

| 上位机变量 | 来源地址 | 类型 | 单位策略 |
|---|---|---|---|
| PressureSensor[1].pressure | DB6.DBD46 | float | scale=1（原值） |
| PressureSensor[2].pressure | DB6.DBD154 | float | scale=1（原值） |
| PressureSensor[3].pressure | DB6.DBD262 | float | scale=1（原值） |

---

## 3. DB2 流量映射（待冻结）

| 字段组 | 建议地址 | 类型 | 方向 | 备注 |
|---|---|---|---|---|
| FM1_VALUE | DB2.DBD(待定) | REAL | PLC->HMI | 一期需确认 |
| FM2_VALUE | DB2.DBD(待定) | REAL | PLC->HMI | |
| FM3_VALUE | DB2.DBD(待定) | REAL | PLC->HMI | |
| FM4_VALUE | DB2.DBD(待定) | REAL | PLC->HMI | |

---

## 4. DB3 阀门映射（待冻结）

| 字段名 | 建议地址 | 类型 | 方向 | 说明 |
|---|---|---|---|---|
| VLV01_CMD | DB3.DBX(待定) | BOOL | HMI->PLC | 阀1命令 |
| VLV01_FB | DB3.DBX(待定) | BOOL | PLC->HMI | 阀1反馈 |
| ... | ... | ... | ... | 按11路复制 |
| VLV11_CMD | DB3.DBX(待定) | BOOL | HMI->PLC | 阀11命令 |
| VLV11_FB | DB3.DBX(待定) | BOOL | PLC->HMI | 阀11反馈 |

---

## 5. DB4 泵映射（待冻结）

| 字段名 | 建议地址 | 类型 | 方向 | 说明 |
|---|---|---|---|---|
| PUMP01_CMD | DB4.DBX(待定) | BOOL | HMI->PLC | 泵1启停 |
| PUMP01_FB | DB4.DBX(待定) | BOOL | PLC->HMI | 泵1反馈 |
| PUMP02_CMD | DB4.DBX(待定) | BOOL | HMI->PLC | 泵2启停 |
| PUMP02_FB | DB4.DBX(待定) | BOOL | PLC->HMI | 泵2反馈 |

---

## 6. DB5 系统状态映射（待冻结）

| 字段名 | 建议地址 | 类型 | 方向 | 说明 |
|---|---|---|---|---|
| SYS_MODE | DB5.DBW(待定) | WORD | PLC->HMI | 系统模式 |
| ESTOP_STATE | DB5.DBX(待定) | BOOL | PLC->HMI | 急停状态 |
| ALARM_CODE | DB5.DBW(待定) | WORD | PLC->HMI | 当前报警码 |

---

## 7. 版本控制字段（建议强制）

建议在系统状态DB增加:

| 字段名 | 类型 | 方向 | 说明 |
|---|---|---|---|
| DB_VERSION | UINT | PLC->HMI | 数据结构版本号 |
| DB_LAYOUT_ID | UINT | PLC->HMI | 地址布局ID |

上位机启动流程:

1. 先读取 DB_VERSION / DB_LAYOUT_ID
2. 与内置期望值比对
3. 不一致则禁写、仅保留只读并弹出告警

---

## 8. 单位与缩放规范

1. 当前压力采用原值读取（scale=1）。
2. 若切换单位，不改核心读取地址，仅改配置层比例系数。
3. 所有单位转换在上位机统一处理，PLC侧保持原始工程值。

---

## 9. 验收核对清单

1. 三路压力地址与趋势一致。
2. 写命令后300 ms内回读反馈状态。
3. 断线重连后10秒内恢复采集。
4. DB版本不匹配时，系统进入只读告警。

---

## 10. 待确认项

1. DB2/DB3/DB4/DB5最终偏移地址。
2. 阀门与泵命令位定义（置位脉冲或保持位）。
3. 流量与温度单位最终工程定义。
4. 报警编码字典与映射表。
