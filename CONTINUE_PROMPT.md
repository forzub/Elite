# CONTINUE PROMPT — Elite Navigation Stage 1 static route

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read first:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`;
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`;
- `src/game/navigation/NominalRoutePlanner.h/.cpp`;
- `tools/navigation_runtime/README.md`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`.

After every state-affecting event update the project Markdown state and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## Current project structure

Navigation is deliberately split into two stages.

### Stage 1 — CURRENT

Build one retained nominal route through STATIC obstacles:

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
3D viewer
```

No ship execution belongs to the Stage-1 gate.

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

Do not start Stage 2 until Stage 1 is target-validated.

## Stage-1 production seam

Files:
- `src/game/navigation/NominalRoutePlanner.h`;
- `src/game/navigation/NominalRoutePlanner.cpp`.

The request owns:
- goal revision;
- static-world revision;
- start;
- finish;
- ordered required waypoints;
- static obstacles;
- coarse route/corridor envelope radius;
- coarse route clearance.

The result owns:
- valid/invalid;
- source goal/static revisions;
- sparse route points;
- route length;
- whether a static detour was needed.

The backend is the existing shared `GeometricPathPlanner`.

Do not create a second path-search engine.

## Route invalidation contract

Rebuild nominal route only when:
- goal revision changes; or
- static-world revision changes.

`dynamicWorldRevision` is intentionally present in
`NominalRoutePlanner::ValidityQuery` and intentionally ignored by
`invalidationReason()`.

Moving actors must not reconstruct the nominal/global route.

Regression:
`tests/navigation_runtime/NominalRoutePlannerTests.cpp`
contains `testDynamicRevisionDoesNotInvalidateNominalRoute`.

## Corridor vs tunnel

Stage-1 `route_envelope_radius_m` / `route_clearance_m` are coarse
navigation/test abstractions.

They are NOT exact physical collision proof.

Exact oriented-Cobra wall/aperture contact belongs to Stage-2
time-parameterized swept-hull **tunnel** proof.

Do not turn Stage 1 into rigid-body trajectory integration.

## Stage-1 live diagnostic runtime

`tools/navigation_runtime/NavigationScenarioRuntime.cpp` was cleaned to route-only.

It no longer contains:
- `NavigationRuntimePlanner::plan`;
- 0.25/0.5 s periodic global replanning;
- `makeShortProgram`;
- Follower;
- PilotSkill;
- SharedShipPhysics;
- DynamicMotionSystem.

`tools/navigation_runtime/CMakeLists.txt` now links only:
- `NominalRoutePlanner.cpp`;
- `GeometricPathPlanner.cpp`;
- `NavigationObstacleGeometry.cpp`;
- viewer/trace/JSON/OpenGL dependencies.

Do not reintroduce execution stack code into Stage 1.

## Dynamic-ready scenario schema

`scenario.json` exposes:
- `goal_revision`;
- `static_world_revision`;
- `dynamic_world_revision`;
- `route_envelope_radius_m`;
- `route_clearance_m`.

It already parses and preserves Stage-2 inputs:
- moving obstacles with velocity;
- moving obstacles with route points + speed;
- sudden obstacle;
- start velocity/body attitude;
- final velocity/attitude requirements;
- flight-style speeds.

Stage 1 deliberately does not pass moving/sudden actors into
`NominalRoutePlanner`.

## Viewer behavior

On `РАССЧИТАТЬ`:
- build one static route;
- show the white route polyline;
- show static obstacles;
- keep Cobra at START;
- do not animate flight;
- report `ЭТАП 1 — МАРШРУТ`;
- report `СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ`.

Pilot/control-law/flight-style/sudden-obstacle UI selectors remain visible for
future Stage 2. Changing them must not change the Stage-1 route.

## Tests / architecture gates

New:
- `tests/navigation_runtime/NominalRoutePlannerTests.cpp`;
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`.

The Stage-1 architecture checker pins:
- dynamic revision cannot invalidate nominal route;
- route runtime has no execution/replan stack;
- Stage-1 tool CMake has no Follower/PilotSkill/physics/runtime-planner link;
- static wall detour exists;
- required authored waypoint is preserved;
- corridor/tunnel ownership remains separate.

Pre-handoff audit also corrected
`tests/architecture_contracts/check_geometric_path_planner.py`:
the old checker incorrectly required duplicate compilation of
`GeometricPathPlanner.cpp`. Current correct ownership is:
- compile once in shared `EliteNavigationGeometry`;
- reuse through `EliteNavigationWorldRuntime` in client/server.

Do not restore duplicate .cpp ownership.

## Evidence boundary

Earlier green tests retain their narrow value:
- component/unit proof;
- authored-program execution;
- planner/topology slice;
- authoritative authored-world integration.

They are not collectively a full autonomous end-to-end proof.

Stage 1 acceptance proves only static route construction.

## Immediate next action: target validation

From MSYS2 MinGW64:

### Pull and record HEAD

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
```

### Architecture contracts

```bash
cd /d/__elite/work
bash tests/architecture_contracts/run_mingw64.sh
```

### Runtime tests

```bash
cd /d/__elite/work
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime
ctest --test-dir build/tests/navigation_runtime --output-on-failure
```

Exact Stage-1 test executable launch:

```bash
cd /d/__elite/work
./build/tests/navigation_runtime/nominal_route_planner_tests.exe
```

### Viewer build

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

Always provide the exact executable launch command after compilation that
produces an executable.

## Stage-1 visual acceptance

Default wall scenario must show:
- route START -> FINISH;
- static-wall detour;
- no fake Cobra flight;
- static route ready status;
- Stage-1 explanation;
- identical route when pilot/control law/flight style/sudden obstacle controls
  are changed.

If a target check fails, fix Stage 1 only, update mandatory MD state files and
recreate this prompt from scratch.
