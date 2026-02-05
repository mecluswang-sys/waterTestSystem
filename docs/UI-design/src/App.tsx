import { useState, useEffect } from 'react';
import { TopBar } from './components/TopBar';
import { BottomPanel } from './components/BottomPanel';
import { OverlayLayer } from './components/OverlayLayer';
import { PumpComponent } from './components/PumpComponent';
import { ValveComponent } from './components/ValveComponent';
import { SensorComponent } from './components/SensorComponent';
import { SafetyValveComponent } from './components/SafetyValveComponent';
import { TankComponent } from './components/TankComponent';
import { PipeComponent } from './components/PipeComponent';
import { JunctionComponent } from './components/JunctionComponent';

// Types matching C++ PLC data structures
export interface PLCSignal {
  timestamp: number;
  quality: 'good' | 'bad' | 'uncertain';
}

export interface PumpData extends PLCSignal {
  id: string;
  name: string;
  running: boolean;
  fault: boolean;
  frequency: number;      // Hz
  current: number;        // Amperes
  power: number;          // kW
  setpoint: number;       // Hz
}

export interface ValveData extends PLCSignal {
  id: string;
  name: string;
  open: boolean;
  fault: boolean;
  position: number;       // 0-100%
  feedback: number;       // 0-100%
  command: number;        // 0-100%
}

export interface SensorData extends PLCSignal {
  id: string;
  name: string;
  type: 'pressure' | 'temperature';
  value: number;
  unit: string;
  alarm: boolean;
  alarmHigh: number;
  alarmLow: number;
}

export interface SafetyValveData extends PLCSignal {
  id: string;
  name: string;
  triggered: boolean;
  setPoint: number;
}

export interface TankData extends PLCSignal {
  id: string;
  name: string;
  level: number;          // 0-100%
  pressure: number;       // bar
  temperature: number;    // °C
  levelAlarmHigh: boolean;
  levelAlarmLow: boolean;
}

export interface SystemData {
  mode: 'auto' | 'manual';
  plcConnected: boolean;
  alarms: Array<{
    id: string;
    severity: 'critical' | 'warning' | 'info';
    message: string;
    timestamp: number;
    acknowledged: boolean;
  }>;
  pumps: { pump1: PumpData; pump2: PumpData };
  valves: { valve1: ValveData; valve2: ValveData };
  sensors: {
    pt101: SensorData;
    tt101: SensorData;
    pt102: SensorData;
    tt102: SensorData;
    pt103: SensorData;
    tt103: SensorData;
  };
  safetyValves: { psv101: SafetyValveData; psv102: SafetyValveData };
  tank: TankData;
}

