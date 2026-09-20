# CURRENT TASK — static route + retained-route execution

**Date:** 2026-09-21  
**Status:** CODE COMMITTED / TARGET BUILD AND EXECUTION VALIDATION PENDING

## Current workflow

The diagnostic stand is one executable with two explicit stages.

### Stage 1 — route

Press `РАССЧИТАТЬ`.

```text
scene.json static geometry
 -> NominalRoutePlanner
 -> retained start->finish polyline
```

The global route is built once. Dynamic actors do not invalidate it.

Last target evidence for Stage 1:
- Planner: OK
- route points: 4
- route length: 323.75 m
- static detour: YES
- nominal_route_planner test: PASS

### Stage 2 — execution

After successful Stage 1 press `ЗАПУСТИТЬ ПОЛЁТ` (or SPACE).

```text
retained Stage-1 route
 -> TrajectoryGenerator / RuckigRoutePlanner
 -> time trajectory
 -> AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> execution trace
```

Hard rule: Stage 2 consumes the retained route and must not call any global planner.

The old viewer-local `makeShortProgram()` shortcut remains removed.

## Route reuse for comparisons

The viewer stores an immutable copy of the successful Stage-1 result.

Changing:
- Assisted/Newtonian;
- Expert/Average/Loser;
- Standard/Extreme;
- sudden-obstacle toggle

after an execution run returns the viewer to the same retained white route. The next
`ЗАПУСТИТЬ ПОЛЁТ` reruns Stage 2 without recalculating Stage 1.

This allows apples-to-apples pilot/control-law comparisons.

## Current static Stage-2 scope

Implemented:
- Ruckig time parameterization of the retained polyline;
- real Follower;
- real PilotSkillExecutor;
- real SharedShipPhysics/DynamicMotionSystem;
- selected pilot profiles;
- Standard/Extreme execution speed;
- Newtonian/Assisted physical law;
- final position;
- zero final speed;
- blended final forward/up body orientation;
- 120 Hz physical execution;
- ~30 Hz viewer trace;
- route-deviation/follower-error/final-state diagnostics.

Not yet enabled:
- moving/sudden-obstacle local avoidance;
- exact oriented swept-hull B6 tunnel proof;
- non-zero terminal speed in current Ruckig route backend.

The sudden-obstacle checkbox is therefore parsed/reserved but current Stage 2 must report
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`.

## Diagnostics

Stage 1:
- stdout: `[NAV-STAGE1]`
- `tools/navigation_runtime/last_route_plan.log`
- `tools/navigation_runtime/last_calculated_trace.json`

Stage 2:
- stdout: `[NAV-STAGE2]`
- `tools/navigation_runtime/last_execution.log`
- `tools/navigation_runtime/last_execution_trace.json`

A failed Stage-2 trace is still replayable when multiple frames were produced.

## Physical-proof boundary

The current Stage-2 static contact check uses the coarse route envelope. This is useful
for observing Follower/physics behavior but is NOT the exact oriented Cobra swept-volume
tunnel proof.

Do not call this final B6 acceptance.

## Immediate target gate

Pull latest main:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
```

Run the focused architecture/route test/viewer build:

