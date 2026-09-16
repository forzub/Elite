# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / Navigation Map runtime foundation  
**Current architecture contract:** `NAVIGATION_WORLD_V2.md`  
**Remote branch:** `main`

## Current state

Navigation v2 is now the active game-development focus. The accepted direction is one central ship-centered `NavigationWorld` with a translating working origin, stable navigation/travel axes, shared spatial indexing, dynamic actor prediction and centralized broadphase/conflict filtering.

Authoritative system/world state remains the physical source of truth. Hub/station navigation remains its own local `prograde/radial/normal` domain and contributes only the subset relevant to the active ship/navigation horizon.

Dynamic actors are stored as compact position/velocity/acceleration/bounds/flags data rather than as dense per-voxel velocity/acceleration fields. GPU work is currently targeted at massively parallel spatial binning, prediction/swept volumes, corridor filtering and shared broadphase; sparse single-query precision graph search remains an asynchronous CPU-worker responsibility unless benchmarks show otherwise.

Ruckig is downstream trajectory/kinematics generation, not obstacle pathfinding. The manual guidance tunnel must visualize the same accepted trajectory/corridor actually used by navigation/control.

All navigation work on the frame path must be asynchronous/non-blocking. The main thread must not synchronously wait for GPU readback or a long planner.

The raw `Shift+F12` NavigationWorld diagnostic view is an accepted contract in `NAVIGATION_WORLD_V2.md`, but its runtime implementation is not yet present on remote `main`. It must consume the same completed backend-neutral NavigationWorld snapshot used by navigation/control and must not build an independent visualization world.

## Fresh local validation — 2026-09-16

The user's current MinGW64 working tree successfully launches the canonical graphical client and exits normally on window close. The user also reported successful client/server smoke tests:

```text
EliteGame.exe --self-test-fast-universe
[PASS] fast-universe real-scene smoke

EliteServer.exe --self-test
[PASS] headless-server boot + two-session authoritative routing smoke
```

The existing navigation guidance suite passed all three tests, including `ruckig_route_planner` and `guidance_tunnel_local_horizon`.

The full `tests/run_all_mingw64.sh` ready gate is **not fully green**: it completed with three failed contract blocks while the canonical client/server builds, standalone runtime and network-process acceptance still passed. The currently reported failures are:

1. `CROSS-TIMELINE + DIAGNOSTIC CONTRACTS` — `Hub guidance cylinder is no longer static`;
2. `SYSTEM MAP BEHAVIOR + ARCHITECTURE` — missing contract token `Keep the first physical sample`;
3. `FEATURE SURFACE CONTRACTS` — missing debug-UI compatibility token around `m_gameUiHttpPort = m_htmlUi.start(requestedWebUiPort, webUiRoot);`.

Do not report this ready gate as PASS until those three failures are classified and resolved or the corresponding stale contracts are deliberately updated.

In the supplied live-client frame sample:

```text
dock_request=0 dock_plan=0 tunnel_builds=0
```

Therefore no docking route request, docking plan or guidance-tunnel build occurred in that observed frame/session. This does **not** mean the older route/guidance machinery is absent: its dedicated tests pass. It does mean the new NavigationWorld v2 route/debug integration is not yet demonstrated by this live run.

## Immediate task

Build the standalone NavigationWorld backend benchmark before committing production architecture to a GPU, CPU spatial-index or hybrid backend.

Benchmark at least:

- 1,000 / 5,000 / 10,000 moving actors;
- ship-centered 3D working space;
- dynamic spatial binning/indexing;
- P/V/A prediction and swept bounds;
- player-corridor filtering;
- shared all-agent neighbour/conflict broadphase;
- GPU time, CPU setup/query time, memory, candidate counts and readback volume/stalls.

The benchmark result is the decision gate. See `NAVIGATION_WORLD_V2.md` for the full architecture and performance contract.

## Remote / local distinction

The remote repository contains the NavigationWorld v2 architecture documentation and raw diagnostic-view contract.

The user's newer `D:/__elite/work/tests/navigation_map` implementation/test work is still treated as **LOCAL / UNPUSHED** until it appears on remote `main` and is verified there. The user previously reported:

```text
bash tests/navigation_map/run_mingw64.sh
1/1 Test #1: navigation_map ... Passed 0.04 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.05 sec
```

Do not imply that remote `main` contains those local files until they are pushed and verified.

The current remote CMake baseline does not expose a dedicated NavigationWorld/navigation library target: navigation implementation files are compiled into `EliteGame`, while navigation tests compile their required navigation sources directly. If the local working tree introduces a navigation library target, the associated `CMakeLists.txt` and related build-boundary changes are also local state and must be preserved when that work is pushed.

## Historical project state

The previous accumulated `PROJECT_STATE.md` content through 2026-09-13 is preserved verbatim as:

`PROJECT_STATE_HISTORY_THROUGH_2026-09-13.md`

That file contains historical Model Asset Editor/render/navigation records. It is retained for traceability, but its old `Current ...` headings are historical snapshots and must not be interpreted as the current project task.

Git history also preserves every prior version.

## Documentation Definition of Done

A meaningful project iteration is not complete while its Markdown state is stale.

Before declaring an iteration complete or beginning the next coding slice:

1. update this file when overall project focus/state changes;
2. update the affected subsystem contract/specification such as `NAVIGATION_WORLD_V2.md` when architecture/runtime/performance contracts change;
3. synchronize the private project-context `CURRENT_STATE.md`, `CURRENT_TASK.md`, `ITERATION_LOG.md`, decisions and source baseline as applicable;
4. mark local-only work explicitly as `LOCAL / UNPUSHED`;
5. verify that dates, active task and repository baseline do not contradict the actual work.

Stale project-state documentation is a project defect and blocks handoff to the next slice.
