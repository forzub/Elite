# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / static-space graph indexing  
**Current architecture contracts:** `src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md`, `NAVIGATION_WORLD_V2.md`  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Repository state

`main` is the only canonical game-development branch. Branch governance is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. Historical divergent lines were reconciled/absorbed on 2026-09-16; do not resume feature work on old parallel refs.

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior spaces remain local domains and publish only relevant subsets.

The old route-wide chain is migration code only. `RuckigTrajectorySolver` remains downstream local kinematics, not path-search authority.

## `NAV-V2-MAP-2` — CLOSED

Accepted hybrid ownership:

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

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block: `src/world/navigation/space/`.

Public `NavigationSpace` API owns static topology behind PImpl and exposes static replacement, transactional local patching, agent-envelope point/corridor queries, bounded invalidation and stats. Public API is independent of GLM/OpenGL/GLFW/game/render state.

Fresh target-machine behavior:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

## Static-space baseline — accepted evidence

Original 10k corridor:

```text
open median/p95 = 1991.0357 / 2008.2074 ms
hub  median/p95 = 1951.6453 / 1956.0686 ms
portal examinations = 285,913,245
```

Root cause was a complete portal-map scan per BFS region.

## Optimization 1 — per-region adjacency — ACCEPTED

Private index:

```text
regionId -> ordered portalId list
```

Target-machine rerun:

```text
open_10k: corridor 21.7848 / 22.1695 ms med/p95; portal examinations=57,189
hub_10k:  corridor 22.3689 / 22.7493 ms med/p95; portal examinations=57,189
```

Other 10k costs after adjacency:

```text
open: point p95=0.4356 ms, invalidate median=3.2622 ms, patch median=16.1963 ms, replace median=13.3648 ms
hub:  point p95=0.4536 ms, invalidate median=3.3208 ms, patch median=15.8516 ms, replace median=13.7190 ms
```

Adjacency reduced corridor median by roughly `87–91x` and portal examinations by roughly `5000x`. Publication/patch got more expensive because they now build connectivity; those are not intended frame-path operations.

Raw evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 2 candidate — dense RegionSlot graph

Implemented privately without changing `NavigationSpace.h`:

```text
RegionId -> dense RegionSlot
RegionSlot -> RegionId
RegionSlot -> ordered adjacency edges { PortalId, neighbor RegionSlot }
```

BFS visited/predecessor/frontier state is vector-backed by dense slots instead of `std::map<RegionId,...>`. Deterministic public RegionId/PortalId output is preserved.

Architecture contract rejects both full portal-map scan and ordered-map visited/previous bookkeeping in corridor BFS.

Point lookup, invalidation, transactional map copy and graph rebuild are deliberately unchanged for this measurement slice.

## Active target-machine gate

```bash
python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Do not add another optimization until the dense-slot result is measured.

## Later integration order

1. remeasure dense-slot graph;
2. choose the next internal bottleneck from evidence;
3. add costed/weighted corridor traversal behind the same API;
4. integrate bounded asynchronous shared NavigationWorld publication;
5. add mass-NPC avoidance and precision docking/repair consumers;
6. expose the same completed NavigationWorld truth to guidance/HUD/debug presentation;
7. retire obsolete route-wide migration code only after v2 owns live navigation.

The raw `Shift+F12` NavigationWorld diagnostic-view contract remains accepted but not yet implemented.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected contracts and project-context evidence before handoff. Stale project state or branch ambiguity is a project defect.
