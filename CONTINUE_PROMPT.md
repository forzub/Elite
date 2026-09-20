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

After every state-affecting event synchronize the mandatory Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## User intent

The diagnostic viewer must contain BOTH stages. Do not remove Follower.

```text
Stage 1 — РАССЧИТАТЬ
static scene -> NominalRoutePlanner -> retained white route

Stage 2 — ЗАПУСТИТЬ ПОЛЁТ
same retained route -> Ruckig trajectory -> Follower -> PilotSkill -> physics
                     -> visible Cobra motion + actual green path
```

The split is ownership, not separate executables and not "route only".

## Stage 1 — static nominal route

`NominalRoutePlanner` owns one sparse start->finish route through static geometry.
It reuses `GeometricPathPlanner`.

Inputs:
- START;
- ordered optional `ship_route_points`;
- FINISH;
- static `NavigationObstacle` geometry;
- coarse route envelope/clearance.

Invalidation:
- goal revision change -> rebuild;
- static-world revision change -> rebuild;
- dynamic-world revision change -> DO NOT rebuild.

Moving actors must not reconstruct the global route.

Last target-verified Stage-1 behavior:
- nominal-route behavioral test PASS;
- user viewer log:
  - PLANNER: OK
  - START (0,0,0)
  - FINISH (300,0,0)
  - static obstacles: 1
  - required waypoints: 0
  - route points: 4
  - route length: 323.75 m
  - static detour: YES.

## Viewer scene preview

Before calculation show:
- reference grid;
- green START;
- yellow FINISH;
- static obstacles;
- Cobra at START.

After Stage 1 the white route is retained independently from execution traces.

## Immutable retained route

The viewer stores the successful Stage-1 `TraceDocument` separately.

Stage 2 always receives that retained route. It must not regenerate it.

Changing execution settings after a run:
- Newtonian/Assisted;
- Expert/Average/Loser;
- Standard/Extreme;
- reserved sudden-obstacle toggle

restores the same retained Stage-1 route and invalidates only Stage 2. This enables
clean apples-to-apples execution comparisons.

## Stage 2 — current static execution chain

```text
retained routePoints
 -> buildExecutionTrajectory()
 -> world::navigation::TrajectoryGenerator::generate()
 -> game::navigation::RuckigRoutePlanner backend
 -> time-parameterized trajectory
 -> AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics
 -> DynamicMotionSystem
 -> execution trace / playback
```

Hard rule: `executeCalculatedRoute()` must never invoke:
- `NominalRoutePlanner::plan`;
- `NavigationRuntimePlanner::plan`;
- `GeometricPathPlanner::plan`.

The old viewer-local `makeShortProgram()` shortcut must remain absent.

## Execution configuration

Cobra diagnostic physics mirrors the current Cobra baseline:
- angular accel 3 rad/s²;
- max pitch/yaw 2.5 rad/s;
- max roll 3 rad/s;
- max linear envelope 7.5g;
- manoeuvre/RCS acceleration 2 m/s²;
- real manoeuvre-gas pressure/use/recharge;
- viewer body half extents 13 x 2.5 x 11.1 m.

Pilot selector is real through `PilotSkillExecutor`:
- Expert: zero reaction/latency, high response bandwidth;
- Average: finite reaction/latency, lower response, small deterministic error;
- Loser: larger delay/latency, lower response, larger deterministic error.

Pilot bridge is reset on neutral revision zero so first real route intent exercises the
selected pilot reaction profile.

Flight style:
- Standard -> `standard_speed_mps`;
- Extreme -> `extreme_speed_mps`.

Control law:
- Newtonian uses one-direction main thrust and body rotation for thrust/braking;
- Assisted uses assisted physical law.

Final forward/up orientation is blended near the last ~35 m instead of terminal snap.

Current Ruckig backend explicitly rejects a non-zero requested terminal speed.

## Dynamic boundary

Current target is static retained-route execution first.

Dynamic/sudden obstacles remain parsed/reserved. Current Stage 2 explicitly reports:

`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`

Do not implement dynamic avoidance until this static execution path is target validated.
Later dynamic avoidance must operate locally relative to the retained route and must not
rebuild the global route merely because a moving actor changed.

## Corridor/tunnel boundary

`route_envelope_radius_m` and `route_clearance_m` are coarse navigation/test
abstractions.

Current Stage 2 reports coarse static contact and allows observation of
Follower/physics behavior.

This is NOT final B6 exact oriented swept-hull proof. Exact rotating-Cobra clearance
through walls/apertures belongs to the future time-parameterized physical tunnel layer.

Do not claim full navigation acceptance before B6.

## Diagnostics

Stage 1:
- stdout `[NAV-STAGE1]`;
- `tools/navigation_runtime/last_route_plan.log`;
- `tools/navigation_runtime/last_calculated_trace.json`.

Stage 2:
- stdout `[NAV-STAGE2]`;
- `tools/navigation_runtime/last_execution.log`;
- `tools/navigation_runtime/last_execution_trace.json`.

Stage-2 diagnostics include:
- cached Planner route;
- Ruckig result/sample count;
- maneuver-program chunks;
- Follower status;
- Pilot bridge status;
- pilot/style/control law;
- execution frame count;
- final position/speed error;
- maximum route deviation;
- maximum Follower error;
- coarse static contact.

Failed multi-frame execution traces remain playable for diagnosis.

## Latest target event — architecture checker false negative

Latest user run stopped before compilation:

`[PASS] canonical obstacle geometry + shared geometric path planner`
`[FAIL] static-route/two-stage navigation: Stage-2 execution path missing TrajectoryGenerator::generate`

Root cause: the architecture checker sliced only `executeCalculatedRoute()`, but the
production call is intentionally delegated:

```text
executeCalculatedRoute()
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator::generate()
```

This was a checker defect, not runtime behavior.

Fix:
- `executeCalculatedRoute()` must contain `buildExecutionTrajectory()`;
- isolated `buildExecutionTrajectory()` must contain
  `calculatedRoute.routePoints` and `TrajectoryGenerator::generate`;
- global-planner prohibition is checked across the Stage-2 trajectory-builder +
  execution text.

Production runtime code did not change for this fix.

## Current validation status

The restored Stage-2 viewer has still NOT reached target compilation because the latest
run stopped at the false architecture assertion.

Do not claim Stage-2 build/run PASS yet.

Known older Stage-2 regressions remain visible and are not part of the focused static
route build gate:
- old `navigation_runtime_planner`: adjusted-target fixture failure;
- old composite proving ground: narrow-passage/full-hull failure.

## Immediate target command

Pull latest and rerun focused gate:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If it fails, use the exact next architecture/compiler/linker output as the next
state-affecting event.

## Exact executable launch after successful build

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Always provide a separate exact executable launch command after build instructions that
produce an executable.

## Expected visual workflow after build

1. Scene is visible before calculation.
2. Press `РАССЧИТАТЬ`.
3. White four-point static route appears.
4. Button becomes `ЗАПУСТИТЬ ПОЛЁТ`.
5. Press it.
6. Cobra moves; white route stays unchanged.
7. Green actual path grows behind the Cobra.
8. Right diagnostics show Ruckig/Follower/Pilot chain.
9. On bad behavior inspect/send:
   - screenshot/video;
   - `tools/navigation_runtime/last_execution.log`;
   - if needed `last_execution_trace.json`.
10. Change pilot/control/style and rerun Stage 2; route must remain identical without a
    new Planner calculation.
