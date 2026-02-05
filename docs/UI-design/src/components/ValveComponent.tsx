import { ValveData } from '../App';

interface ValveComponentProps {
  data: ValveData;
  selected: boolean;
  onSelect: () => void;
}

export function ValveComponent({ data, selected, onSelect }: ValveComponentProps) {
  const statusColor = data.fault ? '#f00' : data.open ? '#0f0' : '#666';
  const borderColor = selected ? '#0af' : data.fault ? '#f00' : '#666';
  const rotation = (data.position / 100) * 90; // 0° closed, 90° open

  return (
    <div 
      className="cursor-pointer select-none"
      onClick={onSelect}
    >
      {/* Tag Label */}
      <div className="text-xs font-bold text-[#0af] mb-1 tracking-wider">
        {data.name}
      </div>

      {/* Valve Symbol - Diamond shape */}
      <svg width="70" height="80" className={`transition-all ${selected ? 'drop-shadow-lg' : ''}`}>
        {/* Selection highlight */}
        {selected && (
          <rect
            x="-2"
            y="-2"
            width="74"
            height="84"
            fill="none"
            stroke="#0af"
            strokeWidth="2"
            opacity="0.6"
          />
        )}

        {/* Actuator */}
        <rect
          x="20"
          y="5"
          width="30"
          height="18"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
          rx="1"
        />

        {/* Actuator label */}
        <text
          x="35"
          y="16"
          textAnchor="middle"
          fill="#999"
          fontSize="8"
          fontFamily="monospace"
          fontWeight="bold"
        >
          M
        </text>

        {/* Status indicator */}
        <circle
          cx="24"
          cy="9"
          r="3"
          fill={statusColor}
          stroke="#000"
          strokeWidth="1"
        />

        {/* Stem */}
        <rect
          x="33"
          y="23"
          width="4"
          height="12"
          fill="#555"
          stroke="#666"
          strokeWidth="1"
        />

        {/* Valve body - Diamond */}
        <path
          d="M 35 35 L 60 50 L 35 65 L 10 50 Z"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Valve disc (rotates based on position) */}
        <g transform={`rotate(${rotation} 35 50)`}>
          <ellipse
            cx="35"
            cy="50"
            rx="16"
            ry="2.5"
            fill={data.open ? '#0f0' : '#f00'}
            stroke="#000"
            strokeWidth="1"
          />
        </g>

        {/* Position indicator arc */}
        <g transform="translate(55, 10)">
          <circle
            cx="0"
            cy="0"
            r="6"
            fill="#2a2a2a"
            stroke="#666"
            strokeWidth="1"
          />
          <line
            x1="0"
            y1="0"
            x2={5 * Math.cos((rotation - 90) * Math.PI / 180)}
            y2={5 * Math.sin((rotation - 90) * Math.PI / 180)}
            stroke={statusColor}
            strokeWidth="2"
            strokeLinecap="round"
          />
        </g>
      </svg>

      {/* Process Value */}
      <div className="mt-2">
        <div className="flex justify-between items-center bg-[#2a2a2a] border border-[#444] px-2 py-0.5 text-[10px] font-mono">
          <span className="text-gray-500">POS:</span>
          <span className="font-bold" style={{ color: statusColor }}>{data.position}%</span>
        </div>
      </div>
    </div>
  );
}
