# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / NavigationMap runtime foundation  
**Current architecture contracts:** `src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md`, `NAVIGATION_WORLD_V2.md`  
**Canonical development branch:** `main`

## Repository state

`main` is the only canonical game-development branch.

On 2026-09-16 the primary divergent histories of old `main`, `chatgpt/mae-v01075-semantic-workflow-motion-v5`, and the user's local line published temporarily as `rescue/local-97500` were audited and reconciled into `main`. The primary reconciliation merge is:

```text
9352fe7589ec109827e9633403d81bba46bdc926
```

A subsequent inventory found older localization/editor staging and anchor refs. The remaining unique localization-completion and v0.10.75 workflow/motion histories were absorbed without replacing the accepted current tree:

```text
255930012d025a6d9f55c08a84f888e9b8ff8de8
```

Every branch tip found during that inventory is now an ancestor of `main`. Permanent branch governance is defined in `REPOSITORY_SOURCE_OF_TRUTH.md`: long-lived parallel development branches are prohibited; rescue/staging refs are recovery-only and must be merged and removed.

## Current navigation state

Navigation v2 is the active game-development focus. The accepted direction is one central ship-centered `NavigationWorld` with a translating working origin, stable navigation/travel axes, shared spatial indexing, dynamic actor prediction and centralized broadphase/conflict filtering.

Authoritative system/world state remains the physical source of truth. Hub/station navigation remains its own local domain and contributes only the subset relevant to the active ship/navigation horizon.

Dynamic actors are stored as compact position/velocity/acceleration/bounds/flags data rather than dense per-voxel velocity/acceleration fields. GPU work is targeted at massively parallel spatial binning, prediction/swept volumes, corridor filtering and shared broadphase; sparse single-query precision graph search remains an asynchronous CPU-worker responsibility unless measurements show otherwise.

Ruckig is downstream local trajectory/kinematics generation, not obstacle pathfinding. All frame-path navigation work must remain asynchronous/non-blocking; the main thread must not synchronously wait for GPU readback or a long planner.

The raw `Shift+F12` NavigationWorld diagnostic view remains an accepted contract, but its runtime implementation is not yet present. It must consume the same completed backend-neutral NavigationWorld snapshot used by navigation/control and must not build an independent visualization world.

## NavigationMap block

Canonical code exists on `main` under:

```text
src/world/navigation/map/
```

The block owns the ship-centered working-frame transform, dynamic actor P/V/A state, conservative prediction, sparse spatial indexing and compact corridor/sphere query products behind a backend-neutral PImpl API.

`EliteNavigationMap` is built as a standalone library for tests/benchmarks. It is intentionally not yet wired into the live `EliteGame` / `EliteServer` runtime path.

Fresh post-reconciliation target-machine evidence on 2026-09-16:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
100% tests passed, 0 tests failed out of 1
```

## Active gate: `NAV-V2-MAP-2`

### CPU measurement completed

Default CPU benchmark parameters:

```text
prediction horizon = 3 s
warmup = 5
iterations = 30
```

Results:

```text
scenario actors  rebuild med/p95 ms   corridor med/p95 ms   sphere med/p95 ms
cruise   1000    0.3090 / 0.3567     0.0169 / 0.0204       0.0102 / 0.0131
cruise   5000    1.3982 / 1.4902     0.0307 / 0.0499       0.0169 / 0.0307
cruise  10000    2.7388 / 2.9632     0.1352 / 0.2089       0.0329 / 0.1573
hub      1000    0.3103 / 0.3209     0.0082 / 0.0098       0.0071 / 0.0089
hub      5000    1.2864 / 2.0914     0.0231 / 0.0639       0.0262 / 0.0791
hub     10000    2.1889 / 2.9984     0.1381 / 0.2198       0.0496 / 0.1172
```

At 10k actors, query cost remains well under the current main-thread budget while whole-snapshot rebuild is the CPU cost center. This keeps CPU query ownership viable if publication/rebuild becomes asynchronous, incremental or lower cadence. The CPU benchmark CSV is:

```text
D:\__elite\work\navigation_map_cpu_benchmark.csv
```

### GPU measurement pending after harness repair

The first GPU benchmark attempt did not execute. Compilation failed with:

```text
fatal error: glad/gl.h: No such file or directory
```

Root cause: the benchmark used the obsolete include root `${ELITE_ROOT}/glad`; current GLAD 2.0.8 requires `${ELITE_ROOT}/glad/include`.

The benchmark CMake was corrected on `main`. At the same time, `tests/architecture_contracts/check_navigation_gpu_benchmark.py` was corrected from stale stage `NAV-V2-GPU-0` to the active `NAV-V2-MAP-2` gate and now explicitly validates the GLAD 2 include root.

GPU performance remains **unmeasured** until the corrected benchmark runs on the user's target machine.

The next target-machine commands are only:

```bash
git fetch origin
git switch main
git merge --ff-only origin/main
python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

Do not rerun the already-passed CPU benchmark solely because the GPU harness changed.

## Backend decision rule

Do not compare one CPU “total” directly against one GPU “total” as equivalent workloads. The CPU benchmark measures snapshot publication plus corridor/sphere queries; the GPU prototype also performs all-agent neighbor/conflict reduction.

The backend decision remains evidence-driven:

- CPU remains viable for spatial query ownership from the measured query timings;
- GPU is a candidate for P/V/A prediction, binning and all-agent conflict reduction;
- hybrid remains valid: CPU static topology/precision search + GPU dynamic reduction;
- no GPU backend may synchronously dispatch, wait and bulk-read back on the frame thread.

Performance design targets remain:

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
full/precision route solve       asynchronous; never a frame-thread blocker
```

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

Treat those as earlier target-machine evidence, not a statement about current `main`. Do not report the full ready gate as PASS until it is rerun/classified on the canonical branch.

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
5. remove temporary branch refs after their commits are ancestors of `main`;
6. label local-only work only when it actually exists;
7. verify dates, active task and repository baseline against the real repository state.

Stale project-state documentation or branch ambiguity is a project defect and blocks handoff.