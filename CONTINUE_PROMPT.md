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

One executable, two responsibilities:

```text
Stage 1 — РАССЧИТАТЬ
static scene -> NominalRoutePlanner -> retained white route

Stage 2 — ЗАПУСТИТЬ ПОЛЁТ
same retained route
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator / RuckigRoutePlanner
 -> AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge / PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> visible Cobra motion / green actual path
```

Stage 2 must NEVER call a global planner. It consumes the immutable Stage-1 route.

## Last target-verified Stage-1 evidence

User target run proved:
- `nominal_route_planner`: PASS;
- default viewer Planner result:
  - START = (0,0,0)
  - FINISH = (300,0,0)
  - static obstacles = 1
  - route points = 4
  - route length = 323.75 m
  - static detour = YES.

## Route ownership

Rebuild nominal route only when:
- goal revision changes; or
- static-world revision changes.

`dynamicWorldRevision` does not invalidate/rebuild the nominal route.

Moving/sudden obstacles are reserved for later local dynamic avoidance.

## Immutable route comparisons

Viewer stores the successful Stage-1 TraceDocument separately.
Changing pilot/control/style after an execution run restores the exact same retained
white route. The next Stage-2 run must reuse it without Planner.

This is required for clean Expert/Average/Loser and Newtonian/Assisted comparisons.

## Stage-2 current static execution scope

Implemented in code but NOT target-compiled yet:
- Ruckig time parameterization of retained route;
- real TrajectoryFollower;
- real PilotSkillExecutor via NavigationRuntimeControlBridge;
- SharedShipPhysics + DynamicMotionSystem;
- Standard/Extreme speed;
- Newtonian/Assisted control-law execution;
- final position / zero final speed;
- blended final forward/up orientation;
- 120 Hz simulation and ~30 Hz trace;
- route deviation / follower error / final-state diagnostics.

Current Ruckig backend explicitly rejects non-zero terminal speed.

Dynamic avoidance is not yet enabled and must report:
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`.

## Corridor vs tunnel

Stage-1 route envelope is only a coarse navigation/test abstraction.

Current Stage-2 static-contact reporting is also coarse.

Exact physical wall/aperture clipping by a rotating Cobra belongs to future B6
time-parameterized swept-hull/tunnel proof.

Do not claim full navigation acceptance before that exists.

## Diagnostics

Stage 1:
- stdout `[NAV-STAGE1]`
- `tools/navigation_runtime/last_route_plan.log`
- `tools/navigation_runtime/last_calculated_trace.json`

Stage 2:
- stdout `[NAV-STAGE2]`
- `tools/navigation_runtime/last_execution.log`
- `tools/navigation_runtime/last_execution_trace.json`

## Latest target event

The focused gate has twice stopped on false architecture-checker assertions before
reaching C++ compilation.

First false negative:
- checker expected `TrajectoryGenerator::generate` directly inside
  `executeCalculatedRoute()`;
- real ownership is:
  `executeCalculatedRoute -> buildExecutionTrajectory -> TrajectoryGenerator::generate`.
- checker was fixed to inspect the helper separately.

Second false negative:
- checker reported missing
  `DynamicMotionSystem::applySystemAccelerationDemand`;
- runtime call exists but is formatted across lines:
  `DynamicMotionSystem::\n applySystemAccelerationDemand`;
- checker was doing raw single-line substring matching.

Latest fix:
- added `compact_cpp()` whitespace normalization;
- Stage-2 execution and trajectory-builder call checks are now formatting-insensitive.

No runtime behavior changed in either checker fix.

## Immediate target action

Pull latest and rerun:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

The important next evidence is whether the gate now reaches actual C++ compile/link.

## Exact executable launch after successful build

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Expected workflow:
1. scene visible;
2. `РАССЧИТАТЬ`;
3. same four-point white route;
4. `ЗАПУСТИТЬ ПОЛЁТ`;
5. Cobra moves along retained route;
6. green actual path grows;
7. right diagnostics show Ruckig/Follower/Pilot chain.

If target build/runtime fails, use exact output as next state-affecting event, update all
mandatory MD files, and recreate this prompt from scratch.
