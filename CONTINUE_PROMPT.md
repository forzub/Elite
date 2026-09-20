# CONTINUE PROMPT — Elite Navigation Stage 1 static route + diagnostics

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
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`

After every state-affecting event, update the mandatory Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## Two-stage process

### Stage 1 — CURRENT

Build and inspect one retained nominal route through STATIC obstacles.

```text
START
 + optional required ship_route_points
 + FINISH
 + static NavigationObstacle geometry
        |
        v
NominalRoutePlanner
        |
        v
GeometricPathPlanner
        |
        v
sparse retained route polyline
        |
        v
3D viewer + diagnostics
```

No ship execution belongs to Stage-1 acceptance.

### Stage 2 — LATER

Consume the retained Stage-1 route and add:

```text
dynamic/local monitor
 -> temporary bypass / braking
 -> B5 physical maneuver candidates
 -> B6 continuous swept-hull/tunnel proof
 -> B7 decision
 -> B8 AcceptedManeuverProgram
 -> B9/B10 Follower
 -> B12 PilotSkill
 -> B13 authoritative physics
 -> B11/B14 event-driven monitor/replan
```

Do not reintroduce Follower/physics into Stage 1.

## Route ownership

`NominalRoutePlanner` owns the Stage-1 nominal static route.

Rebuild only when:
- goal revision changes; or
- static-world revision changes.

`dynamicWorldRevision` must not invalidate/rebuild the global nominal route.

Moving/sudden obstacles are already parsed as reserved Stage-2 inputs but are not
passed into Stage-1 route planning.

## Corridor vs tunnel

Stage-1 route envelope/clearance is only a coarse navigation/test abstraction.

It is not exact rigid-body collision truth.

Exact oriented-Cobra wall/aperture contact belongs to Stage-2 time-parameterized
swept-hull/tunnel proof.

## Current Stage-1 viewer behavior

A user screenshot showed a blank scene before calculation and ambiguity after calculation
because there was no motion.

This has been fixed architecturally and visually.

### Before Calculate

The authored scene must be visible immediately:
- green START cross;
- yellow FINISH cross/ring;
- static obstacle geometry;
- reference grid;
- Cobra at START.

`TraceDocument` now carries explicit:
- `hasSceneEndpoints`;
- `sceneStartMapMeters`;
- `sceneFinishMapMeters`.

These exist independently from a calculated route.

`fitCamera()` explicitly includes those endpoints.

### After Calculate

White route appears.

Fixed diagnostic panel must show an explicit chain such as:

```text
SCENE: LOADED
PLANNER: OK
FOLLOWER: NOT RUN (STAGE 1)
START: ...
FINISH: ...
STATIC OBSTACLES: ...
REQUIRED WAYPOINTS: ...
ROUTE POINTS: ...
ROUTE LENGTH: ...
STATIC DETOUR: YES/NO
GOAL REVISION: ...
STATIC WORLD REVISION: ...
LOG: last_route_plan.log
PLANNER MESSAGE: ...
```

On failure it must say `PLANNER: FAIL`.

The same calculation diagnostics are:
- printed to stdout with prefix `[NAV-STAGE1]`;
- written to `tools/navigation_runtime/last_route_plan.log`.

This log is the primary evidence for deciding whether Stage-1 Planner input/output is wrong.

## Playback controls

Stage 1 contains only one route-result frame.

Therefore playback is not a Stage-1 feature.

The UI must explicitly show:
- `ПОЛЁТ: ЭТАП 2`;
- `FOLLOWER: OFF`;
- no working frame/playback controls when only one frame exists.

SPACE / playback buttons must not create the illusion of a broken Follower.

## Stage-1 runtime boundary

`tools/navigation_runtime/NavigationScenarioRuntime.cpp` must not contain:
- `NavigationRuntimePlanner::plan`;
- periodic global replanning;
- `makeShortProgram`;
- Follower;
- PilotSkill;
- SharedShipPhysics;
- DynamicMotionSystem.

The Stage-1 viewer CMake must not link those execution components.

## Known older Stage-2 failures

Preserve them; do not weaken them to make full CTest green.

1. `navigation_runtime_planner`
   - failure: `fixture must produce a safe adjusted target`
   - old local-dynamic Stage-2 fixture.

2. `navigation_composite_proving_ground`
   - failure: `composite full hull exceeded narrow passage`
   - after dynamic Newtonian bypass, hand-authored narrow-passage execution exceeds
     the 19 m half-width gate.
   - this is future Stage-2 attitude/tunnel evidence, not Stage-1 route failure.

## Stage-1 target gate

Use the focused runner:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Exact Stage-1 test executable:

```bash
cd /d/__elite/work
./build/tests/navigation_runtime/nominal_route_planner_tests.exe
```

Exact viewer executable:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Always provide a separate exact executable launch command after build instructions that
produce an executable.

## Immediate acceptance check

Before Calculate:
- scene visible;
- START/FINISH visible;
- wall visible;
- diagnostics says Planner not run / Follower not run.

After Calculate:
- white route visible;
- planner diagnostics explicit;
- route length and point count visible;
- `last_route_plan.log` created;
- no movement expected;
- changing pilot/control law/flight style/sudden-obstacle must not alter the Stage-1 route.

If route geometry is wrong, ask for:
- screenshot;
- `tools/navigation_runtime/last_route_plan.log`.

Then fix Stage 1 only, update mandatory MD files, and recreate this prompt from scratch.
