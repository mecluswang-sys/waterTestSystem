import { X, Play, Square, Settings2 } from 'lucide-react';
import { SystemData } from '../App';
import { useState } from 'react';

interface BottomPanelProps {
  selectedDevice: string | null;
  systemData: SystemData;
  onCommand: (deviceId: string, command: string, value?: number) => void;
  onClose: () => void;
}

export function BottomPanel({ selectedDevice, systemData, onCommand, onClose }: BottomPanelProps) {
  const [frequencyInput, setFrequencyInput] = useState('');
  const [positionInput, setPositionInput] = useState('');

  if (!selectedDevice) {
    return (
      <div className="bg-[#1a1a1a] border-t-2 border-[#444] px-4 py-3 shrink-0">
        <div className="text-xs text-gray-500 text-center font-mono">
          NO DEVICE SELECTED - CLICK ON A COMPONENT TO VIEW DETAILS
        </div>
      </div>
    );
  }

  // Get device data
  let deviceData: any = null;
  let deviceType = '';

  if (selectedDevice.startsWith('pump')) {
    deviceData = systemData.pumps[selectedDevice as 'pump1' | 'pump2'];
    deviceType = 'PUMP';
  } else if (selectedDevice.startsWith('valve')) {
    deviceData = systemData.valves[selectedDevice as 'valve1' | 'valve2'];
    deviceType = 'VALVE';
  } else if (selectedDevice.startsWith('pt') || selectedDevice.startsWith('tt')) {
    const sensors = systemData.sensors;
    deviceData = sensors[selectedDevice as keyof typeof sensors];
    deviceType = 'SENSOR';
  } else if (selectedDevice.startsWith('psv')) {
    deviceData = systemData.safetyValves[selectedDevice as 'psv101' | 'psv102'];
    deviceType = 'SAFETY VALVE';
  } else if (selectedDevice === 'tank1') {
    deviceData = systemData.tank;
    deviceType = 'TANK';
  }

  if (!deviceData) return null;

  const renderControls = () => {
    if (deviceType === 'PUMP') {
      return (
        <div className="flex items-center gap-4">
          <div className="flex gap-2">
            <button
              onClick={() => onCommand(selectedDevice, 'start')}
              disabled={deviceData.running}
              className={`flex items-center gap-2 px-4 py-2 text-sm font-bold transition-colors ${
                deviceData.running
                  ? 'bg-[#333] text-gray-600 cursor-not-allowed'
                  : 'bg-[#0a0] text-black hover:bg-[#0c0]'
              }`}
            >
              <Play className="w-4 h-4" />
              START
            </button>
            <button
              onClick={() => onCommand(selectedDevice, 'stop')}
              disabled={!deviceData.running}
              className={`flex items-center gap-2 px-4 py-2 text-sm font-bold transition-colors ${
                !deviceData.running
                  ? 'bg-[#333] text-gray-600 cursor-not-allowed'
                  : 'bg-[#a00] text-white hover:bg-[#c00]'
              }`}
            >
              <Square className="w-4 h-4" />
              STOP
            </button>
          </div>

          <div className="w-px h-12 bg-[#444]" />

          <div className="flex items-center gap-2">
            <label className="text-xs text-gray-400 font-mono">FREQUENCY (Hz):</label>
            <input
              type="number"
              value={frequencyInput}
              onChange={(e) => setFrequencyInput(e.target.value)}
              placeholder={deviceData.setpoint.toFixed(1)}
              className="w-20 px-2 py-1 bg-[#2a2a2a] border border-[#444] text-white font-mono text-sm"
              min="0"
              max="60"
              step="0.1"
            />
            <button
              onClick={() => {
                const val = parseFloat(frequencyInput);
                if (!isNaN(val) && val >= 0 && val <= 60) {
                  onCommand(selectedDevice, 'setFrequency', val);
                  setFrequencyInput('');
                }
              }}
              className="px-3 py-1 bg-[#0a0] text-black text-xs font-bold hover:bg-[#0c0]"
            >
              SET
            </button>
          </div>
        </div>
      );
    }

    if (deviceType === 'VALVE') {
      return (
        <div className="flex items-center gap-4">
          <div className="flex gap-2">
            <button
              onClick={() => onCommand(selectedDevice, 'open')}
              disabled={deviceData.open && deviceData.position >= 95}
              className={`px-4 py-2 text-sm font-bold transition-colors ${
                deviceData.open && deviceData.position >= 95
                  ? 'bg-[#333] text-gray-600 cursor-not-allowed'
                  : 'bg-[#0a0] text-black hover:bg-[#0c0]'
              }`}
            >
              OPEN
            </button>
            <button
              onClick={() => onCommand(selectedDevice, 'close')}
              disabled={!deviceData.open && deviceData.position <= 5}
              className={`px-4 py-2 text-sm font-bold transition-colors ${
                !deviceData.open && deviceData.position <= 5
                  ? 'bg-[#333] text-gray-600 cursor-not-allowed'
                  : 'bg-[#a00] text-white hover:bg-[#c00]'
              }`}
            >
              CLOSE
            </button>
          </div>

          <div className="w-px h-12 bg-[#444]" />

          <div className="flex items-center gap-2">
            <label className="text-xs text-gray-400 font-mono">POSITION (%):</label>
            <input
              type="number"
              value={positionInput}
              onChange={(e) => setPositionInput(e.target.value)}
              placeholder={deviceData.command.toString()}
              className="w-20 px-2 py-1 bg-[#2a2a2a] border border-[#444] text-white font-mono text-sm"
              min="0"
              max="100"
            />
            <button
              onClick={() => {
                const val = parseInt(positionInput);
                if (!isNaN(val) && val >= 0 && val <= 100) {
                  onCommand(selectedDevice, 'setPosition', val);
                  setPositionInput('');
                }
              }}
              className="px-3 py-1 bg-[#0a0] text-black text-xs font-bold hover:bg-[#0c0]"
            >
              SET
            </button>
          </div>
        </div>
      );
    }

    return (
      <div className="text-xs text-gray-500 font-mono">
        READ-ONLY DEVICE
      </div>
    );
  };

  const renderParameters = () => {
    if (deviceType === 'PUMP') {
      return (
        <>
          <ParameterDisplay label="FREQUENCY" value={`${deviceData.frequency.toFixed(1)} Hz`} color="#0af" />
          <ParameterDisplay label="CURRENT" value={`${deviceData.current.toFixed(1)} A`} color="#fa0" />
          <ParameterDisplay label="POWER" value={`${deviceData.power.toFixed(1)} kW`} color="#f0a" />
          <ParameterDisplay label="SETPOINT" value={`${deviceData.setpoint.toFixed(1)} Hz`} color="#aaa" />
          <ParameterDisplay 
            label="STATUS" 
            value={deviceData.fault ? 'FAULT' : deviceData.running ? 'RUNNING' : 'STOPPED'} 
            color={deviceData.fault ? '#f00' : deviceData.running ? '#0f0' : '#666'} 
          />
        </>
      );
    }

    if (deviceType === 'VALVE') {
      return (
        <>
          <ParameterDisplay label="POSITION" value={`${deviceData.position} %`} color="#0af" />
          <ParameterDisplay label="FEEDBACK" value={`${deviceData.feedback} %`} color="#fa0" />
          <ParameterDisplay label="COMMAND" value={`${deviceData.command} %`} color="#aaa" />
          <ParameterDisplay 
            label="STATUS" 
            value={deviceData.fault ? 'FAULT' : deviceData.open ? 'OPEN' : 'CLOSED'} 
            color={deviceData.fault ? '#f00' : deviceData.open ? '#0f0' : '#666'} 
          />
        </>
      );
    }

    if (deviceType === 'SENSOR') {
      return (
        <>
          <ParameterDisplay label="VALUE" value={`${deviceData.value.toFixed(2)} ${deviceData.unit}`} color="#0af" />
          <ParameterDisplay label="HIGH LIMIT" value={`${deviceData.alarmHigh} ${deviceData.unit}`} color="#f60" />
          <ParameterDisplay label="LOW LIMIT" value={`${deviceData.alarmLow} ${deviceData.unit}`} color="#06f" />
          <ParameterDisplay 
            label="STATUS" 
            value={deviceData.alarm ? 'ALARM' : 'NORMAL'} 
            color={deviceData.alarm ? '#f00' : '#0f0'} 
          />
        </>
      );
    }

    if (deviceType === 'SAFETY VALVE') {
      return (
        <>
          <ParameterDisplay label="SET POINT" value={`${deviceData.setPoint} bar`} color="#fa0" />
          <ParameterDisplay 
            label="STATUS" 
            value={deviceData.triggered ? 'TRIGGERED' : 'NORMAL'} 
            color={deviceData.triggered ? '#f00' : '#0f0'} 
          />
        </>
      );
    }

    if (deviceType === 'TANK') {
      return (
        <>
          <ParameterDisplay label="LEVEL" value={`${deviceData.level.toFixed(1)} %`} color="#0af" />
          <ParameterDisplay label="PRESSURE" value={`${deviceData.pressure.toFixed(2)} bar`} color="#f0a" />
          <ParameterDisplay label="TEMPERATURE" value={`${deviceData.temperature.toFixed(1)} °C`} color="#f60" />
          <ParameterDisplay 
            label="LEVEL ALARM" 
            value={deviceData.levelAlarmHigh || deviceData.levelAlarmLow ? 'ACTIVE' : 'NORMAL'} 
            color={deviceData.levelAlarmHigh || deviceData.levelAlarmLow ? '#f00' : '#0f0'} 
          />
        </>
      );
    }

    return null;
  };

  return (
    <div className="bg-[#1a1a1a] border-t-2 border-[#444] px-4 py-3 shrink-0">
      <div className="flex items-start justify-between">
        {/* Device Info */}
        <div className="flex items-center gap-4">
          <Settings2 className="w-5 h-5 text-[#0af]" />
          <div>
            <div className="flex items-center gap-2">
              <span className="text-sm font-bold text-[#0af]">{deviceData.name}</span>
              <span className="text-[10px] px-2 py-0.5 bg-[#2a2a2a] border border-[#444] text-gray-400">
                {deviceType}
              </span>
              <span className={`text-[10px] px-2 py-0.5 border font-bold ${
                deviceData.quality === 'good' ? 'bg-[#0a0]/20 border-[#0a0] text-[#0f0]' : 'bg-[#a00]/20 border-[#a00] text-[#f00]'
              }`}>
                {deviceData.quality.toUpperCase()}
              </span>
            </div>
            <div className="text-[10px] text-gray-500 font-mono mt-0.5">
              ID: {selectedDevice.toUpperCase()}
            </div>
          </div>
        </div>

        {/* Parameters */}
        <div className="flex-1 mx-8">
          <div className="grid grid-cols-5 gap-3">
            {renderParameters()}
          </div>
        </div>

        {/* Controls */}
        <div className="flex items-center gap-4">
          {renderControls()}
          <button
            onClick={onClose}
            className="p-2 hover:bg-[#333] border border-[#444] transition-colors ml-2"
          >
            <X className="w-4 h-4 text-gray-400" />
          </button>
        </div>
      </div>
    </div>
  );
}

function ParameterDisplay({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div className="bg-[#2a2a2a] border border-[#444] px-2 py-1.5">
      <div className="text-[9px] text-gray-500 font-mono mb-0.5">{label}</div>
      <div className="text-sm font-bold font-mono" style={{ color }}>
        {value}
      </div>
    </div>
  );
}
