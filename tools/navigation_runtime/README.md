# Navigation Runtime — diagnostic stand

Location: `tools/navigation_runtime/`.

## Current mode: Stage 1 — static route construction

The stand is intentionally split into two stages.

### Stage 1 — current

`РАССЧИТАТЬ` builds one nominal route from START to FINISH through the
**static** obstacle set from `scenario.json`.

Current chain:

```text
scenario.json
  -> start / required waypoints / finish
  -> static NavigationObstacle geometry
  -> NominalRoutePlanner
  -> GeometricPathPlanner backend
  -> retained nominal polyline
  -> 3D display
```

The ship does **not** fly in Stage 1. No Follower, PilotSkill or physics result is
presented as if execution had been proved.

The calculated route is retained until either:
- the goal revision changes; or
- the static-world revision changes.

A dynamic-world revision by itself does **not** invalidate/rebuild the route.

### Stage 2 — next

Stage 2 will consume the retained Stage-1 route and add:
- local monitoring of moving objects;
- local temporary bypass / braking;
- physical maneuver generation;
- continuous swept-hull/tunnel proof;
- doctrine selection;
- AcceptedManeuverProgram;
- Follower / PilotSkill / authoritative physics;
- progressive reacquisition of the same nominal route.

Moving obstacles must not cause global route reconstruction.

## Corridor vs tunnel

The Stage-1 route uses a coarse navigation envelope:

```text
route_envelope_radius_m
route_clearance_m
```

This is a **route/corridor abstraction**, not exact collision truth.

Exact questions such as whether the Cobra clips a wall while rotating belong to
the later time-parameterized swept-hull **tunnel** proof.

## Dynamic-ready architecture

`scenario.json` already retains the schema for:
- moving obstacles with velocity;
- moving obstacles with route points + speed;
- a sudden obstacle.

Stage 1 parses these inputs but deliberately does not use them to rebuild the
static route. They are reserved for the Stage-2 local dynamic overlay.

`NominalRoutePlanner::ValidityQuery` explicitly contains a dynamic revision and
explicitly ignores it for nominal-route invalidation. Regression tests pin this
contract.

## UI

The existing selectors remain visible because Stage 2 will consume them:
- Assisted / Newtonian;
- Expert / Average / Loser;
- Standard / Extreme;
- sudden-obstacle checkbox.

During Stage 1 these controls do not change the nominal static route. The viewer
reports `ЭТАП 1 — МАРШРУТ` and displays one static calculation frame.

## scenario.json

Supported Stage-1 route inputs:
- start position;
- optional `ship_route_points` as ordered required checkpoints;
- finish position;
- static obstacles: box / sphere / capsule;
- `route_envelope_radius_m`;
- `route_clearance_m`.

Inputs already reserved for Stage 2:
- start velocity / attitude;
- final velocity / attitude requirements;
- standard/extreme speeds;
- moving obstacles;
- sudden obstacle.

## Build

From MSYS2 MinGW64:

```bash
cd /d/__elite/work
cmake -S tools/navigation_runtime -B build/tools/navigation_runtime -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tools/navigation_runtime
```

## Run executable

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

The window is an ordinary decorated Windows window, maximized to the desktop
work area.


## Pre-calculation scene and diagnostics

The authored scene is visible immediately on startup, before `РАССЧИТАТЬ`:
- green cross: START;
- yellow cross/ring: FINISH;
- grey wire geometry: static obstacles;
- reference grid: visual scale/orientation aid;
- Cobra body remains at START.

The white route does not exist until the Stage-1 planner runs.

After `РАССЧИТАТЬ`, the fixed diagnostics panel and console show the Stage-1 chain explicitly:

```text
SCENE: LOADED
PLANNER: OK / FAIL
FOLLOWER: NOT RUN (STAGE 1)
START / FINISH
STATIC OBSTACLES
REQUIRED WAYPOINTS
ROUTE POINTS
ROUTE LENGTH
STATIC DETOUR
GOAL / STATIC WORLD REVISION
```

The same calculation log is written to:
`tools/navigation_runtime/last_route_plan.log`.

Playback controls are deliberately disabled in Stage 1 and labeled as Stage-2 execution controls. A one-frame route result must never look like a failed Follower run.
