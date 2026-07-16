# WEEKLY REPORT - 2026-07-16

## Scope

This report summarizes the current uncommitted functional changes that were integrated and validated this week.

## Functional Changes

1. PLC self-check logic simplification in DeviceManager.
- Extracted common DB bit/address helpers to reduce duplicated command/status IO logic.
- Refactored self-check status readback into field-driven loops for BOOL and REAL groups.
- Preserved existing config keys and external behavior for start/abort/reset/enable paths.

2. Regulating valve data path hardening.
- Added write-readback behavior for valve opening command path.
- Added AO/AI read diagnostics to improve PLC mapping troubleshooting.
- Kept minimal model semantics based on openingSetpoint/openingPercent.

3. Monitor panel pressure display correction.
- Fixed realtime monitor pressure display to use raw kPa values.
- Removed unintended kPa-to-kgf/cm2 conversion in display path.
- Updated pressure UI debug log fields to reflect kPa display.

4. Monitor panel valve area usability update.
- Renamed section to include regulating valves explicitly.
- Moved regulating valves to top rows for better visibility during commissioning.

5. Station1 pressure source mapping alignment.
- Simplified local pressure sensor ID resolution.
- Expanded remote pressure index usage from 4 channels to 6 channels for Pressure3..Pressure8 mapping.
- Updated display read path to prefer local PLC values when available and fallback to station data.

6. Configuration alignment.
- Updated pressure sensor count to 8.
- Updated SensorKpa base offset mapping under db.sensor.kpa.base_offset.

7. README clarification.
- Added explicit pipeline sequence description for station pressure/valve/flow path.

## Files Included

- README.md
- config/system.conf
- src/DeviceManager.cpp
- src/gui/MonitorPanel.cpp
- src/gui/Station1Panel.cpp

## Validation

- Build target: WaterTestSystem (Debug)
- Result: Build succeeded via CMake/MSBuild in local build directory.

## Notes

- This report records the functional baseline submitted in the current commit.
