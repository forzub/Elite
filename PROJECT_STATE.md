# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / static-space foundation  
**Current architecture contracts:** `src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md`, `NAVIGATION_WORLD_V2.md`  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Repository state

`main` is the only canonical game-development branch. Branch governance is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`.

The divergent historical lines were reconciled into `main` on 2026-09-16. Primary history merge:

```text
9352fe7589ec109827e9633403d81bba46bdc926
```

Historical side-branch absorption:

```text
255930012d025a6d9f55c08a84f888e9b8ff8de8
```

Do not resume feature work on old `chatgpt/*`, rescue, staging or anchor refs.

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld` rather than a complete per-NPC world/planner.

Authoritative system/world state remains the physical source of truth. The active NavigationWorld uses a translating local origin with stable navigation/travel axes. Hub/station/carrier/interior spaces remain local domains and publish only relevant transformed subsets.

Ruckig remains downstream local kinematics. It is not free-space/path-search authority.

The old route-wide chain remains migration code only:

```text
GeometricPathPlanner
 -> route-wide TrajectoryGenerator / RuckigRoutePlanner
 -> dense sampling / route-wide validation
 -> GuidanceTunnel
```

## NavigationMap block — accepted

Canonical code:

```text
src/world/navigation/map/
```

The API is backend-neutral and owns ship-centered dynamic actor publication, prediction, spatial indexing and compact corridor/sphere queries behind PImpl.

Target-machine gates:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

## `NAV-V2-MAP-2` — accepted measurement gate

### CPU 100-iteration measurement

```text
horizon_s=3 warmup=10 iterations=100
```

```text
scenario actors  rebuild med/p95 ms   corridor med/p95 ms   sphere med/p95 ms
cruise   1000    0.3048 / 0.3226     0.0170 / 0.0199       0.0100 / 0.0120
cruise   5000    1.4725 / 1.6711     0.0309 / 0.0412       0.0168 / 0.0212
cruise  10000    2.7892 / 3.0958     0.0552 / 0.1713       0.0346 / 0.1329
hub      1000    0.2995 / 0.3459     0.0064 / 0.0084       0.0080 / 0.0098
hub      5000    1.2608 / 1.4092     0.0226 / 0.0349       0.0234 / 0.0414
hub     10000    2.1775 / 2.4924     0.0328 / 0.1551       0.0324 / 0.0471
```

All CPU scenarios reported `out_of_bounds=0`, `rejected=0`.

Interpretation: compact CPU queries are inexpensive; whole-snapshot rebuild is the cost center and should be asynchronous/incremental/lower cadence rather than a synchronous 10k-per-frame rebuild.

### GPU 100-iteration measurement

Accepted adapter:

```text
NVIDIA Corporation
Quadro RTX 3000/PCIe/SSE2
OpenGL 4.3
```

```text
scenario actors  bin ms   neighbor ms  total med ms  p95 ms   submit ms  neighbor checks
cruise   1000    0.0163   0.5482       0.5646        0.5655   0.0010       8,412
cruise   5000    0.0171   0.9298       0.9466        0.9500   0.0014     213,211
cruise  10000    0.0106   0.6737       0.6840        1.3226   0.0013     855,225
hub      1000    0.0087   0.3350       0.3441        0.3499   0.0005      52,865
hub      5000    0.0091   0.8663       0.8754        0.8806   0.0014   1,353,890
hub     10000    0.0105   1.5991       1.6097        1.6258   0.0014   5,527,327
```

All GPU scenarios: `overflow=0`, `out_of_bounds=0`, `valid=1`, fixed 32-byte readback. Both 1k scenarios matched the independent CPU correctness reference exactly. 10k memory ≈16.63 MiB.

GPU prediction/binning is approximately 0.01–0.02 ms; neighbor/conflict reduction dominates. Dense Hub traffic is the future optimization target.

Shader portability repair is canonical on `main`:

```text
48c8dbfaefc1a2bda82af762f46a5b38400033ac
```

Raw GPU evidence: `benchmarks/navigation_gpu/RUN_LOG.md`.

## Backend selection — HYBRID

This is not based on comparing incompatible CPU/GPU “total” workloads. It is based on measured complementary strengths.

Accepted ownership:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse global corridor search
    precision local planning

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent conflict reduction
```

The public NavigationMap/NavigationWorld boundaries remain backend-neutral. GPU production scheduling is asynchronous/double- or triple-buffered with bounded result products; no synchronous frame-thread wait/bulk-readback path is allowed.

## Active stage — `NAV-V2-SPACE-1`

The next block is persistent static navigation space.

Required capability:

```text
sparse static obstacle/free-space representation
clearance parameterized by agent envelope
explicit connectivity / portals
local invalidation after geometry/damage changes
coarse region/portal corridor queries
outside <-> inside / narrow-passage routing
```

The first implementation slice is isolated and CPU-reference-driven. It must expose a backend-neutral API and standalone tests before any live `EliteGame` / `EliteServer` integration.

Do not start by building a huge dense system voxel volume. Sparse/hierarchical and region/portal hybrid representations remain valid candidates; the code/tests should preserve the abstraction until measured evidence justifies a lower-level representation.

## Later integration

After `NAV-V2-SPACE-1` acceptance:

1. bounded asynchronous shared NavigationWorld publication;
2. mass-NPC local avoidance consumer;
3. precision docking/repair planner;
4. local `RuckigTrajectorySolver` execution;
5. guidance/HUD/debug rendering of the same accepted navigation truth;
6. retirement of obsolete route-wide migration code after v2 owns live navigation.

The raw `Shift+F12` NavigationWorld diagnostic-view contract remains accepted but not yet implemented.

## Earlier runtime/test evidence

Historical graphical/server smokes and older guidance tests remain recorded in `PROJECT_STATE_HISTORY_THROUGH_2026-09-13.md` and prior iteration logs. A previously reported full ready gate was not fully green; do not report the whole project test suite as PASS without a fresh canonical run.

## Documentation Definition of Done

Every meaningful NavigationWorld iteration must synchronize:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
project-context CURRENT_STATE/CURRENT_TASK/DECISIONS/ITERATION_LOG/SOURCES as applicable
```

Stale project state or branch ambiguity is a project defect and blocks handoff.
