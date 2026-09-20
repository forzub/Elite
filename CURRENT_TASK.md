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
