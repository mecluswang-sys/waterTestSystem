PLC 侧通讯系统总结 (Modbus RTU 转 S7-DB)
1. 硬件配置 (Physical Layer)
    通信模块: SIMATIC S7-1200 + CB 1241 (RS485)
    硬件标识符 (PORT): 269
    通信参数:
        波特率: 9600 bps
        校验位: None (0)
        数据位: 8
        停止位: 1
    超时设定 (RESP_TO): 1000ms (1秒)
2. 逻辑控制机制 (Logic Control)
    触发方式: 由 %M10.3 (脉冲信号) 触发 %M10.4 (启动位)。
    任务切换: 程序通过 %MW16 "Ind_P" 的值动态切换读取任务。
        Case 8/9/10: 切换 Modbus 寄存器起始地址（如 40091 或 40001）。
        目标索引: 计算不同的 #DEST_INDEX（如 119, 136, 154），用于在 C++ 端定位数据偏移。
    看门狗复位 (Watchdog):
        监测信号: %M10.6 "PS1_Busy"。
        触发阈值: 2s。
        动作: 若 Busy 持续 2 秒未消除，自动复位 %M10.4 并停止 MB_MASTER 请求，防止系统死锁。
3. 数据存储结构 (Data Mapping)
    PLC 从传感器读取的数据以 Word (16-bit) 为单位，存放在 DB1 的连续字节空间中。
    C++ 侧解析时需严格遵循以下内存映射结构：
    3.1. 寄存器与字节对齐映射Modbus 
    寄存器 (Register) 每一个占用 2 个字节 (Bytes)。
    18 * 8 结构 (针对 Ind_P >= 8):
    寄存器数量: 18 个 Word。总字节数: $18 \times 2 = 36$ Bytes。
    位空间: $18 \times 16 = 288$ Bits (即你提到的 $18 \times 8$ 对寄存器单元)。
    起始偏移: DB1.DBX 119.0。17 * 8 结构 (针对 Ind_P == 9/10):
    寄存器数量: 17 个 Word。总字节数: $17 \times 2 = 34$ Bytes。位空间: $17 \times 16 = 272$ Bits。起始偏移: DB1.DBX 136.0 或 154.0。

4. 上位机当前对接方式（2026-04）
    上位机已支持 flow.source = plc_buffer 模式：
        当前读取 DB1 从 word_offset=136 开始的 18 个 Word。
        流量解析：
            totalFlow = 寄存器索引 0/1（对应 90/91）
            flowRate  = 寄存器索引 8/9（对应 98/99）
    可通过 config/system.conf 调整：
        flow.plc_buffer.db_number
        flow.plc_buffer.word_offset
        flow.plc_buffer.byte_offset
        flow.plc_buffer.word_count
        flow.plc_buffer.reg_total_index
        flow.plc_buffer.reg_rate_index

5. 现场文档补充（你最新提供的图片）
    该页协议表给出的寄存器定义为：
        90/91  : 正向累计流量 (FLOAT)
        92/93  : 反向累计流量 (FLOAT)
        94/95  : 总累计流量 (FLOAT)
        96/97  : 累积流量清零 (DWORD)
        98/99  : 瞬时流量 (FLOAT)
        100/101: 流速 (FLOAT)
        102/103: 流量百分比 (FLOAT)
        104    : 空管百分比 (WORD)
        105    : 流量单位码 (WORD)
        106    : 空管报警 (WORD)
        107    : 励磁报警 (WORD)

    流量单位码（寄存器 105）:
        0:L/H 1:L/M 2:L/S 3:M3/H 4:M3/M 5:M3/S
        6:KG/H 7:KG/M 8:KG/S 9:T/H 10:T/M 11:T/S

    多站点（1-8）建议：
        若现场是 1~8 号设备轮询，建议每个站点都按 90~107 读取 18 个 WORD，
        并将每个站点写入 DB1 独立缓冲区段（每段 36 字节）。
        上位机可按 "站号 -> byte_offset" 映射解析，避免后续改代码。
