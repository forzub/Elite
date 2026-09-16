# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / static-space scaling and indexing  
**Current architecture contracts:** `src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md`, `NAVIGATION_WORLD_V2.md`  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Repository state

`main` is the only canonical game-development branch. Branch governance is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. Historical divergent lines were reconciled/absorbed on 2026-09-16; do not resume feature work on old `chatgpt/*`, rescue, staging or anchor refs.

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior spaces remain local domains and publish only relevant subsets.

The old route-wide chain is migration code only. `RuckigTrajectorySolver` remains downstream local kinematics, not path-search authority.

## `NAV-V2-MAP-2` — CLOSED

Accepted hybrid ownership from target-machine CPU/GPU measurements:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local planning

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

Accepted 10k long-run evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

GPU 1k correctness matched exactly; all scenarios had zero overflow/out-of-bounds and fixed 32-byte readback. GPU production scheduling remains asynchronous with no frame-thread wait/bulk-readback path.

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
```

Public `NavigationSpace` API owns static topology behind PImpl and exposes static replacement, transactional local patching, agent-envelope point/corridor queries, bounded invalidation and stats. Public API is independent of GLM/OpenGL/GLFW/game/render state.

First deterministic CPU reference uses free-space AABB regions + explicit portals internally. This is a behavior/reference implementation, not a permanent storage commitment.

Fresh target-machine evidence:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

## Static-space baseline benchmark — accepted evidence

Benchmark contract passed on the user's MinGW64 machine. Default run: `warmup=1`, `iterations=3`.

10k results:

```text
open_10k
    replace med/p95    6.6942 / 7.2406 ms
    point med/p95      0.2787 / 0.2812 ms
    corridor med/p95   1991.0357 / 2008.2074 ms
    invalidate med/p95 2.5731 / 2.9869 ms
    patch med/p95      7.6135 / 7.8543 ms
    portals examined   285,913,245

hub_10k
    replace med/p95    7.2397 / 7.5269 ms
    point med/p95      0.2784 / 0.2845 ms
    corridor med/p95   1951.6453 / 1956.0686 ms
    invalidate med/p95 2.8412 / 3.2230 ms
    patch med/p95      8.6678 / 8.7083 ms
    portals examined   285,913,245
```

The baseline shows one overwhelming defect: corridor BFS scanned the complete portal map for every visited region. From 1k to 10k, corridor median grew about 102-105x while portal examinations grew about 106-108x. Point lookup remains ~0.28 ms at 10k, so it is not the first optimization target.

Raw baseline: `benchmarks/navigation_space/RUN_LOG.md`.

## First measured optimization — per-region adjacency

Implemented privately without changing `NavigationSpace.h`:

```text
regionId -> ordered portalId list
```

`replaceStaticWorld()` and `applyLocalPatch()` rebuild adjacency transactionally. PortalId order remains stable for deterministic BFS tie-breaking. `queryCorridor()` now examines only portals adjacent to the current region. The architecture contract rejects regression to a complete portal-map scan inside BFS.

Point lookup, invalidation and full transactional patch copying intentionally remain unchanged until the adjacency rerun identifies the next dominant cost.

## Active gate

Rerun after the implementation change:

```bash
python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Do not add another optimization before this evidence exists.

## Later integration order

1. remeasure adjacency-indexed reference;
2. implement only the next measured internal optimization;
3. add costed/weighted corridor traversal behind the same boundary;
4. integrate bounded asynchronous shared NavigationWorld publication;
5. add mass-NPC avoidance and precision docking/repair consumers;
6. expose the same completed NavigationWorld truth to guidance/HUD/debug presentation;
7. retire obsolete route-wide migration code only after v2 owns live navigation.

The raw `Shift+F12` NavigationWorld diagnostic-view contract remains accepted but not yet implemented.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected contracts and project-context evidence before handoff. Stale project state or branch ambiguity is a project defect.
