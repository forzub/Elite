# CONTINUE PROMPT — Elite Navigation Stage 1 static route

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Before changing behavior read:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`;
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`;
- `src/game/navigation/NominalRoutePlanner.h/.cpp`;
- `tools/navigation_runtime/README.md`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`.

After every state-affecting project event update the Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## Current project split

Navigation work is now explicitly divided into two stages.

### Stage 1 — CURRENT

Build one retained nominal route through **static** obstacles.

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
GeometricPathPlanner backend
        |
        v
sparse retained route polyline
        |
        v
3D viewer
```

No flight/execution is part of this acceptance gate.

### Stage 2 — LATER

Consume the retained Stage-1 route and add:

```text
dynamic/local monitor
 -> temporary bypass / braking
 -> B5 physical maneuver candidate(s)
 -> B6 continuous swept-hull/tunnel proof
 -> B7 decision
 -> B8 AcceptedManeuverProgram
 -> B9/B10 Follower
 -> B12 PilotSkill
 -> B13 authoritative physics
 -> B11/B14 monitor + event-driven replan
```

Do not start Stage 2 until the user target-validates Stage 1.

## Stage-1 implementation now in repo

New production-facing component:
- `src/game/navigation/NominalRoutePlanner.h`;
- `src/game/navigation/NominalRoutePlanner.cpp`.

It reuses the existing shared `GeometricPathPlanner` backend.

The Stage-1 request contains:
- `goalRevision`;
- `staticWorldRevision`;
- start;
- finish;
- ordered required waypoints;
- static obstacles;
- coarse navigation envelope radius;
- coarse route clearance.

The returned plan contains:
- validity;
- goal/static source revisions;
- sparse route points;
- route length;
- whether a static detour was required.

## Invalidation contract

A nominal global/static route is rebuilt only when:
- goal revision changes; or
- static-world revision changes.

`dynamicWorldRevision` is deliberately present in
`NominalRoutePlanner::ValidityQuery` but deliberately ignored by
`invalidationReason()`.

Changing dynamic actors must **not** rebuild the nominal route.

A regression test pins this behavior:
`tests/navigation_runtime/NominalRoutePlannerTests.cpp`.

## Corridor vs tunnel

The Stage-1 route envelope is a coarse navigation/test abstraction.

It is NOT exact physical collision truth.

Exact questions such as whether the oriented Cobra clips a wall while rotating
belong to the later time-parameterized swept-hull **tunnel** proof in Stage 2.

Do not turn Stage-1 route generation into exact rigid-body trajectory proof.

## Important cleanup already completed

The old transitional live-stand flight path has been removed from the Stage-1
runtime, not merely disabled.

`tools/navigation_runtime/NavigationScenarioRuntime.cpp` no longer contains:
- `NavigationRuntimePlanner::plan`;
- periodic 0.25/0.5 s global replanning;
- `makeShortProgram`;
- Follower execution;
- PilotSkill execution;
- SharedShipPhysics;
- DynamicMotionSystem.

`tools/navigation_runtime/CMakeLists.txt` now links only:
- `NominalRoutePlanner.cpp`;
- `GeometricPathPlanner.cpp`;
- `NavigationObstacleGeometry.cpp`;
- viewer/trace/json/OpenGL dependencies.

Do not reintroduce execution code into Stage 1.

## Dynamic-ready scenario architecture

`scenario.json` still parses and preserves:
- moving obstacles with velocity;
- moving obstacles with route points + speed;
- sudden obstacle;
- `dynamic_world_revision`.

These are reserved Stage-2 inputs.

Stage 1 must not pass them into `NominalRoutePlanner`.

Current revision fields in JSON:
- `goal_revision`;
- `static_world_revision`;
- `dynamic_world_revision`.

Current route abstraction fields:
- `route_envelope_radius_m`;
- `route_clearance_m`.

## Viewer behavior

On `РАССЧИТАТЬ`:
- build one static route;
- show the white route polyline;
- show static obstacles;
- keep Cobra at START;
- do not animate a fake flight;
- phase/status should report Stage 1 static route state.

Pilot/control-law/flight-style/sudden-obstacle UI selectors remain visible for
future Stage 2, but changing them must not change the Stage-1 route.

## Architecture gate

New checker:
`tests/architecture_contracts/check_navigation_stage1_nominal_route.py`.

It pins:
- dynamic revision cannot invalidate the nominal route;
- Stage-1 viewer contains no execution stack;
- Stage-1 tool CMake does not link Follower/PilotSkill/physics/runtime planner;
- static wall detour and required waypoint regressions exist;
- corridor/tunnel ownership remains separated.

## Evidence boundary

Earlier green tests remain valid only for the scope they actually proved:
- component/unit proof;
- execution of authored AcceptedManeuverProgram;
- planner/topology slice;
- authoritative authored-world integration.

Do not call those collectively a full autonomous navigation proof.

Stage 1 acceptance proves only static route construction.

## Immediate next action: target validation

User must run from MSYS2 MinGW64.

Pull and record HEAD:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
```

Architecture contracts:

```bash
cd /d/__elite/work
bash tests/architecture_contracts/run_mingw64.sh
```

Runtime tests:

```bash
cd /d/__elite/work
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime
ctest --test-dir build/tests/navigation_runtime --output-on-failure
```

Exact new test executable launch:

```bash
cd /d/__elite/work
./build/tests/navigation_runtime/nominal_route_planner_tests.exe
```

Viewer build:

```bash
cd /d/__elite/work
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

Exact viewer executable launch:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Always provide the exact executable launch command after any build instruction
that produces an executable.

## Stage-1 visual acceptance

Default wall scenario must show:
- route starts at START;
- route ends at FINISH;
- route detours around the wall;
- no Cobra flight animation;
- `СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ`;
- explanation says Stage 1 / flight not yet calculated;
- changing pilot/control/flight-style/sudden-obstacle settings does not alter
  the static route.

If target build/test fails, fix Stage 1 only, update all MD state files, and
recreate this prompt from scratch again.
