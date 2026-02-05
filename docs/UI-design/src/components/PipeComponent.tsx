interface PipeComponentProps {
  x: number;
  y: number;
  length: number;
  direction: 'horizontal' | 'vertical';
  flowing: boolean;
}

export function PipeComponent({ x, y, length, direction, flowing }: PipeComponentProps) {
  const pipeWidth = 12;
  const isHorizontal = direction === 'horizontal';
  const width = isHorizontal ? length : pipeWidth;
  const height = isHorizontal ? pipeWidth : length;

  return (
    <svg
      width={width}
      height={height}
      style={{
        position: 'absolute',
        left: `${x}px`,
        top: `${y}px`,
      }}
    >
      <defs>
        {/* Flow pattern */}
        <pattern
          id={`flowPattern-${x}-${y}`}
          x="0"
          y="0"
          width={isHorizontal ? "20" : pipeWidth}
          height={isHorizontal ? pipeWidth : "20"}
          patternUnits="userSpaceOnUse"
        >
          <rect
            width={isHorizontal ? "10" : pipeWidth}
            height={isHorizontal ? pipeWidth : "10"}
            fill={flowing ? '#0af' : 'transparent'}
            opacity={flowing ? "0.4" : "0"}
          />
          {flowing && (
            <animateTransform
              attributeName="patternTransform"
              type="translate"
              from={isHorizontal ? "0 0" : "0 0"}
              to={isHorizontal ? "20 0" : "0 20"}
              dur="2s"
              repeatCount="indefinite"
            />
          )}
        </pattern>
      </defs>

      {/* Pipe outer */}
      <rect
        x="0"
        y="0"
        width={width}
        height={height}
        fill="#1a1a1a"
        stroke="#666"
        strokeWidth="2"
      />

      {/* Pipe inner */}
      <rect
        x="2"
        y="2"
        width={width - 4}
        height={height - 4}
        fill="#2a2a2a"
      />

      {/* Flow indicator */}
      <rect
        x="2"
        y="2"
        width={width - 4}
        height={height - 4}
        fill={`url(#flowPattern-${x}-${y})`}
      />

      {/* Flow direction arrows */}
      {flowing && (
        <g>
          {isHorizontal ? (
            <path
              d="M 50 6 L 45 4 L 45 8 Z"
              fill="#0af"
              opacity="0.8"
            >
              <animateTransform
                attributeName="transform"
                type="translate"
                from="0 0"
                to="30 0"
                dur="2s"
                repeatCount="indefinite"
              />
              <animate
                attributeName="opacity"
                values="0.8;0;0.8"
                dur="2s"
                repeatCount="indefinite"
              />
            </path>
          ) : (
            <path
              d="M 6 50 L 4 45 L 8 45 Z"
              fill="#0af"
              opacity="0.8"
            >
              <animateTransform
                attributeName="transform"
                type="translate"
                from="0 0"
                to="0 30"
                dur="2s"
                repeatCount="indefinite"
              />
              <animate
                attributeName="opacity"
                values="0.8;0;0.8"
                dur="2s"
                repeatCount="indefinite"
              />
            </path>
          )}
        </g>
      )}

      {/* Flange markers */}
      <rect
        x={isHorizontal ? "0" : "0"}
        y={isHorizontal ? "0" : "0"}
        width={isHorizontal ? "3" : width}
        height={isHorizontal ? height : "3"}
        fill="#555"
        stroke="#777"
        strokeWidth="1"
      />
      <rect
        x={isHorizontal ? width - 3 : "0"}
        y={isHorizontal ? "0" : height - 3}
        width={isHorizontal ? "3" : width}
        height={isHorizontal ? height : "3"}
        fill="#555"
        stroke="#777"
        strokeWidth="1"
      />
    </svg>
  );
}
