# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — hybrid backend selected; static free-space/clearance/connectivity is the active implementation stage

## Repository source of truth

`main` is the only canonical game-development branch. Read and obey `REPOSITORY_SOURCE_OF_TRUTH.md`. Long-lived parallel feature/rescue/staging branches are prohibited.

## Navigation v2 architecture

The legacy route-wide synchronous chain remains migration code, not the new foundation:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route sampling / route-wide validation
    -> GuidanceTunnel
```

The retained low-level kinematic primitive is `RuckigTrajectorySolver`, used only after navigation has selected a temporary safe target state.

Canonical contracts:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
NAVIGATION_WORLD_V2.md
```

The accepted runtime direction is one shared ship-centered `NavigationWorld` with a translating origin, stable navigation/travel axes, shared spatial reduction and separate mass-NPC versus precision consumers.

## NavigationMap boundary — accepted

Canonical block:

```text
src/world/navigation/map/
```

Public ingress:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

Public egress:

```text
queryCorridor()
querySphere()
stats()
```

The API stays backend-neutral. Actor tables, spatial cells, GPU buffers and other implementation resources remain private behind PImpl.

Fresh target-machine gates on 2026-09-16:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

## `NAV-V2-MAP-2` — CLOSED

The isolated CPU/GPU measurement gate is accepted. Both default and 100-iteration target-machine measurements exist.

### CPU — 100-iteration accepted measurement

Parameters:

```text
horizon_s=3
warmup=10
iterations=100
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

10k diagnostics:

```text
cruise: corridor candidates=63, examined=239; sphere candidates=43, examined=204; occupied_cells=8310
hub:    corridor candidates=150, examined=815; sphere candidates=192, examined=1382; occupied_cells=1719
out_of_bounds=0 and rejected=0 for every scenario
```

Interpretation: CPU compact corridor/sphere queries remain comfortably inside the current main-thread budget. Whole-snapshot rebuild is the CPU cost center and must not become a synchronous per-frame 10k path; publication should be asynchronous, incremental and/or lower cadence.

### GPU — 100-iteration accepted Quadro RTX 3000 measurement

The accepted run used:

```text
OpenGL=4.3
vendor="NVIDIA Corporation"
renderer="Quadro RTX 3000/PCIe/SSE2"
horizon_s=3
warmup=10
iterations=100
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

All scenarios: `overflow=0`, `out_of_bounds=0`, `valid=1`, fixed `readback_bytes=32`; 10k memory ≈ `16.6321 MiB`. Both 1k scenarios exactly matched the independent CPU pair/corridor reference (`reference_ok=1`).

The GPU prediction/binning pass is approximately `0.01–0.02 ms`; the all-agent neighbor/conflict pass dominates cost. Dense Hub traffic is the main future optimization target.

The shader portability defect (`flat` reserved GLSL identifier) is fixed on `main` by commit:

```text
48c8dbfaefc1a2bda82af762f46a5b38400033ac
```

Raw GPU run history: `benchmarks/navigation_gpu/RUN_LOG.md`.

## Backend decision — HYBRID

The benchmark workloads are complementary rather than identical, so no naive CPU-total versus GPU-total winner is used.

Accepted production ownership direction:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search (docking/repair/special)
    backend-neutral compact query products

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    mass all-agent neighbor/conflict reduction
```

GPU publication/consumption must be asynchronous/double- or triple-buffered. No frame-thread `dispatch -> wait -> bulk readback` path is permitted. Result age is part of the safety horizon.

## Active stage — `NAV-V2-SPACE-1`

The next implementation target is persistent static navigation space:

```text
static obstacle/free-space representation
agent-envelope clearance
connectivity / portals
local invalidation / rebuild
cached sparse global corridor
outside <-> inside / narrow passage support
```

Representation is not chosen merely because one data structure is familiar. The first slice must define the backend-neutral static-space API/contract and deterministic CPU reference behavior, then measure/query it before live integration.

Required properties:

- sparse/hierarchical rather than one huge dense field;
- explicit connectivity or portal traversal;
- clearance queries parameterized by agent envelope;
- bounded local invalidation when geometry/damage changes;
- usable across open space, station exteriors, interiors and breaches;
- compatible with the existing ship-centered working frame;
- usable by both mass routing and precision planners without duplicating world state.

## Later integration order

1. implement and test `NAV-V2-SPACE-1` in isolation;
2. integrate bounded asynchronous shared NavigationWorld publication;
3. add mass-NPC local avoidance consumer;
4. add precision docking/repair planner;
5. feed temporary target states into `RuckigTrajectorySolver`;
6. expose the same accepted navigation truth to guidance/HUD/debug presentation;
7. retire obsolete route-wide navigation only after v2 owns the live path.

Performance targets remain:

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
precision/global route solve     asynchronous only
```

The raw `Shift+F12` NavigationWorld debug-view contract remains accepted but not yet implemented.
