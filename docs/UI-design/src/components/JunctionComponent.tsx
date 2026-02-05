interface JunctionComponentProps {
  flowing: boolean;
}

export function JunctionComponent({ flowing }: JunctionComponentProps) {
  return (
    <svg width="24" height="24" style={{ position: 'relative', zIndex: 10 }}>
      {/* Junction circle */}
      <circle
        cx="12"
        cy="12"
        r="8"
        fill="#2a2a2a"
        stroke="#666"
        strokeWidth="2"
      />

      {/* Inner indicator */}
      {flowing && (
        <circle
          cx="12"
          cy="12"
          r="4"
          fill="#0af"
          opacity="0.6"
        >
          <animate
            attributeName="r"
            values="3;5;3"
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

      {!flowing && (
        <circle
          cx="12"
          cy="12"
          r="3"
          fill="#444"
        />
      )}
    </svg>
  );
}
