# CONTINUE PROMPT — Elite Navigation static route + retained-route execution

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
- `src/game/navigation/NominalRoutePlanner.h/.cpp`
- `tools/navigation_runtime/README.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.h/.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`

After every state-affecting event update the mandatory Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## Canonical viewer workflow

One executable, two stages:

```text
Stage 1 — РАССЧИТАТЬ
static scene -> NominalRoutePlanner -> retained white route

Stage 2 — ЗАПУСТИТЬ ПОЛЁТ
same retained route
 -> TrajectoryGenerator / Ruckig
 -> AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge / PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> visible Cobra motion / green actual path
```

Stage 2 must never invoke a global planner.

## Last target-verified Stage-1 evidence

User target run:
- `nominal_route_planner`: PASS
- default viewer route:
  - START = (0,0,0)
  - FINISH = (300,0,0)
  - static obstacles = 1
  - route points = 4
  - route length = 323.75 m
  - static detour = YES.

## Route ownership

Nominal route rebuild triggers:
- goal revision changes;
- static-world revision changes.

Dynamic-world revision does NOT rebuild the nominal route.

Moving/sudden obstacles are reserved for later local dynamic avoidance.

## Immutable retained route

Viewer stores the successful Stage-1 route separately from execution traces.

Changing Newtonian/Assisted, Expert/Average/Loser, Standard/Extreme, or the reserved
sudden-obstacle toggle after an execution restores the same retained white route and
invalidates only Stage 2. The next execution must not call Planner.

## Initial ship state

Stage-2 start state is now explicitly:
- position P;
- linear velocity V;
- linear acceleration A;
- body orientation basis (forward/up);
- pitch/yaw/roll angular rates.

Current default `scenario.json`:
- P = (0,0,0) m
- V = (6,0,0) m/s
- A = (0,0,0) m/s²
- forward = +X
- up = +Y
- pitch/yaw/roll rate = 0.

New JSON fields under `start`:
- `acceleration`
- `pitch_rate_rad_s`
- `yaw_rate_rad_s`
- `roll_rate_rad_s`.

`TrajectoryGenerationRequest` now owns `initialAccelerationMps2`.
`TrajectoryGenerator` validates it and passes it into the first Ruckig state instead of
hardcoding zero. A focused Ruckig route test pins first-sample acceleration continuity.

## Latest real Stage-2 target evidence

User target execution log showed:
- PLANNER: CACHED ROUTE OK
- TRAJECTORY: RUCKIG OK
- TRAJECTORY SAMPLES: 1521
- PROGRAM CHUNKS: 102
- FOLLOWER: FAIL
- PILOT BRIDGE: EXECUTED
- Expert / Standard / Newtonian
- EXECUTION FRAMES: 24
- FINAL POSITION ERROR: 295.43 m
- FINAL SPEED: 6.32 m/s
- MAX ROUTE DEVIATION: 1.65 m
- MAX FOLLOWER ERROR: 0.02 m
- COARSE STATIC CONTACT: NO.

Interpretation: route and Ruckig trajectory were valid, Follower tracked accurately for
the first ~0.75 s, then execution failed at the first AcceptedManeuverProgram chunk
handoff.

## Follower chunk-boundary root cause

Ruckig sampling for this scenario is 0.05 s.
AcceptedManeuverProgram capacity is 16 samples, so one chunk spans about:
15 * 0.05 = 0.75 s.

24 viewer frames at ~30 Hz matches the first chunk boundary.

Bug in Stage-2 scheduler:

```cpp
vehicle.timeSeconds + 1.0e-9 >= next.acceptedAtUniverseTimeSeconds
```

The positive epsilon could activate the next program fractionally BEFORE its actual
acceptedAt. `ManeuverProgramSampler` then returns `BeforeStart`, and
`TrajectoryFollower` maps BeforeStart to InvalidInput.

Fix committed:
- removed positive epsilon from future-chunk activation;
- current chunk remains active until simulation time is actually >= next acceptedAt;
- pre-sample active chunk before Follower;
- record exact failure cause/index/time/acceptedAt.

New possible diagnostics:
- `PROGRAM_BEFORE_START`
- `PROGRAM_INVALID`
- `FOLLOWER_OR_TRACKER_INVALID`
- follower fail program index
- follower fail time
- active program accepted-at time.

Architecture checker now forbids reintroducing the early-switch expression.

## Stage-2 execution configuration

Current static chain:
- Ruckig time parameterization;
- real TrajectoryFollower;
- real PilotSkillExecutor;
- SharedShipPhysics + DynamicMotionSystem;
- Standard/Extreme speed;
- Newtonian/Assisted control law;
- final position / zero final speed;
- blended final forward/up orientation;
- 120 Hz physics, ~30 Hz trace.

Current Ruckig backend explicitly rejects non-zero terminal speed.

## Dynamic boundary

Current work remains static obstacles first.

Dynamic/sudden obstacle JSON is parsed/reserved but current Stage 2 must report:
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`.

Do not implement moving-object avoidance until static retained-route execution is stable.

## Corridor vs tunnel

Route envelope is coarse navigation/test geometry.
Current static-contact reporting is coarse.

Exact rotating-Cobra clearance belongs to future B6 time-parameterized swept-hull tunnel
proof.

Do not claim full navigation acceptance before B6.

## Diagnostics

Stage 1:
- stdout `[NAV-STAGE1]`
- `tools/navigation_runtime/last_route_plan.log`
- `tools/navigation_runtime/last_calculated_trace.json`

Stage 2:
- stdout `[NAV-STAGE2]`
- `tools/navigation_runtime/last_execution.log`
- `tools/navigation_runtime/last_execution_trace.json`

## Immediate target action

Pull latest and rebuild focused gate:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Also run the focused Ruckig route test suite because initial acceleration handling changed:

```bash
cd /d/__elite/work
bash tests/navigation_guidance/run_ruckig_mingw64.sh
```

## Exact executable launch after successful build

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Expected next result:
1. same white Stage-1 route;
2. press `ЗАПУСТИТЬ ПОЛЁТ`;
3. Follower must continue through chunk 0 -> 1 instead of dying at ~0.75 s;
4. if execution still fails, use the new exact Follower failure reason/index/time fields
   from `last_execution.log`.

Always provide a separate exact executable launch command after build instructions that
produce an executable.

If target build/run changes state, update all mandatory MD files and recreate this prompt
from scratch again.
