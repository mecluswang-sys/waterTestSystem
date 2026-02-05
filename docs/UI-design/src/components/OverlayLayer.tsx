import { AlertTriangle, X, Bell } from 'lucide-react';

interface Alarm {
  id: string;
  severity: 'critical' | 'warning' | 'info';
  message: string;
  timestamp: number;
  acknowledged: boolean;
}

interface OverlayLayerProps {
  alarms: Alarm[];
  onAcknowledge: (alarmId: string) => void;
}

export function OverlayLayer({ alarms, onAcknowledge }: OverlayLayerProps) {
  const activeAlarms = alarms.filter(a => !a.acknowledged);

  if (activeAlarms.length === 0) return null;

  return (
    <div className="fixed top-16 right-4 z-50 w-96 max-h-[500px] overflow-y-auto">
      <div className="bg-[#1a1a1a] border-2 border-[#fa0] shadow-2xl">
        <div className="bg-[#fa0] text-black px-4 py-2 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Bell className="w-5 h-5" />
            <span className="font-bold text-sm">ACTIVE ALARMS ({activeAlarms.length})</span>
          </div>
        </div>

        <div className="divide-y divide-[#444]">
          {activeAlarms.map(alarm => (
            <div
              key={alarm.id}
              className={`p-3 ${
                alarm.severity === 'critical'
                  ? 'bg-[#a00]/20 border-l-4 border-[#f00]'
                  : alarm.severity === 'warning'
                  ? 'bg-[#a60]/20 border-l-4 border-[#fa0]'
                  : 'bg-[#06a]/20 border-l-4 border-[#0af]'
              }`}
            >
              <div className="flex items-start justify-between gap-3">
                <div className="flex-1">
                  <div className="flex items-center gap-2 mb-1">
                    <AlertTriangle
                      className={`w-4 h-4 ${
                        alarm.severity === 'critical'
                          ? 'text-[#f00]'
                          : alarm.severity === 'warning'
                          ? 'text-[#fa0]'
                          : 'text-[#0af]'
                      }`}
                    />
                    <span
                      className={`text-xs font-bold ${
                        alarm.severity === 'critical'
                          ? 'text-[#f00]'
                          : alarm.severity === 'warning'
                          ? 'text-[#fa0]'
                          : 'text-[#0af]'
                      }`}
                    >
                      {alarm.severity.toUpperCase()}
                    </span>
                  </div>
                  <div className="text-sm text-white mb-1">{alarm.message}</div>
                  <div className="text-[10px] text-gray-500 font-mono">
                    {new Date(alarm.timestamp).toLocaleString()}
                  </div>
                </div>
                <button
                  onClick={() => onAcknowledge(alarm.id)}
                  className="p-1.5 hover:bg-[#333] border border-[#444] transition-colors shrink-0"
                  title="Acknowledge"
                >
                  <X className="w-4 h-4 text-gray-400" />
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
