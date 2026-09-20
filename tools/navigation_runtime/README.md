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


## Interactive HUD

The viewer now has a visible diagnostic HUD instead of relying on hidden keyboard
knowledge or the window title alone.

Top buttons:
- `PLAY/PAUSE`;
- `PREV`;
- `NEXT`;
- `NEXT REPLAN`;
- `FIT`.

The right panel shows:
- control law;
- frame number and simulation time;
- current phase and planner status;
- live dynamic clearance;
- explicit REPLAN event indication;
- a short plain-English explanation of what the navigation system is doing;
- a color legend for route, ship path, hazard path, ship box/nose, turn points,
  bypass target, reacquisition reference, portal target and replan event;
- mouse/keyboard controls.

The phase `portal_102` is explicitly described as the current known
clearance-loss area so visual replay can be used to diagnose the failure rather
than only observe motion.


## 2026-09-20 diagnostic accuracy update

The viewer remains a deterministic replay tool: the composite test runs the real
planner/follower/physics first and writes the result to JSON. The viewer does not
rerun planner/follower while playing the file.

Replay smoothness:
- trace sampling remains approximately 0.10 s for compact diagnostic files;
- the viewer now interpolates ship/hazard/reference state between samples, so visible
  motion is no longer limited to ~10 Hz.

Attitude correctness:
- v2 trace stores actual ship forward/right/up, preserving roll;
- v2 trace also stores the sampled AcceptedManeuverProgram reference basis;
- the viewer draws actual nose separately from program nose and shows orientation error.

Tracking corridor:
- NavigationRuntimePlanner::Result does not publish a standalone volumetric corridor;
- the viewer therefore does not invent one;
- it renders the AcceptedManeuverProgram reference path plus
  tracking.positionErrorMeters as a translucent tracking tube.

Cobra horizon:
- bottom-left inset is a plane perpendicular to current Cobra forward axis;
- dynamic hazard positions are projected into Cobra right/up coordinates;
- the predicted obstacle envelope is drawn through planner look-ahead using recorded
  hazard velocity and current ship velocity.

Window:
- viewer opens as a normal decorated Windows window maximized to the desktop work area,
  not exclusive/borderless fullscreen.

Newtonian fixture:
- dynamic local bypass/reacquisition programs no longer force VelocityAligned attitude
  when law is Newtonian;
- Newtonian bypass preserves the current rigid-body basis unless an explicit maneuver
  attitude contract requires otherwise;
- Assisted retains velocity-aligned behavior.
