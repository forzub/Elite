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

Dynamic-map boundary/tests passed on target machine. Accepted hybrid ownership from 100-iteration CPU/GPU measurements:

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

10k long-run evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

GPU 1k reference checks matched exactly; all scenarios had zero overflow/out-of-bounds and fixed 32-byte readback. GPU production scheduling remains asynchronous with no frame-thread wait/bulk-readback path.

## `NAV-V2-SPACE-1` static-space boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
```

Public `NavigationSpace` API owns static topology behind PImpl and exposes static replacement, transactional local patching, agent-envelope point/corridor queries, bounded invalidation and stats. The public header is independent of GLM/OpenGL/GLFW/game/render state.

First deterministic CPU reference uses free-space AABB regions + explicit portals internally. This is a behavior/reference implementation, not a permanent storage commitment.

Fresh target-machine evidence:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Accepted behavior:

- point admission is parameterized by radius + additional clearance;
- region/portal clearance rejects oversized agents;
- deterministic region/portal corridor for connected traversable topology;
- disconnected/invalidated topology fails closed;
- local invalidation affects intersecting regions plus touching portals;
- local patches are transactional and rejected patches do not partially mutate state.

## Active gate — static-space scaling benchmark

Canonical benchmark surface:

```text
benchmarks/navigation_space/
tests/architecture_contracts/check_navigation_space_benchmark.py
```

Generated deterministic datasets:

```text
open_1k / open_5k / open_10k
hub_1k  / hub_5k  / hub_10k
```

Measured independently:

```text
replaceStaticWorld
queryPoint
queryCorridor
invalidateBounds
applyLocalPatch
```

Diagnostics include regions examined, regions visited, portals examined, invalidated region/portal counts and corridor success.

Current reference costs intentionally left unoptimized for measurement:

```text
queryPoint       linear region scan
queryCorridor    BFS + full portal-map scan per visited region
invalidateBounds full region + portal scan
applyLocalPatch  transactional copy of region + portal maps
```

The next target-machine benchmark decides the first internal optimization while keeping `NavigationSpace.h` stable. Candidate optimizations include per-region adjacency, spatial point-location/invalidation index, bounded/chunked copy-on-write topology and, only if measured necessary, hierarchical regions/bricks.

## Integration order

1. run current static-space benchmark;
2. implement only the measured first internal optimization and remeasure;
3. add costed/weighted corridor traversal behind the same boundary;
4. integrate bounded asynchronous shared NavigationWorld publication;
5. add mass-NPC avoidance and precision docking/repair consumers;
6. expose the same completed NavigationWorld truth to guidance/HUD/debug presentation;
7. retire obsolete route-wide migration code only after v2 owns live navigation.

The raw `Shift+F12` NavigationWorld diagnostic-view contract remains accepted but not yet implemented.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected architecture contracts, and project-context evidence before handoff. Stale project state or branch ambiguity is a project defect.
