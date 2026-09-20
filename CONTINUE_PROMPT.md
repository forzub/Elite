# CONTINUE PROMPT — Elite Navigation Stage 1 static route

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

After every state-affecting event, synchronize the Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch**.

## Two-stage navigation process

### Stage 1 — CURRENT

Build one retained nominal route through STATIC obstacles.

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

No flight/execution belongs to Stage-1 acceptance.

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

Do not start Stage 2 until Stage 1 is target-validated visually.

## Stage-1 route ownership

`NominalRoutePlanner` is the production-facing Stage-1 seam.

Its route is invalidated only when:
- goal revision changes; or
- static-world revision changes.

`dynamicWorldRevision` is explicitly present in the validity query and explicitly
ignored for nominal-route invalidation.

Moving objects must not rebuild the global nominal route.

The Stage-1 route envelope is only a coarse navigation/test abstraction.
It is NOT exact physical collision truth.

Exact oriented-Cobra wall/aperture contact belongs to Stage-2 time-parameterized
swept-hull/tunnel proof.

## Stage-1 diagnostic runtime

`tools/navigation_runtime/NavigationScenarioRuntime.cpp` is route-only.

It must not contain or link:
- `NavigationRuntimePlanner::plan`
- periodic global replanning
- `makeShortProgram`
- Follower execution
- PilotSkill execution
- SharedShipPhysics
- DynamicMotionSystem

The JSON may still parse moving/sudden obstacles as reserved Stage-2 inputs, but
Stage 1 must not feed them into `NominalRoutePlanner`.

Viewer behavior on `РАССЧИТАТЬ`:
- build one static route;
- show the white route polyline;
- show static obstacles;
- keep Cobra at START;
- do not animate flight;
- report `ЭТАП 1 — МАРШРУТ`;
- report `СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ`.

Pilot/control-law/flight-style/sudden-obstacle controls remain visible for Stage 2,
but changing them must not alter the Stage-1 route.

## Target evidence received

User target checkout:
`5da0be0d05ef91958a0e7dc9adda3b4eb8fdee29`.

Observed target results:
- new `nominal_route_planner`: PASS;
- 18/20 runtime tests passed;
- `navigation_runtime_planner`: FAIL — old adjusted-target Stage-2 fixture;
- `navigation_composite_proving_ground`: FAIL — full hull exceeded the 19 m
  narrow-passage half-width after a dynamic Newtonian bypass;
- architecture suite stopped at an obsolete `...Map...` navigation control field
  name in `check_navigation_live_runtime_control.py`.

Interpretation:
- Stage-1 behavioral route test is green on the target machine;
- the two runtime failures are retained Stage-2 regressions, not Stage-1 route failures;
- the architecture failure was a stale checker after the production API migrated to
  explicit `...System...` frame names.

## Fixes after target output

- Updated `check_navigation_live_runtime_control.py` to:
  - `navigationLinearAccelerationDemandSystemMps2`
  - `navigationAngularAccelerationDemandSystemRadPerSec2`
  - System-frame bridge snapshot names
  - `applySystemAccelerationDemand`
- Labeled `nominal_route_planner` with CTest label `navigation_stage1`.
- Added focused target runner:
  `tests/navigation_runtime/run_stage1_mingw64.sh`.

The focused runner intentionally checks only:
1. shared geometric-path architecture;
2. Stage-1 nominal-route architecture;
3. Stage-1 nominal-route behavioral test;
4. Stage-1 viewer build.

This does NOT hide Stage-2 failures. They remain recorded and must stay visible.

## Known Stage-2 failures — preserve, do not weaken

### navigation_runtime_planner

Failure:
`fixture must produce a safe adjusted target`.

The legacy fixture no longer demonstrates a safe bypass under current B4 semantics.
Do not change production safety merely to make this old fixture green. Revisit in Stage 2.

### navigation_composite_proving_ground

Failure:
`composite full hull exceeded narrow passage`.

The log shows:
- dynamic bypass succeeds;
- continuation succeeds;
- planner returns to nominal topology;
- the following hand-authored narrow-passage program enters portal 102 with body
  attitude/hull projection that exceeds the 19 m half-width gate.

This is a Stage-2 physical attitude/tunnel issue. Preserve it as evidence.

## Immediate target command

Run:

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

Always give the exact executable launch command after compilation that produces an executable.

## Stage-1 visual acceptance

Default wall scenario must show:
- white route from START to FINISH;
- route detours around the static wall;
- Cobra remains at START;
- no fake flight animation;
- status `СТАТИЧЕСКИЙ МАРШРУТ ГОТОВ`;
- Stage-1 explanation;
- changing pilot/control law/flight style/sudden obstacle does not change the route.

If this focused gate or viewer fails, fix Stage 1 only, update all mandatory MD files,
and recreate this prompt from scratch again.
