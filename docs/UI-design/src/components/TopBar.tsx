import { AlertTriangle, Wifi, WifiOff } from 'lucide-react';

interface Alarm {
  id: string;
  severity: 'critical' | 'warning' | 'info';
  message: string;
  timestamp: number;
  acknowledged: boolean;
}

interface TopBarProps {
  mode: 'auto' | 'manual';
  plcConnected: boolean;
  alarms: Alarm[];
  onModeChange: (mode: 'auto' | 'manual') => void;
}

export function TopBar({ mode, plcConnected, alarms, onModeChange }: TopBarProps) {
  const criticalAlarms = alarms.filter(a => a.severity === 'critical' && !a.acknowledged).length;
  const warningAlarms = alarms.filter(a => a.severity === 'warning' && !a.acknowledged).length;
  const totalAlarms = criticalAlarms + warningAlarms;

  return (
    <div className="bg-[#1a1a1a] border-b-2 border-[#444] px-4 py-2.5 flex items-center justify-between shrink-0">
      {/* Left Section */}
      <div className="flex items-center gap-6">
        <div className="text-sm font-bold tracking-wider text-[#0f0]">
          SCADA SYSTEM
        </div>

        <div className="flex items-center gap-2 bg-[#2a2a2a] border border-[#444] px-3 py-1">
          <span className="text-xs text-gray-400 font-mono">MODE:</span>
          <button
            onClick={() => onModeChange('auto')}
            className={`px-3 py-0.5 text-xs font-bold transition-colors ${
              mode === 'auto'
                ? 'bg-[#0a0] text-black'
                : 'bg-[#333] text-gray-500'
            }`}
          >
            AUTO
          </button>
          <button
            onClick={() => onModeChange('manual')}
            className={`px-3 py-0.5 text-xs font-bold transition-colors ${
              mode === 'manual'
                ? 'bg-[#fa0] text-black'
                : 'bg-[#333] text-gray-500'
            }`}
          >
            MANUAL
          </button>
        </div>

        <div className={`flex items-center gap-2 px-3 py-1 border ${
          plcConnected ? 'bg-[#0a0]/20 border-[#0a0]' : 'bg-[#a00]/20 border-[#a00]'
        }`}>
          {plcConnected ? (
            <Wifi className="w-4 h-4 text-[#0f0]" />
          ) : (
            <WifiOff className="w-4 h-4 text-[#f00]" />
          )}
          <span className={`text-xs font-bold ${plcConnected ? 'text-[#0f0]' : 'text-[#f00]'}`}>
            PLC {plcConnected ? 'ONLINE' : 'OFFLINE'}
          </span>
        </div>
      </div>

      {/* Center Section */}
      <div className="text-center">
        <div className="text-[10px] text-gray-500 font-mono">SYSTEM TIME</div>
        <div className="text-sm font-bold font-mono">
          {new Date().toLocaleString('en-US', {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: false,
          })}
        </div>
      </div>

      {/* Right Section - Alarms */}
      <div className="flex items-center gap-4">
        {totalAlarms > 0 && (
          <div className={`flex items-center gap-2 px-4 py-1.5 border-2 ${
            criticalAlarms > 0
              ? 'bg-[#a00] border-[#f00] animate-pulse'
              : 'bg-[#a60] border-[#fa0]'
          }`}>
            <AlertTriangle className="w-5 h-5 text-white" />
            <div>
              <div className="text-[10px] text-gray-200">ACTIVE ALARMS</div>
              <div className="text-xl font-bold text-white font-mono">
                {totalAlarms}
              </div>
            </div>
          </div>
        )}

        {totalAlarms === 0 && (
          <div className="flex items-center gap-2 px-4 py-1.5 bg-[#2a2a2a] border border-[#444]">
            <div className="w-2 h-2 bg-[#0f0] rounded-full" />
            <span className="text-xs text-gray-400 font-mono">NO ALARMS</span>
          </div>
        )}
      </div>
    </div>
  );
}
