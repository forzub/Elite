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

After every state-affecting event synchronize the mandatory Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## User intent / correction

The user explicitly rejected removing Follower from the diagnostic viewer.

The two stages are responsibilities inside ONE viewer:

```text
Stage 1 — РАССЧИТАТЬ
static world -> Planner -> retained white route

Stage 2 — ЗАПУСТИТЬ ПОЛЁТ
same retained route -> trajectory -> Follower -> PilotSkill -> physics -> visible Cobra motion
```

Do not interpret "two stages" as "route only, no movement".

## Stage 1 — canonical static route

`NominalRoutePlanner` builds the route once from:
- START;
- optional ordered `ship_route_points`;
- FINISH;
- static NavigationObstacle geometry.

It reuses `GeometricPathPlanner`.

Nominal route invalidation:
- goal revision change -> rebuild;
- static-world revision change -> rebuild;
- dynamic-world revision change -> DO NOT rebuild.

Moving objects must not reconstruct the global route.

Last target-verified Stage-1 result:
- target checkout at that time: `5da0be0d05ef91958a0e7dc9adda3b4eb8fdee29`;
- `nominal_route_planner`: PASS;
- viewer Stage-1 log from user:
  - PLANNER: OK
  - START: (0,0,0)
  - FINISH: (300,0,0)
  - STATIC OBSTACLES: 1
  - REQUIRED WAYPOINTS: 0
  - ROUTE POINTS: 4
  - ROUTE LENGTH: 323.75 m
  - STATIC DETOUR: YES.

This is current evidence that Planner can build the default static-wall route.

## Scene preview

Before calculation the viewer must already show:
- reference grid;
- green START;
- yellow FINISH cross/ring;
- static obstacles;
- Cobra at START.

`TraceDocument` carries authored scene endpoints independently from route existence.

## Immutable retained route

After successful Stage 1 the viewer stores an immutable copy of that route in AppState.

Stage 2 always consumes that retained copy.

Changing:
- Assisted/Newtonian;
- Expert/Average/Loser;
- Standard/Extreme;
- reserved sudden-obstacle toggle

after an execution run restores the same retained white route and invalidates only the
execution result. The next Stage-2 run must NOT call Planner again.

This is required for clean apples-to-apples execution comparisons.

## Stage 2 — current static execution implementation

After successful Stage 1 the button becomes `ЗАПУСТИТЬ ПОЛЁТ`.
SPACE may also start Stage 2.

Current chain:

```text
retained Stage-1 routePoints
 -> world::navigation::TrajectoryGenerator
 -> game::navigation::RuckigRoutePlanner backend
 -> time-parameterized trajectory
 -> AcceptedManeuverProgram chunks (max 16 reference samples each)
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics
 -> DynamicMotionSystem
 -> playback trace
```

Hard ownership rule:
`executeCalculatedRoute()` must NOT call:
- `NominalRoutePlanner::plan`;
- `NavigationRuntimePlanner::plan`;
- `GeometricPathPlanner::plan`.

It only consumes `calculatedRoute.routePoints`.

The old stand-local `makeShortProgram()` quintic shortcut must remain absent.

## Stage-2 execution parameters

Cobra diagnostic physics mirrors current Cobra baseline:
- angular accel 3 rad/s²;
- max pitch/yaw 2.5 rad/s;
- max roll 3 rad/s;
- max linear G envelope 7.5g;
- manoeuvre/RCS acceleration 2 m/s²;
- real RCS gas pressure/use/recharge is active;
- body half extents used by viewer: 13 x 2.5 x 11.1 m.

Pilot selector is real:
- Expert: zero reaction/latency, high bandwidth;
- Average: finite reaction/latency, lower response, small deterministic error;
- Loser: larger delay/latency, lower response, larger deterministic error.

Bridge is reset at revision zero so first real goal revision exercises reaction delay.

Flight style:
- Standard uses `standard_speed_mps`;
- Extreme uses `extreme_speed_mps`.

Control law:
- Newtonian uses physical one-direction main thrust; reference attitude follows required
  acceleration when significant so the hull must rotate for thrust/braking.
- Assisted uses the Assisted physical law and generally velocity-aligned transit attitude.

Final forward/up orientation is blended in over the last ~35 m rather than snapped at
the final sample.

Current Ruckig backend explicitly rejects non-zero final speed; do not silently ignore
that unsupported contract.

## Dynamic boundary

Current work is STATIC obstacles first.

Dynamic/sudden obstacle JSON remains parsed/reserved but Stage 2 currently reports:

`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`

Do not implement moving-object avoidance until retained-route static execution is target
validated.

When dynamic avoidance is added later it must operate locally over the retained route;
it must not rebuild global route because a moving object changed.

## Corridor vs tunnel

The coarse `route_envelope_radius_m` / `route_clearance_m` is a navigation/test
abstraction.

Current static Stage-2 execution checks the route/envelope against static geometry and
reports `COARSE STATIC CONTACT`.

This is NOT final B6 exact physical proof.

The future tunnel is the time-parameterized swept volume of the real oriented hull.
Only that layer decides exact wall/aperture clipping during rotations.

Do not claim full navigation acceptance before that B6 tunnel proof exists.

## Diagnostics

Stage 1:
- stdout `[NAV-STAGE1]`;
- `tools/navigation_runtime/last_route_plan.log`;
- `tools/navigation_runtime/last_calculated_trace.json`.

Stage 2:
- stdout `[NAV-STAGE2]`;
- `tools/navigation_runtime/last_execution.log`;
- `tools/navigation_runtime/last_execution_trace.json`.

Stage-2 panel/log separates:
- cached Planner route;
- Ruckig trajectory;
- trajectory sample count;
- program chunks;
- Follower;
- Pilot bridge;
- pilot profile;
- flight style;
- control law;
- execution frames;
- final position/speed;
- max route deviation;
- max Follower error;
- coarse static contact.

Failed multi-frame execution is still playable for diagnosis.

## Known older Stage-2 regressions

Do not hide or weaken:
1. `navigation_runtime_planner` old fixture:
   `fixture must produce a safe adjusted target`.
2. `navigation_composite_proving_ground`:
   `composite full hull exceeded narrow passage`.

These are not the focused static route build gate but remain evidence for future
dynamic/tunnel work.

## Architecture checker

`tests/architecture_contracts/check_navigation_stage1_nominal_route.py` now pins
separation rather than absence of Follower:
- Stage 1 may call Planner but no execution stack;
- Stage 2 must consume retained route and contain Follower/Pilot/physics;
- Stage 2 may not call a global planner;
- viewer retains immutable Stage-1 route for repeated mode comparisons.

## Immediate target validation

Current restored Stage-2 path is NOT yet target-compiled.

Run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If compile/link fails, use exact target output as next state-affecting event.

Exact executable launch:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Always provide a separate exact executable launch command after build instructions that
produce an executable.

## Expected viewer workflow

1. Scene visible before calculation.
2. Press `РАССЧИТАТЬ`.
3. Confirm same white static route.
4. Button becomes `ЗАПУСТИТЬ ПОЛЁТ`.
5. Press it.
6. Cobra must visibly move while white route remains fixed.
7. Green actual path grows behind Cobra.
8. Right panel shows Ruckig/Follower/Pilot state.
9. On bad behavior send:
   - screenshot/video;
   - `tools/navigation_runtime/last_execution.log`;
   - if needed `last_execution_trace.json`.
10. Change pilot/control/style after the run and execute again; the route must remain
    identical without another Planner calculation.

If target build/execution fails, fix this static retained-route execution path first,
update mandatory MD files, and recreate this prompt from scratch.
