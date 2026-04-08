# Flow Meter PLC UDT v1.0

Document ID: WTS-UDT-FM-20260330
Version: v1.0
Date: 2026-03-30
Scope: Siemens S7-1200 / TIA Portal, for migration from direct Modbus to PLC aggregation.

## 1. Design goals

1. Keep compatibility with current PC software DB2 read layout in DeviceManager.
2. Keep raw Modbus register area for diagnostics and future protocol changes.
3. Make one meter object reusable for FM1..FM4.

## 2. UDT for HMI interface (must match current PC layout)

Important:
- Current PC code reads DB2 with stride 16 bytes per meter.
- Field order expected by PC:
  - offset +0: INT status
  - offset +2: REAL flowRate
  - offset +6: REAL totalFlow
  - offset +10: REAL temperature
- Because this is 14 bytes, reserve 2 bytes to keep stride = 16.

```pascal
TYPE "UDT_WTS_FlowMeter_HMI"
VERSION : 0.1
   STRUCT
      Status      : INT;   // 0=OFFLINE, 1=ONLINE, 2=FAULT
      FlowRate    : REAL;  // Instantaneous flow (recommend L/min)
      TotalFlow   : REAL;  // Forward cumulative flow
      Temperature : REAL;  // Medium temperature, set 0.0 if unavailable
      Reserved    : WORD;  // Padding to 16 bytes total
   END_STRUCT;
END_TYPE
```

## 3. UDT for Modbus raw data

From manual (holding registers):
- 90/91  : Forward total flow (float)
- 92/93  : Reverse total flow (float)
- 94/95  : Net total flow (float)
- 96/97  : Clear command (DW)
- 98/99  : Instantaneous flow (float)
- 100/101: Velocity (float)
- 102/103: Flow percent (float)
- 104    : Empty pipe percent (word)
- 105    : Flow unit code (word)
- 106    : Empty pipe alarm (word)
- 107    : Excitation alarm (word)

```pascal
TYPE "UDT_WTS_FlowMeter_ModbusRaw"
VERSION : 0.1
   STRUCT
      Reg90_ForwardTotal_Hi : WORD;
      Reg91_ForwardTotal_Lo : WORD;
      Reg92_ReverseTotal_Hi : WORD;
      Reg93_ReverseTotal_Lo : WORD;
      Reg94_NetTotal_Hi     : WORD;
      Reg95_NetTotal_Lo     : WORD;
      Reg96_ClearCmd_Hi     : WORD;
      Reg97_ClearCmd_Lo     : WORD;
      Reg98_Rate_Hi         : WORD;
      Reg99_Rate_Lo         : WORD;
      Reg100_Velocity_Hi    : WORD;
      Reg101_Velocity_Lo    : WORD;
      Reg102_Percent_Hi     : WORD;
      Reg103_Percent_Lo     : WORD;
      Reg104_EmptyPipePct   : WORD;
      Reg105_UnitCode       : WORD;
      Reg106_EmptyPipeAlarm : WORD;
      Reg107_ExciteAlarm    : WORD;
   END_STRUCT;
END_TYPE
```

## 4. Recommended DB layout

### 4.1 DB2 for PC/HMI readback (compatible with current software)

```pascal
DATA_BLOCK "DB2_FlowMeters_HMI"
{ S7_Optimized_Access := 'FALSE' }
VERSION : 0.1
   VAR
      FM : ARRAY[1..4] OF "UDT_WTS_FlowMeter_HMI";
   END_VAR
BEGIN
END_DATA_BLOCK
```

Address map (non-optimized):
- FM[1]: DB2.DBW0 / DBD2 / DBD6 / DBD10
- FM[2]: starts at DB2.DBW16
- FM[3]: starts at DB2.DBW32
- FM[4]: starts at DB2.DBW48

### 4.2 Optional DB for protocol diagnostics

```pascal
DATA_BLOCK "DB_FlowMeter_Raw"
{ S7_Optimized_Access := 'TRUE' }
VERSION : 0.1
   VAR
      FM1_Raw : "UDT_WTS_FlowMeter_ModbusRaw";
   END_VAR
BEGIN
END_DATA_BLOCK
```

## 5. Mapping rule from Modbus to HMI UDT

For FM1 (address 1):
- Status      := 1 when Modbus polling OK, else 0 or 2 on fault.
- FlowRate    := FLOAT(Reg98, Reg99).
- TotalFlow   := FLOAT(Reg90, Reg91).  // Forward cumulative
- Temperature := 0.0 (if meter does not provide temperature register).

## 6. Implementation notes in TIA

1. For DB2 used by PC, set S7_Optimized_Access = FALSE.
2. Keep field order unchanged.
3. If unit code Reg105 is not L/min, convert in PLC before writing FlowRate, or document scaling clearly.
4. If using MB_CLIENT, keep timeout and retry counters; set Status=FAULT on repeated failures.

## 7. Acceptance checklist

1. PC can read FM1 FlowRate and TotalFlow from DB2 and values change with process.
2. FM1 stride is exactly 16 bytes in DB2.
3. Modbus fail case updates Status and does not freeze stale values forever.
4. Unit used in PLC output is documented and consistent with UI.
