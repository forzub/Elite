# Navigation Runtime — two-stage diagnostic stand

Location: `tools/navigation_runtime/`.

The tool deliberately separates **route construction** from **route execution** so a
bad path can be distinguished from bad ship control.

## Stage 0 — scene preview

The authored scene is visible immediately, before any calculation:

- green cross: START;
- yellow cross/ring: FINISH;
- grey wire geometry: static obstacles;
- reference grid;
- Cobra body at START.

The scene is loaded from `scenario.json`. No planner or follower has run yet.

## Stage 1 — calculate the static nominal route

Press `РАССЧИТАТЬ`.

```text
start
 + optional required ship_route_points
 + finish
 + static NavigationObstacle geometry
        |
        v
NominalRoutePlanner
        |
        v
GeometricPathPlanner
        |
        v
retained sparse route polyline
```

The white route appears after Stage 1.

The nominal route is rebuilt only when:
- goal revision changes; or
- static-world revision changes.

A dynamic-world revision does **not** invalidate/rebuild the route.

Stage-1 diagnostics are printed to stdout and written to:

`tools/navigation_runtime/last_route_plan.log`

They include:
- Planner success/failure;
- START/FINISH;
- static obstacle count;
- required waypoint count;
- route point count;
- route length;
- static-detour flag;
- goal/static-world revisions.

## Stage 2 — execute the retained route

After a successful Stage 1, the former playback button becomes
`ЗАПУСТИТЬ ПОЛЁТ`.

Stage 2 does **not** call a global planner. It consumes the exact
`TraceDocument::routePoints` produced by Stage 1:

```text
retained Stage-1 polyline
        |
        v
TrajectoryGenerator / RuckigRoutePlanner
        |
        v
time-parameterized trajectory
        |
        v
AcceptedManeuverProgram chunks
        |
        v
TrajectoryFollower
        |
        v
NavigationRuntimeControlBridge
        |
        v
PilotSkillExecutor
        |
        v
SharedShipPhysics + DynamicMotionSystem
        |
        v
actual Cobra trajectory / playback trace
```

The route remains white. The actual ship path is green.

Stage 2 records frames at approximately 30 Hz and simulates physics at 120 Hz. A failed
execution trace is still kept and can be replayed for diagnosis.

Stage-2 diagnostics are printed with `[NAV-STAGE2]` and written to:

`tools/navigation_runtime/last_execution.log`

The complete execution trace is written to:

`tools/navigation_runtime/last_execution_trace.json`

Diagnostics include:
- cached Planner route state;
- Ruckig trajectory result/sample count;
- maneuver-program chunk count;
- Follower status;
- Pilot bridge status;
- selected pilot / flight style / control law;
- final position/speed error;
- maximum deviation from the nominal route;
- maximum Follower tracking error;
- coarse static-contact flag.

## Control selectors

The top selectors have different ownership.

### Stage 1

These do **not** change the static route:
- Assisted / Newtonian;
- Expert / Average / Loser;
- Standard / Extreme;
- sudden-obstacle checkbox.

### Stage 2

These are real execution inputs:
- Assisted / Newtonian changes the physical local flight law and reference-attitude
  behavior;
- Expert / Average / Loser changes the real PilotSkillExecutor profile;
- Standard / Extreme changes the requested Ruckig cruise speed.

The sudden-obstacle option is still reserved for the later dynamic-avoidance pass.
The current static Stage-2 diagnostics explicitly report:

`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`

It must not silently rebuild the global route.

## Corridor vs physical tunnel

`route_envelope_radius_m` and `route_clearance_m` remain coarse navigation/test
abstractions.

The current Ruckig execution backend checks static geometry using this conservative
route envelope, which is useful for the static diagnostic pass, but this is **not yet**
the final oriented swept-hull/tunnel proof.

The exact question “does the rotating Cobra physically clip this wall/aperture?” remains
owned by the later B6 time-parameterized swept-hull tunnel proof.

Therefore a visually successful Stage-2 run is execution evidence, not final B6
navigation acceptance.

## Final-state requirements

The current static scenario supports:
- final position;
- final forward requirement;
- final up requirement;
- zero terminal speed.

The current Ruckig route backend stops at the final waypoint. A non-zero
`finish.speed_mps` is rejected explicitly instead of being silently ignored.

## Build

From MSYS2 MinGW64:

```bash
cd /d/__elite/work
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

## Run executable

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Workflow:

1. inspect scene;
2. press `РАССЧИТАТЬ`;
3. inspect the white route and Stage-1 log;
4. press `ЗАПУСТИТЬ ПОЛЁТ`;
5. watch the Cobra/Follower execute that retained route;
6. inspect `last_execution.log` if behavior is wrong.

The window is an ordinary decorated maximized Windows window, not exclusive fullscreen.
