# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / NavigationMap runtime foundation  
**Current architecture contracts:** `src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md`, `NAVIGATION_WORLD_V2.md`  
**Canonical development branch:** `main`

## Repository state

`main` is the only canonical game-development branch.

On 2026-09-16 the accidentally divergent histories of old `main`, `chatgpt/mae-v01075-semantic-workflow-motion-v5`, and the user's local line published temporarily as `rescue/local-97500` were audited and reconciled into `main`. The reconciliation merge is `9352fe7589ec109827e9633403d81bba46bdc926`.

The content audit established that the rescue and remote development lines carried the same current NavigationMap implementation. The remote development line additionally contained the later `NAV-V2-MAP-2` contract fix and CPU benchmark. Old `main` supplied current project-state/history and asset-license/provenance material. All of these are now represented in canonical `main` history.

Permanent branch governance is defined in `REPOSITORY_SOURCE_OF_TRUTH.md`: long-lived parallel development branches are prohibited; rescue refs are recovery-only and must be merged and removed.

## Current navigation state

Navigation v2 is the active game-development focus. The accepted direction is one central ship-centered `NavigationWorld` with a translating working origin, stable navigation/travel axes, shared spatial indexing, dynamic actor prediction and centralized broadphase/conflict filtering.

Authoritative system/world state remains the physical source of truth. Hub/station navigation remains its own local domain and contributes only the subset relevant to the active ship/navigation horizon.

Dynamic actors are stored as compact position/velocity/acceleration/bounds/flags data rather than dense per-voxel velocity/acceleration fields. GPU work is currently targeted at massively parallel spatial binning, prediction/swept volumes, corridor filtering and shared broadphase; sparse single-query precision graph search remains an asynchronous CPU-worker responsibility unless measurements show otherwise.

Ruckig is downstream local trajectory/kinematics generation, not obstacle pathfinding. All frame-path navigation work must remain asynchronous/non-blocking; the main thread must not synchronously wait for GPU readback or a long planner.

The raw `Shift+F12` NavigationWorld diagnostic view remains an accepted contract, but its runtime implementation is not yet present. It must consume the same completed backend-neutral NavigationWorld snapshot used by navigation/control and must not build an independent visualization world.

## NavigationMap block

Canonical code now exists on `main` under:

```text
src/world/navigation/map/
```

The block owns the ship-centered working-frame transform, dynamic actor P/V/A state, conservative prediction, sparse spatial indexing and compact corridor/sphere query products behind a backend-neutral PImpl API.

`EliteNavigationMap` is built as a standalone library for tests/benchmarks. It is intentionally not yet wired into the live `EliteGame` / `EliteServer` runtime path.

User target-machine evidence previously reported:

```text
bash tests/navigation_map/run_mingw64.sh
1/1 Test #1: navigation_map ... Passed 0.04 sec
100% tests passed, 0 tests failed out of 1
```

That test evidence was produced before the branch reconciliation, but the audited NavigationMap implementation/test tree is represented in `main` after the merge. A fresh post-merge run remains the next local verification step.

## Active gate: `NAV-V2-MAP-2`

Both isolated measurement programs are now present on `main`:

```text
benchmarks/navigation_map/
benchmarks/navigation_gpu/
```

The immediate decision gate is matched CPU/GPU measurement at 1k / 5k / 10k actors for deterministic cruise/hub datasets. Do not select CPU, GPU or hybrid production ownership before those measurements exist.

Required comparison includes publication/rebuild time, query/broadphase time, p95, candidate/conflict counts, visited cells/actors, memory, overflow/out-of-bounds diagnostics, command submission cost and GPU readback volume.

After backend selection, the next planned stage is `NAV-V2-SPACE-1`: persistent static free-space/clearance, connectivity/portals, local invalidation and agent-envelope queries. Live asynchronous integration follows only after the isolated map/space gates are accepted.

## Earlier runtime/test evidence

Before this reconciliation, the user reported successful canonical graphical launch/close plus:

```text
EliteGame.exe --self-test-fast-universe
[PASS] fast-universe real-scene smoke

EliteServer.exe --self-test
[PASS] headless-server boot + two-session authoritative routing smoke
```

The navigation guidance suite also passed all three reported tests, including `ruckig_route_planner` and `guidance_tunnel_local_horizon`.

A previously reported full `tests/run_all_mingw64.sh` ready gate was **not fully green** and had three contract failures:

1. `CROSS-TIMELINE + DIAGNOSTIC CONTRACTS` — `Hub guidance cylinder is no longer static`;
2. `SYSTEM MAP BEHAVIOR + ARCHITECTURE` — missing `Keep the first physical sample`;
3. `FEATURE SURFACE CONTRACTS` — missing debug-UI compatibility token around `m_gameUiHttpPort = m_htmlUi.start(requestedWebUiPort, webUiRoot);`.

Treat those as earlier target-machine evidence, not a statement about the freshly reconciled `main`. Do not report the full ready gate as PASS until it is rerun/classified on the canonical branch.

In the previously supplied live-client frame sample:

```text
dock_request=0 dock_plan=0 tunnel_builds=0
```

Therefore that observed frame/session had no docking request, plan or tunnel build. The older docking/guidance machinery still exists; the new NavigationWorld v2 live integration has not yet been demonstrated.

## Historical project state

The accumulated project history through 2026-09-13 is preserved separately as:

```text
PROJECT_STATE_HISTORY_THROUGH_2026-09-13.md
```

That file is historical traceability only. Old `Current ...` headings inside it are snapshots, not the active task.

## Documentation Definition of Done

A meaningful project iteration is not complete while its Markdown state is stale.

Before declaring an iteration complete or beginning the next coding slice:

1. update this file when overall project focus/state changes;
2. update affected subsystem contracts when architecture/runtime/performance contracts change;
3. synchronize `CURRENT_STATE.md`, `CURRENT_TASK.md` and the project context/iteration log as applicable;
4. keep `main` as the only canonical development branch;
5. label local-only work only when it actually exists;
6. verify dates, active task and repository baseline against the real repository state.

Stale project-state documentation or branch ambiguity is a project defect and blocks handoff.