export default function App() {
  const [selectedDevice, setSelectedDevice] = useState<string | null>(null);
  const [systemData, setSystemData] = useState<SystemData>({
    mode: 'auto',
    plcConnected: true,
    alarms: [],
    pumps: {
      pump1: {
        id: 'pump1',
        name: 'P-101',
        running: true,
        fault: false,
        frequency: 45.2,
        current: 12.5,
        power: 8.3,
        setpoint: 45.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      pump2: {
        id: 'pump2',
        name: 'P-102',
        running: true,
        fault: false,
        frequency: 42.8,
        current: 11.8,
        power: 7.9,
        setpoint: 43.0,
        timestamp: Date.now(),
        quality: 'good',
      },
    },
    valves: {
      valve1: {
        id: 'valve1',
        name: 'MV-101',
        open: true,
        fault: false,
        position: 85,
        feedback: 85,
        command: 85,
        timestamp: Date.now(),
        quality: 'good',
      },
      valve2: {
        id: 'valve2',
        name: 'MV-102',
        open: true,
        fault: false,
        position: 72,
        feedback: 72,
        command: 72,
        timestamp: Date.now(),
        quality: 'good',
      },
    },
    sensors: {
      pt101: {
        id: 'pt101',
        name: 'PT-101',
        type: 'pressure',
        value: 4.23,
        unit: 'bar',
        alarm: false,
        alarmHigh: 6.0,
        alarmLow: 2.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      tt101: {
        id: 'tt101',
        name: 'TT-101',
        type: 'temperature',
        value: 22.5,
        unit: '°C',
        alarm: false,
        alarmHigh: 40.0,
        alarmLow: 10.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      pt102: {
        id: 'pt102',
        name: 'PT-102',
        type: 'pressure',
        value: 3.87,
        unit: 'bar',
        alarm: false,
        alarmHigh: 6.0,
        alarmLow: 2.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      tt102: {
        id: 'tt102',
        name: 'TT-102',
        type: 'temperature',
        value: 23.1,
        unit: '°C',
        alarm: false,
        alarmHigh: 40.0,
        alarmLow: 10.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      pt103: {
        id: 'pt103',
        name: 'PT-103',
        type: 'pressure',
        value: 2.15,
        unit: 'bar',
        alarm: false,
        alarmHigh: 5.0,
        alarmLow: 1.0,
        timestamp: Date.now(),
        quality: 'good',
      },
      tt103: {
        id: 'tt103',
        name: 'TT-103',
        type: 'temperature',
        value: 22.8,
        unit: '°C',
        alarm: false,
        alarmHigh: 40.0,
        alarmLow: 10.0,
        timestamp: Date.now(),
        quality: 'good',
      },
    },
    safetyValves: {
      psv101: {
        id: 'psv101',
        name: 'PSV-101',
        triggered: false,
        setPoint: 6.5,
        timestamp: Date.now(),
        quality: 'good',
      },
      psv102: {
        id: 'psv102',
        name: 'PSV-102',
        triggered: false,
        setPoint: 6.5,
        timestamp: Date.now(),
        quality: 'good',
      },
    },
    tank: {
      id: 'tank1',
      name: 'TK-201',
      level: 67.3,
      pressure: 2.15,
      temperature: 22.8,
      levelAlarmHigh: false,
      levelAlarmLow: false,
      timestamp: Date.now(),
      quality: 'good',
    },
  });

  // Simulate PLC data updates
  useEffect(() => {
    const interval = setInterval(() => {
      setSystemData(prev => ({
        ...prev,
        pumps: {
          pump1: {
            ...prev.pumps.pump1,
            frequency: prev.pumps.pump1.setpoint + (Math.random() - 0.5) * 0.5,
            current: 12.5 + (Math.random() - 0.5) * 0.3,
            power: 8.3 + (Math.random() - 0.5) * 0.2,
            timestamp: Date.now(),
          },
          pump2: {
            ...prev.pumps.pump2,
            frequency: prev.pumps.pump2.setpoint + (Math.random() - 0.5) * 0.5,
            current: 11.8 + (Math.random() - 0.5) * 0.3,
            power: 7.9 + (Math.random() - 0.5) * 0.2,
            timestamp: Date.now(),
          },
        },
        sensors: {
          pt101: { ...prev.sensors.pt101, value: 4.23 + (Math.random() - 0.5) * 0.15, timestamp: Date.now() },
          tt101: { ...prev.sensors.tt101, value: 22.5 + (Math.random() - 0.5) * 0.2, timestamp: Date.now() },
          pt102: { ...prev.sensors.pt102, value: 3.87 + (Math.random() - 0.5) * 0.12, timestamp: Date.now() },
          tt102: { ...prev.sensors.tt102, value: 23.1 + (Math.random() - 0.5) * 0.2, timestamp: Date.now() },
          pt103: { ...prev.sensors.pt103, value: 2.15 + (Math.random() - 0.5) * 0.08, timestamp: Date.now() },
          tt103: { ...prev.sensors.tt103, value: 22.8 + (Math.random() - 0.5) * 0.15, timestamp: Date.now() },
        },
        tank: {
          ...prev.tank,
          level: Math.max(60, Math.min(75, prev.tank.level + (Math.random() - 0.5) * 0.2)),
          pressure: 2.15 + (Math.random() - 0.5) * 0.08,
          temperature: 22.8 + (Math.random() - 0.5) * 0.15,
          timestamp: Date.now(),
        },
      }));
    }, 1000);

    return () => clearInterval(interval);
  }, []);

  const handleDeviceSelect = (deviceId: string) => {
    setSelectedDevice(deviceId === selectedDevice ? null : deviceId);
  };

  const handleCommand = (deviceId: string, command: string, value?: number) => {
    console.log(`PLC Command: ${deviceId} - ${command}`, value);
    
    // Simulate sending command to PLC
    setSystemData(prev => {
      const updated = { ...prev };
      
      if (deviceId.startsWith('pump')) {
        const pumpKey = deviceId as 'pump1' | 'pump2';
        if (command === 'start') {
          updated.pumps[pumpKey] = { ...updated.pumps[pumpKey], running: true };
        } else if (command === 'stop') {
          updated.pumps[pumpKey] = { ...updated.pumps[pumpKey], running: false };
        } else if (command === 'setFrequency' && value !== undefined) {
          updated.pumps[pumpKey] = { ...updated.pumps[pumpKey], setpoint: value };
        }
      } else if (deviceId.startsWith('valve')) {
        const valveKey = deviceId as 'valve1' | 'valve2';
        if (command === 'open') {
          updated.valves[valveKey] = { ...updated.valves[valveKey], open: true, command: 100 };
        } else if (command === 'close') {
          updated.valves[valveKey] = { ...updated.valves[valveKey], open: false, command: 0 };
        } else if (command === 'setPosition' && value !== undefined) {
          updated.valves[valveKey] = { ...updated.valves[valveKey], command: value };
        }
      }
      
      return updated;
    });
  };

  const handleModeChange = (mode: 'auto' | 'manual') => {
    setSystemData(prev => ({ ...prev, mode }));
  };

  const handleAlarmAcknowledge = (alarmId: string) => {
    setSystemData(prev => ({
      ...prev,
      alarms: prev.alarms.map(a => 
        a.id === alarmId ? { ...a, acknowledged: true } : a
      ),
    }));
  };

  return (
    <div className="h-screen bg-[#2a2a2a] text-white flex flex-col overflow-hidden">
      {/* Top Bar - Fixed */}
      <TopBar
        mode={systemData.mode}
        plcConnected={systemData.plcConnected}
        alarms={systemData.alarms}
        onModeChange={handleModeChange}
      />

      {/* Main Process View - QGraphicsView equivalent */}
      <div className="flex-1 overflow-auto">
        <div className="relative min-w-[1400px] min-h-[800px] bg-[#353535] p-8">
          {/* Background Grid */}
          <div 
            className="absolute inset-0 opacity-5"
            style={{
              backgroundImage: 'linear-gradient(#fff 1px, transparent 1px), linear-gradient(90deg, #fff 1px, transparent 1px)',
              backgroundSize: '20px 20px'
            }}
          />

          {/* Process Title */}
          <div className="absolute top-4 left-4 text-sm font-bold text-[#0f0] tracking-widest">
            FLUID DISTRIBUTION CONTROL SYSTEM
          </div>

          {/* PUMP 1 */}
          <div className="absolute" style={{ left: '60px', top: '150px' }}>
            <PumpComponent
              data={systemData.pumps.pump1}
              selected={selectedDevice === 'pump1'}
              onSelect={() => handleDeviceSelect('pump1')}
            />
          </div>

          {/* Pipe: Pump1 to Valve1 */}
          <PipeComponent
            x={180}
            y={180}
            length={80}
            direction="horizontal"
            flowing={systemData.pumps.pump1.running}
          />

          {/* VALVE 1 */}
          <div className="absolute" style={{ left: '270px', top: '160px' }}>
            <ValveComponent
              data={systemData.valves.valve1}
              selected={selectedDevice === 'valve1'}
              onSelect={() => handleDeviceSelect('valve1')}
            />
          </div>

          {/* Pipe: Valve1 to Sensor branch */}
          <PipeComponent
            x={340}
            y={180}
            length={100}
            direction="horizontal"
            flowing={systemData.pumps.pump1.running && systemData.valves.valve1.open}
          />

          {/* SENSOR PT-101 */}
          <div className="absolute" style={{ left: '450px', top: '140px' }}>
            <SensorComponent
              data={systemData.sensors.pt101}
              selected={selectedDevice === 'pt101'}
              onSelect={() => handleDeviceSelect('pt101')}
            />
          </div>

          {/* SENSOR TT-101 */}
          <div className="absolute" style={{ left: '550px', top: '140px' }}>
            <SensorComponent
              data={systemData.sensors.tt101}
              selected={selectedDevice === 'tt101'}
              onSelect={() => handleDeviceSelect('tt101')}
            />
          </div>

          {/* Pipe: Sensors to Safety Valve */}
          <PipeComponent
            x={650}
            y={180}
            length={60}
            direction="horizontal"
            flowing={systemData.pumps.pump1.running && systemData.valves.valve1.open}
          />

          {/* SAFETY VALVE PSV-101 */}
          <div className="absolute" style={{ left: '720px', top: '145px' }}>
            <SafetyValveComponent
              data={systemData.safetyValves.psv101}
              selected={selectedDevice === 'psv101'}
              onSelect={() => handleDeviceSelect('psv101')}
            />
          </div>

          {/* Pipe: Continue to junction */}
          <PipeComponent
            x={770}
            y={180}
            length={70}
            direction="horizontal"
            flowing={systemData.pumps.pump1.running && systemData.valves.valve1.open}
          />

          {/* Pipe: Vertical down to junction */}
          <PipeComponent
            x={840}
            y={180}
            length={160}
            direction="vertical"
            flowing={systemData.pumps.pump1.running && systemData.valves.valve1.open}
          />

          {/* PUMP 2 */}
          <div className="absolute" style={{ left: '60px', top: '400px' }}>
            <PumpComponent
              data={systemData.pumps.pump2}
              selected={selectedDevice === 'pump2'}
              onSelect={() => handleDeviceSelect('pump2')}
            />
          </div>

          {/* Pipe: Pump2 to Valve2 */}
          <PipeComponent
            x={180}
            y={430}
            length={80}
            direction="horizontal"
            flowing={systemData.pumps.pump2.running}
          />

          {/* VALVE 2 */}
          <div className="absolute" style={{ left: '270px', top: '410px' }}>
            <ValveComponent
              data={systemData.valves.valve2}
              selected={selectedDevice === 'valve2'}
              onSelect={() => handleDeviceSelect('valve2')}
            />
          </div>

          {/* Pipe: Valve2 to Sensor branch */}
          <PipeComponent
            x={340}
            y={430}
            length={100}
            direction="horizontal"
            flowing={systemData.pumps.pump2.running && systemData.valves.valve2.open}
          />

          {/* SENSOR PT-102 */}
          <div className="absolute" style={{ left: '450px', top: '390px' }}>
            <SensorComponent
              data={systemData.sensors.pt102}
              selected={selectedDevice === 'pt102'}
              onSelect={() => handleDeviceSelect('pt102')}
            />
          </div>

          {/* SENSOR TT-102 */}
          <div className="absolute" style={{ left: '550px', top: '390px' }}>
            <SensorComponent
              data={systemData.sensors.tt102}
              selected={selectedDevice === 'tt102'}
              onSelect={() => handleDeviceSelect('tt102')}
            />
          </div>

          {/* Pipe: Sensors to Safety Valve */}
          <PipeComponent
            x={650}
            y={430}
            length={60}
            direction="horizontal"
            flowing={systemData.pumps.pump2.running && systemData.valves.valve2.open}
          />

          {/* SAFETY VALVE PSV-102 */}
          <div className="absolute" style={{ left: '720px', top: '395px' }}>
            <SafetyValveComponent
              data={systemData.safetyValves.psv102}
              selected={selectedDevice === 'psv102'}
              onSelect={() => handleDeviceSelect('psv102')}
            />
          </div>

          {/* Pipe: Continue to junction */}
          <PipeComponent
            x={770}
            y={430}
            length={70}
            direction="horizontal"
            flowing={systemData.pumps.pump2.running && systemData.valves.valve2.open}
          />

          {/* Pipe: Vertical up to junction */}
          <PipeComponent
            x={840}
            y={340}
            length={90}
            direction="vertical"
            flowing={systemData.pumps.pump2.running && systemData.valves.valve2.open}
          />

          {/* JUNCTION */}
          <div className="absolute" style={{ left: '834px', top: '334px' }}>
            <JunctionComponent
              flowing={
                (systemData.pumps.pump1.running && systemData.valves.valve1.open) ||
                (systemData.pumps.pump2.running && systemData.valves.valve2.open)
              }
            />
          </div>

          {/* Pipe: Junction to Tank */}
          <PipeComponent
            x={852}
            y={340}
            length={120}
            direction="horizontal"
            flowing={
              (systemData.pumps.pump1.running && systemData.valves.valve1.open) ||
              (systemData.pumps.pump2.running && systemData.valves.valve2.open)
            }
          />

          {/* SENSOR PT-103 (Tank inlet) */}
          <div className="absolute" style={{ left: '900px', top: '300px' }}>
            <SensorComponent
              data={systemData.sensors.pt103}
              selected={selectedDevice === 'pt103'}
              onSelect={() => handleDeviceSelect('pt103')}
            />
          </div>

          {/* SENSOR TT-103 (Tank inlet) */}
          <div className="absolute" style={{ left: '900px', top: '360px' }}>
            <SensorComponent
              data={systemData.sensors.tt103}
              selected={selectedDevice === 'tt103'}
              onSelect={() => handleDeviceSelect('tt103')}
            />
          </div>

          {/* TANK */}
          <div className="absolute" style={{ left: '980px', top: '200px' }}>
            <TankComponent
              data={systemData.tank}
              selected={selectedDevice === 'tank1'}
              onSelect={() => handleDeviceSelect('tank1')}
            />
          </div>
        </div>
      </div>

      {/* Bottom Panel - Fixed */}
      <BottomPanel
        selectedDevice={selectedDevice}
        systemData={systemData}
        onCommand={handleCommand}
        onClose={() => setSelectedDevice(null)}
      />

      {/* Overlay Layer - Alarms & Notifications */}
      <OverlayLayer
        alarms={systemData.alarms}
        onAcknowledge={handleAlarmAcknowledge}
      />
    </div>
  );
}
