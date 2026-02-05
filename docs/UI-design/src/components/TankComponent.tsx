import { TankData } from '../App';

interface TankComponentProps {
  data: TankData;
  selected: boolean;
  onSelect: () => void;
}

export function TankComponent({ data, selected, onSelect }: TankComponentProps) {
  const borderColor = selected ? '#0af' : data.levelAlarmHigh || data.levelAlarmLow ? '#f00' : '#666';
  const levelColor = data.levelAlarmHigh ? '#f00' : data.levelAlarmLow ? '#fa0' : '#0af';

  return (
    <div 
      className="cursor-pointer select-none"
      onClick={onSelect}
    >
      {/* Tag */}
      <div className="text-xs font-bold text-[#0af] mb-2 text-center tracking-wider">
        {data.name}
      </div>

      {/* Tank Symbol */}
      <svg width="160" height="280" className={`transition-all ${selected ? 'drop-shadow-lg' : ''}`}>
        {/* Selection highlight */}
        {selected && (
          <rect
            x="-2"
            y="-2"
            width="164"
            height="284"
            fill="none"
            stroke="#0af"
            strokeWidth="3"
            opacity="0.6"
          />
        )}

        {/* Tank roof */}
        <ellipse
          cx="80"
          cy="25"
          rx="50"
          ry="10"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Tank shell */}
        <rect
          x="30"
          y="25"
          width="100"
          height="200"
          fill="#2a2a2a"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Tank bottom */}
        <ellipse
          cx="80"
          cy="225"
          rx="50"
          ry="10"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Level scale */}
        <defs>
          <clipPath id="tankClip">
            <rect x="30" y="25" width="100" height="200" />
          </clipPath>
        </defs>

        {/* Level markings */}
        {[0, 25, 50, 75, 100].map((mark) => (
          <g key={mark}>
            <line
              x1="25"
              y1={25 + 200 * (1 - mark / 100)}
              x2="30"
              y2={25 + 200 * (1 - mark / 100)}
              stroke="#999"
              strokeWidth="1"
            />
            <text
              x="20"
              y={25 + 200 * (1 - mark / 100) + 3}
              textAnchor="end"
              fill="#999"
              fontSize="8"
              fontFamily="monospace"
            >
              {mark}
            </text>
          </g>
        ))}

        {/* Liquid fill */}
        <rect
          x="32"
          y={25 + 200 * (1 - data.level / 100)}
          width="96"
          height={200 * (data.level / 100)}
          fill={levelColor}
          opacity="0.5"
          clipPath="url(#tankClip)"
        />

        {/* Liquid surface */}
        <line
          x1="32"
          y1={25 + 200 * (1 - data.level / 100)}
          x2="128"
          y2={25 + 200 * (1 - data.level / 100)}
          stroke={levelColor}
          strokeWidth="2"
        />

        {/* Inlet nozzle */}
        <rect
          x="5"
          y="115"
          width="25"
          height="10"
          fill="#444"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Outlet nozzle */}
        <rect
          x="130"
          y="210"
          width="25"
          height="10"
          fill="#444"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Support structure */}
        <rect x="40" y="225" width="8" height="35" fill="#444" stroke="#666" strokeWidth="1" />
        <rect x="112" y="225" width="8" height="35" fill="#444" stroke="#666" strokeWidth="1" />
        <rect x="35" y="258" width="90" height="5" fill="#444" stroke="#666" strokeWidth="1" />

        {/* Manhole */}
        <ellipse
          cx="80"
          cy="18"
          rx="8"
          ry="3"
          fill="#444"
          stroke="#777"
          strokeWidth="1.5"
        />
      </svg>

      {/* Instrumentation Panel */}
      <div className="mt-3 bg-[#2a2a2a] border-2 border-[#444] p-2.5">
        <div className="grid grid-cols-3 gap-2 text-[10px] font-mono mb-2">
          <GaugeDisplay label="LT-201" value={data.level.toFixed(1)} unit="%" color={levelColor} />
          <GaugeDisplay label="PT-201" value={data.pressure.toFixed(2)} unit="bar" color="#a0f" />
          <GaugeDisplay label="TT-201" value={data.temperature.toFixed(1)} unit="°C" color="#f60" />
        </div>

        {/* Alarm indicators */}
        <div className="flex justify-between items-center pt-2 border-t border-[#444] text-[9px]">
          <div className="flex items-center gap-1">
            <div className={`w-1.5 h-1.5 ${data.levelAlarmHigh || data.levelAlarmLow ? 'bg-[#f00]' : 'bg-[#666]'}`} />
            <span className="text-gray-500">LEVEL ALARM</span>
          </div>
          <div className="flex items-center gap-1">
            <div className="w-1.5 h-1.5 bg-[#0f0]" />
            <span className="text-gray-500">NORMAL</span>
          </div>
        </div>
      </div>
    </div>
  );
}

function GaugeDisplay({ label, value, unit, color }: { label: string; value: string; unit: string; color: string }) {
  return (
    <div className="bg-[#1a1a1a] border border-[#555] px-1.5 py-1">
      <div className="text-gray-500 mb-0.5">{label}</div>
      <div className="flex items-baseline gap-0.5">
        <span className="text-sm font-bold" style={{ color }}>{value}</span>
        <span className="text-[8px] text-gray-400">{unit}</span>
      </div>
    </div>
  );
}
