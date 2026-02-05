import { PumpData } from '../App';

interface PumpComponentProps {
  data: PumpData;
  selected: boolean;
  onSelect: () => void;
}

export function PumpComponent({ data, selected, onSelect }: PumpComponentProps) {
  const statusColor = data.fault ? '#f00' : data.running ? '#0f0' : '#666';
  const borderColor = selected ? '#0af' : data.fault ? '#f00' : '#666';

  return (
    <div 
      className="cursor-pointer select-none"
      onClick={onSelect}
    >
      {/* Tag Label */}
      <div className="text-xs font-bold text-[#0af] mb-1 tracking-wider">
        {data.name}
      </div>

      {/* Pump Symbol */}
      <svg width="100" height="60" className={`transition-all ${selected ? 'drop-shadow-lg' : ''}`}>
        {/* Selection highlight */}
        {selected && (
          <rect
            x="-2"
            y="-2"
            width="104"
            height="64"
            fill="none"
            stroke="#0af"
            strokeWidth="2"
            opacity="0.6"
          />
        )}

        {/* Motor body */}
        <rect
          x="5"
          y="15"
          width="40"
          height="30"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Motor end */}
        <ellipse
          cx="25"
          cy="30"
          rx="6"
          ry="15"
          fill="#2a2a2a"
          stroke={borderColor}
          strokeWidth="1"
        />

        {/* Shaft */}
        <rect
          x="45"
          y="27"
          width="12"
          height="6"
          fill="#555"
          stroke="#777"
          strokeWidth="1"
        />

        {/* Pump casing - circular */}
        <circle
          cx="70"
          cy="30"
          r="18"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Discharge nozzle */}
        <rect
          x="88"
          y="26"
          width="10"
          height="8"
          fill="#444"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Status indicator */}
        <circle
          cx="15"
          cy="20"
          r="4"
          fill={statusColor}
          stroke="#000"
          strokeWidth="1"
        />

        {/* Running indicator (pulsing center) */}
        {data.running && (
          <circle
            cx="70"
            cy="30"
            r="8"
            fill="none"
            stroke="#0af"
            strokeWidth="2"
            opacity="0.6"
          >
            <animate
              attributeName="r"
              values="6;10;6"
              dur="1.5s"
              repeatCount="indefinite"
            />
            <animate
              attributeName="opacity"
              values="0.8;0.3;0.8"
              dur="1.5s"
              repeatCount="indefinite"
            />
          </circle>
        )}
      </svg>

      {/* Process Values */}
      <div className="mt-2 space-y-0.5">
        <ValueDisplay label="FREQ" value={`${data.frequency.toFixed(1)} Hz`} color="#0af" />
        <ValueDisplay label="CURR" value={`${data.current.toFixed(1)} A`} color="#fa0" />
        <ValueDisplay label="PWR" value={`${data.power.toFixed(1)} kW`} color="#f0a" />
      </div>
    </div>
  );
}

function ValueDisplay({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div className="flex justify-between items-center bg-[#2a2a2a] border border-[#444] px-2 py-0.5 text-[10px] font-mono">
      <span className="text-gray-500">{label}:</span>
      <span className="font-bold" style={{ color }}>{value}</span>
    </div>
  );
}
