import { SensorData } from '../App';

interface SensorComponentProps {
  data: SensorData;
  selected: boolean;
  onSelect: () => void;
}

export function SensorComponent({ data, selected, onSelect }: SensorComponentProps) {
  const typeColor = data.type === 'pressure' ? '#a0f' : '#f60';
  const borderColor = selected ? '#0af' : data.alarm ? '#f00' : '#666';
  const valueColor = data.alarm ? '#f00' : typeColor;

  return (
    <div 
      className="inline-block cursor-pointer select-none"
      onClick={onSelect}
    >
      <div 
        className={`bg-[#2a2a2a] border-2 px-3 py-2 min-w-[90px] transition-all ${
          selected ? 'shadow-lg' : ''
        }`}
        style={{ borderColor }}
      >
        {/* Tag */}
        <div className="text-[9px] text-gray-500 font-mono mb-1 tracking-wider">
          {data.name}
        </div>

        {/* Value */}
        <div className="flex items-baseline gap-1">
          <div 
            className="text-base font-bold font-mono leading-none"
            style={{ color: valueColor }}
          >
            {data.value.toFixed(data.type === 'pressure' ? 2 : 1)}
          </div>
          <div className="text-[9px] text-gray-400 font-mono">
            {data.unit}
          </div>
        </div>

        {/* Status */}
        <div className="mt-1 flex items-center gap-1">
          <div 
            className="w-1.5 h-1.5" 
            style={{ backgroundColor: data.alarm ? '#f00' : '#0f0' }}
          />
          <div className="text-[8px] text-gray-600 font-mono">
            {data.quality.toUpperCase()}
          </div>
        </div>
      </div>

      {/* Connection line to pipe */}
      <svg width="90" height="15" className="mx-auto">
        <line
          x1="45"
          y1="0"
          x2="45"
          y2="15"
          stroke="#666"
          strokeWidth="1.5"
          strokeDasharray="2,2"
        />
        <circle
          cx="45"
          cy="15"
          r="2.5"
          fill={typeColor}
          stroke="#000"
          strokeWidth="1"
        />
      </svg>
    </div>
  );
}
