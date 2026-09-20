# CURRENT TASK — Navigation Stage 1: static nominal route

**Date:** 2026-09-20  
**Status:** IMPLEMENTED IN REPO / TARGET VALIDATION PENDING

## Two-stage process

Navigation work is now deliberately split.

### Stage 1 — current

Build and display one nominal route:

```text
START
 + optional required checkpoints
 + FINISH
 + static obstacles
        |
        v
NominalRoutePlanner
        |
        v
sparse retained route polyline
```

No ship execution belongs to this gate.

### Stage 2 — later

Consume the retained Stage-1 route and add:

```text
dynamic/local monitor
 -> temporary bypass / braking
 -> physical maneuver generation
 -> continuous swept-hull/tunnel proof
 -> doctrine selection
 -> AcceptedManeuverProgram
 -> Follower
 -> PilotSkill
 -> authoritative physics
 -> event-driven invalidation/replan
```

Moving obstacles must not reconstruct the global nominal route.

## Implemented Stage-1 fixes

- Added `src/game/navigation/NominalRoutePlanner.h/.cpp`.
- Reused the shared `GeometricPathPlanner` as the static geometric backend.
- Route can include ordered authored `ship_route_points`.
- No arbitrary obstacle-count correctness cap is used by the new seam.
- Route validity is keyed by goal revision + static-world revision.
- Dynamic-world revision is explicitly **not** a nominal-route invalidation trigger.
- Added `NominalRoutePlannerTests.cpp`.
- Added architecture gate
  `check_navigation_stage1_nominal_route.py`.
- `tools/navigation_runtime` now calculates Stage 1 only.
- Removed the old stand-side periodic global replan loop and
  `makeShortProgram()` execution from `NavigationScenarioRuntime.cpp`.
- Stage-1 viewer no longer links Follower/PilotSkill/physics/NavigationMap runtime
  code.
- Viewer reports `ЭТАП 1 — МАРШРУТ`; the Cobra remains at START.
- `scenario.json` exposes goal/static/dynamic revisions plus coarse
  `route_envelope_radius_m` and `route_clearance_m`.

## Corridor vs tunnel

Stage-1 envelope is only a coarse route/corridor abstraction.

It is **not** exact collision truth.

Whether the real oriented Cobra clips a wall/aperture belongs to Stage-2
time-parameterized swept-hull/tunnel proof.

## Dynamic architecture already reserved

The JSON schema still parses:
- moving obstacles with velocity;
- moving obstacles with route points + speed;
- sudden obstacle.

Stage 1 deliberately does not feed them to `NominalRoutePlanner`.

## Target validation required now

From MSYS2 MinGW64:

### 1. Pull and record HEAD

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
```

### 2. Architecture contracts

```bash
cd /d/__elite/work
bash tests/architecture_contracts/run_mingw64.sh
```

### 3. Runtime tests

```bash
cd /d/__elite/work
cmake -S tests/navigation_runtime -B build/tests/navigation_runtime -G Ninja
cmake --build build/tests/navigation_runtime
ctest --test-dir build/tests/navigation_runtime --output-on-failure
```

Exact new Stage-1 test executable:

```bash
cd /d/__elite/work
./build/tests/navigation_runtime/nominal_route_planner_tests.exe
```

### 4. Viewer build

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

## Visual acceptance for Stage 1

With the default wall scenario:
- the white route starts at START and ends at FINISH;
- it must detour around the static wall rather than pass through it;
- Cobra must remain at the start pose; there is no fake flight animation;
- status must read `СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ`;
- explanation must explicitly say Stage 1 and that flight is not yet calculated;
- changing pilot / control law / flight style / sudden-obstacle checkbox must not
  change the Stage-1 static route.

Do not begin Stage 2 until this Stage-1 target gate is verified.


## Architecture-gate compatibility fix

During pre-handoff audit, the existing `check_geometric_path_planner.py` was found to
encode an obsolete ownership assumption: it required the same geometric planner .cpp
to appear twice in root CMake. Current runtime architecture already compiles it once in
shared `EliteNavigationGeometry`, which both client/server navigation runtime reuse.
The checker now pins that shared-library ownership instead of duplicate compilation.
This is an architecture-test correction only; route behavior is unchanged.


## Updated immediate target gate

Do **not** use the full 20-test runtime suite as the Stage-1 acceptance gate.
It contains intentionally retained Stage-2 regressions.

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

Known Stage-2 failures remain open and must not be deleted or weakened:
- `navigation_runtime_planner`: adjusted-target fixture no longer demonstrates a safe
  bypass under current B4 semantics;
- `navigation_composite_proving_ground`: after the dynamic Newtonian bypass, the
  hand-authored narrow-passage program enters portal 102 with a full-hull width above
  the 19 m half-width gate. This is a Stage-2 body-attitude/tunnel issue, not a
  Stage-1 route-planning issue.
