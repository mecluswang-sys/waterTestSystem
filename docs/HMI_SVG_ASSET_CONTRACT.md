# HMI SVG Asset Contract

## Source Of Truth

`PLC-HMI/` is the shared source directory for basic HMI device symbols. The PLC HMI consumes these files directly. The C++ application packages the same source files through `resources/styles.qrc` and refers to them with the `:/hmi/...` aliases.

Do not copy an SVG into a C++-only asset directory. Update the source SVG in `PLC-HMI/`, then rebuild the C++ application so RCC packages the updated version.

## Layering

The shared SVG is the basic visual layer:

- device geometry and static piping ports
- basic open/closed or running/stopped state color
- shape proportions and viewBox

The C++ UI is the enhanced layer:

- PLC live values, units, alarms, and communication state
- instance-specific identifiers, names, and setpoints
- selection, click handling, drag behavior, tooltips, and diagnostics
- animated flow paths and automatic pipe routing
- C++ theme-specific decorations

An SVG must not be the authority for a live PLC value. If an existing asset has demo text such as `PUMP01`, `RUN`, or `45.5Hz`, the C++ page must either overlay the real information or use it only as a basic HMI preview until the asset is converted to a neutral symbol.

## Coordinate Contract

Every reusable device SVG must document its `viewBox` and ports. The C++ `QGraphicsItem::boundingRect()` must fully contain that viewBox, including stroke widths; otherwise Qt can clip the asset.

For the current two-port valve:

- viewBox: `-90 -60 180 120`
- inlet port: `(-33, 0)`
- outlet port: `(33, 0)`

The C++ `ValveItem` preserves these two local ports for `DynamicPipe`, while its SVG supplies the basic valve appearance.

For the current two-port pump:

- viewBox: `-55 -70 110 140`
- inlet port: `(-45, 17)`
- outlet port: `(45, -17)`

For the current two-port flowmeter:

- viewBox: `-70 -52 140 110`
- inlet port: `(-60, 0)`
- outlet port: `(60, 0)`

For the preparation-area tee node:

- viewBox: `-24 -20 48 40`
- upper inlet: `(-18, -8)`
- lower inlet: `(-18, 8)`
- outlet: `(18, 0)`

For dynamic process pipes, `pipe-open-100.svg` and `pipe-close-100.svg` are canonical straight segments, not fixed 100-pixel layout assets:

- viewBox: `-50 -6 100 12`
- local endpoints: `(-50, 0)` and `(50, 0)`
- pipe strokes extend beyond both viewBox ends so the SVG viewport clips the end borders; tiled segments must not draw a separate cap at each boundary
- C++ stretches the segment to each horizontal or vertical routed section
- C++ retains the animated flow overlay and all drag/route behavior

## Asset Registration

When a file is added to `PLC-HMI/`, add one stable, lowercase alias in `resources/styles.qrc`. Use that alias in C++ rather than a filesystem path:

```cpp
QSvgRenderer renderer(QStringLiteral(":/hmi/valve-open.svg"));
```

This keeps the deployed executable independent of its current working directory and guarantees that the PLC HMI and C++ application use the same source asset.