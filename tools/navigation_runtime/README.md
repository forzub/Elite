# Navigation Runtime 3D Viewer

Standalone visualizer for the deterministic navigation composite trace.

The viewer lives under `tools/navigation_runtime/` deliberately: it is a
diagnostic tool, not part of the production game renderer.

## What it shows

- authored route polyline;
- turn/portal points;
- actual ship trajectory;
- ship as an oriented wire box;
- arrow extending from the ship nose;
- dynamic hazard trajectory;
- hazard body radius;
- hull-collision envelope;
- planner safety envelope;
- selected local bypass target;
- on-route reacquisition reference;
- active portal target;
- every recorded replan point.

The window title shows the current frame, universe time, phase, planner status,
dynamic clearance and REPLAN marker.

## Controls

- RMB drag: orbit camera;
- MMB drag: pan;
- mouse wheel: zoom;
- F: fit the complete trace;
- Space: play / pause;
- [: previous recorded frame;
- ]: next recorded frame;
- R: jump to next replan event;
- Esc: close.

## Trace files

`navigation_composite_proving_ground_tests` writes:

- `tools/navigation_runtime/last_trace_newtonian.json`;
- `tools/navigation_runtime/last_trace_assisted.json` when the assisted run is reached.

The file is written by an RAII guard, so a trace is retained even when a
composite assertion throws after motion has already been simulated.

## Build on MSYS2 MinGW64

From repository root:

```bash
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

Run the latest Newtonian trace:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe
```

Or supply a trace explicitly:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/last_trace_newtonian.json
```

The viewer uses the repository's existing GLAD source plus the same installed
GLFW/GLM/nlohmann-json environment already used by Elite development.
