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
AcceptedManeuverProgram route-leg phases
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
- route-leg program phase count;
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


## Initial ship state

Stage-2 execution now starts from a complete local kinematic state.

`scenario.json/start` provides:
- `position`;
- `velocity`;
- `acceleration`;
- `forward` / `up` body orientation;
- `pitch_rate_rad_s`;
- `yaw_rate_rad_s`;
- `roll_rate_rad_s`.

The default scenario currently starts at:
- P=(0,0,0) m;
- V=(6,0,0) m/s;
- A=(0,0,0) m/s²;
- forward=+X;
- up=+Y;
- angular rates=0.

The initial acceleration is passed into the Ruckig route request instead of being
hardcoded to zero. This preserves acceleration continuity at the start of a Newtonian
execution.

## Follower route-phase handoff

The 2026-09-21 target run exposed a real execution bug at the first
AcceptedManeuverProgram phase boundary. The route produced 1521 trajectory samples and
102 bounded program chunks; Follower failed after ~24 viewer frames with only ~0.02 m
tracking error.

Root cause: the Stage-2 scheduler allowed the next chunk to become active fractionally
before its `acceptedAtUniverseTimeSeconds` because of a positive epsilon in the
comparison. The sampler correctly returned `BeforeStart`, which Follower surfaced as
`InvalidInput`.

The scheduler now switches chunks only when current simulation time is actually >= the
next program acceptance time. Stage-2 diagnostics also record the exact Follower failure
reason, phase index, failure time, and phase accepted-at time.


## 2026-09-21 test audit: why old green tests did not catch the viewer failure

The previous navigation test matrix did **not** contain the exact chain now exercised by
the viewer.

The important split is:

- Ruckig route tests validate route-to-trajectory generation and collision safety, but do
  not execute that trajectory through Follower/PilotSkill/physics.
- Follower/physics movement tests execute `AcceptedManeuverProgram`, but construct those
  programs directly in the test. They do not consume raw Ruckig output.
- Composite tests combine Planner, Follower, PilotSkill and physics, but each physical
  phase is hand-authored from the current actual vehicle state; Ruckig is not in that
  chain.

The old execution tests use a small number of meaningful physical phases. A typical
100 m line is one 16-sample program spanning the whole ~20 s maneuver. A right-angle
case explicitly executes `leg1 -> rotate -> leg2`, with each next phase accepted at the
actual current simulation time and advanced by `ManeuverPhaseGate`.

The viewer had introduced a new untested adapter that copied every 16 consecutive 0.05 s
Ruckig samples into a new program. A 1521-sample trajectory therefore became 102 tiny
programs. That is not what the passing execution tests validated and is not the intended
meaning of the fixed 16-sample execution product.

The viewer adapter now follows the tested model:
- the global route is still calculated exactly once;
- Ruckig still parameterizes the retained route once;
- raw Ruckig samples are compressed into one bounded physical program per retained
  coarse route leg;
- each program uses up to 16 reference knots spread across the whole leg;
- `ManeuverPhaseGate` owns phase advancement;
- the next phase is accepted at the actual current simulation time.

For the current four-point route the expected scale is approximately three execution
phases, not 102 micro-programs.

A new exact regression `navigation_runtime_pipeline_tests` now runs the same chain as
the viewer:

```text
scenario.json
 -> NominalRoutePlanner
 -> retained route
 -> Ruckig trajectory
 -> route-leg AcceptedManeuverProgram phases
 -> TrajectoryFollower
 -> PilotSkill
 -> SharedShipPhysics/DynamicMotionSystem
 -> authored finish
```

The focused navigation script now runs this E2E test after building the viewer. A green
component matrix alone is no longer sufficient to claim the viewer execution path works.
