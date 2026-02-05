import { SafetyValveData } from '../App';

interface SafetyValveComponentProps {
  data: SafetyValveData;
  selected: boolean;
  onSelect: () => void;
}

export function SafetyValveComponent({ data, selected, onSelect }: SafetyValveComponentProps) {
  const statusColor = data.triggered ? '#f00' : '#666';
  const borderColor = selected ? '#0af' : data.triggered ? '#f00' : '#666';

  return (
    <div 
      className="inline-block cursor-pointer select-none"
      onClick={onSelect}
    >
      {/* Tag */}
      <div className="text-[9px] text-gray-500 font-mono mb-1 text-center tracking-wider">
        {data.name}
      </div>

      {/* Safety Valve Symbol - Spring loaded */}
      <svg width="40" height="65" className={`transition-all ${selected ? 'drop-shadow-lg' : ''}`}>
        {/* Selection highlight */}
        {selected && (
          <rect
            x="-2"
            y="-2"
            width="44"
            height="69"
            fill="none"
            stroke="#0af"
            strokeWidth="2"
            opacity="0.6"
          />
        )}

        {/* Spring housing */}
        <rect
          x="15"
          y="5"
          width="10"
          height="10"
          fill="#333"
          stroke={borderColor}
          strokeWidth="1.5"
        />

        {/* Spring visualization */}
        <path
          d="M 20 15 L 17 17 L 23 19 L 17 21 L 23 23 L 17 25 L 20 27"
          fill="none"
          stroke="#999"
          strokeWidth="1.5"
        />

        {/* Valve body - Triangle */}
        <path
          d="M 10 28 L 20 42 L 30 28 Z"
          fill="#333"
          stroke={borderColor}
          strokeWidth="2"
        />

        {/* Discharge vent */}
        <path
          d="M 30 32 L 38 28 L 38 36 Z"
          fill="#2a2a2a"
          stroke={borderColor}
          strokeWidth="1.5"
        />

        {/* Vent outlet */}
        {data.triggered && (
          <>
            <rect
              x="38"
              y="30"
              width="3"
              height="4"
              fill="#f00"
              opacity="0.8"
            />
            {/* Discharge animation */}
            <circle cx="40" cy="32" r="2" fill="#f00" opacity="0.6">
              <animate attributeName="r" values="2;4;2" dur="0.5s" repeatCount="indefinite" />
              <animate attributeName="opacity" values="0.6;0;0.6" dur="0.5s" repeatCount="indefinite" />
            </circle>
          </>
        )}

        {/* Inlet */}
        <rect
          x="17"
          y="42"
          width="6"
          height="12"
          fill="#333"
          stroke={borderColor}
          strokeWidth="1.5"
        />

        {/* Status indicator */}
        <circle
          cx="6"
          cy="34"
          r="3"
          fill={statusColor}
          stroke="#000"
          strokeWidth="1"
        />

        {data.triggered && (
          <circle cx="6" cy="34" r="3" fill="#f00" opacity="0.5">
            <animate attributeName="opacity" values="0.5;1;0.5" dur="0.5s" repeatCount="indefinite" />
          </circle>
        )}
      </svg>

      {/* Set point */}
      <div className="text-[9px] text-gray-500 font-mono text-center mt-1">
        {data.setPoint} bar
      </div>
    </div>
  );
}