```bash
cd /d/__elite/work
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If the focused script stops during build, send the complete compiler/linker output.

## Exact executable launch

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

## Visual/behavior acceptance

1. Before calculation: scene, START, FINISH, wall and Cobra are visible.
2. Press `РАССЧИТАТЬ`: white four-point route appears.
3. Button becomes `ЗАПУСТИТЬ ПОЛЁТ`.
4. Press it: Cobra must visibly execute the retained white route.
5. White route must remain unchanged while green actual trajectory grows.
6. Right panel must identify Ruckig / Follower / Pilot bridge status.
7. If execution fails, inspect/send:
   - `tools/navigation_runtime/last_execution.log`
   - screenshot/video
   - optionally `last_execution_trace.json`.
8. Change pilot/control/style after the run: viewer must return to the same retained route
   without invoking Planner; rerun Stage 2 for comparison.

Do not enable dynamic avoidance until this static retained-route execution can be
observed and diagnosed.


## 2026-09-21 — target gate false negative in Stage-2 architecture checker

Target run stopped before compilation with:

`[FAIL] static-route/two-stage navigation: Stage-2 execution path missing TrajectoryGenerator::generate`

This was a checker defect, not a runtime defect. The checker isolated only the body of
`executeCalculatedRoute()`, while the actual Ruckig call is correctly delegated to
`buildExecutionTrajectory()`:

```text
executeCalculatedRoute()
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator::generate()
```

Fix committed in `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`:
- execution function must call `buildExecutionTrajectory()`;
- trajectory-builder slice must contain `calculatedRoute.routePoints` and
  `TrajectoryGenerator::generate`;
- global-planner prohibition is checked across the owned Stage-2 trajectory-builder +
  execution text.

No production navigation/runtime behavior changed in this fix.

Validation state: target compilation of the restored Stage-2 viewer still has NOT been
reached. Rerun the focused gate from latest main.


## 2026-09-21 — second Stage-2 architecture checker false negative

Target gate stopped with:
`Stage-2 execution path missing DynamicMotionSystem::applySystemAccelerationDemand`.

Runtime call is present. The checker failed because production formatting splits the C++
scope operator/call across lines:

```cpp
game::navigation::DynamicMotionSystem::
    applySystemAccelerationDemand(...)
```

The architecture checker previously searched an exact single-line string.

Fix:
- added whitespace-insensitive C++ token normalization via `compact_cpp()`;
- Stage-2 execution and trajectory-builder call checks now compare normalized token
  streams rather than raw formatting;
- runtime behavior is unchanged.

Validation state: target compilation of the restored two-stage viewer still has not yet
been reached; rerun the focused gate from latest main.


## 2026-09-21 — Follower first-chunk handoff bug + full initial kinematics

Target Stage-2 execution reached real runtime and produced:
- cached Planner route OK;
- Ruckig trajectory OK, 1521 samples;
- 102 AcceptedManeuverProgram chunks;
- Follower FAIL after only 24 viewer frames;
- max Follower position error only 0.02 m;
- final position still ~295 m from goal.

The failure signature localizes the problem to the first AcceptedManeuverProgram chunk
boundary, not route tracking quality. At Standard 10 m/s the Ruckig sampling interval is
0.05 s for this scenario. A 16-sample program therefore spans about 0.75 s, matching the
~24 trace frames at 30 Hz before failure.

Root cause found in Stage-2 program selection:

```cpp
vehicle.timeSeconds + 1.0e-9 >= next.acceptedAtUniverseTimeSeconds
```

This can activate the next program fractionally before its acceptance time. The canonical
ManeuverProgramSampler treats any negative elapsed time as `BeforeStart`, and
TrajectoryFollower maps `BeforeStart` to `InvalidInput`.

Fix committed:
- remove the positive epsilon from future-program activation;
- keep the current chunk until simulation time is actually >= next acceptedAt;
- pre-sample the active program before Follower and record exact failure reason/index/
  execution time/program acceptedAt in Stage-2 diagnostics;
- architecture gate now forbids the early-switch expression and pins the new diagnostics.

The user's video is consistent with this: Cobra moves only a few meters, then execution
stops at the first chunk handoff.

### Initial ship state correction

The scenario previously supplied only position, velocity and body orientation. That was
incomplete for a physically continuous Newtonian execution handoff.

Stage-2 initial kinematic state is now explicitly:
- position P;
- linear velocity V;
- linear acceleration A;
- body orientation (forward/up basis);
- pitch/yaw/roll angular rates.

New scenario fields under `start`:
- `acceleration`;
- `pitch_rate_rad_s`;
- `yaw_rate_rad_s`;
- `roll_rate_rad_s`.

Default current scenario remains:
- P = (0,0,0) m;
- V = (6,0,0) m/s;
- A = (0,0,0) m/s2;
- forward = +X;
- up = +Y;
- angular rates = 0.

`TrajectoryGenerationRequest` now owns `initialAccelerationMps2`, validates it, and
passes it into the initial Ruckig state instead of hardcoding zero. A focused Ruckig route
test now requires initial acceleration to be preserved at the first trajectory sample.

The execution vehicle initializes pitch/yaw/roll rate from the scenario. No global route
recalculation semantics changed.

Validation state: these fixes are committed but not yet target-rebuilt/re-run.
