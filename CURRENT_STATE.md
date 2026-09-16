# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static-space boundary/reference accepted; scaling benchmark is active

## Repository source of truth

`main` is the only canonical game-development branch. `REPOSITORY_SOURCE_OF_TRUTH.md` governs branch/recovery policy.

## Navigation v2 accepted architecture

The legacy route-wide synchronous chain remains migration code. `RuckigTrajectorySolver` is retained only as a downstream local kinematic primitive after navigation selects a safe temporary target.

Navigation v2 uses one shared ship-centered `NavigationWorld` with stable navigation axes and separate static/dynamic ownership.

Accepted hybrid split from `NAV-V2-MAP-2` measurements:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

GPU work remains asynchronous/double- or triple-buffered; no frame-thread `dispatch -> wait -> bulk readback` path is allowed.

## `NAV-V2-MAP-2` — CLOSED

Fresh target-machine dynamic-map gates passed:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

Accepted 100-iteration 10k results:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

GPU 1k correctness matched the independent CPU pair/corridor reference. All measured GPU scenarios had `overflow=0`, `out_of_bounds=0`, `valid=1`, fixed 32-byte readback.

## `NAV-V2-SPACE-1` static-space boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
    NavigationSpace.h
    NavigationSpace.cpp
    CMakeLists.txt
    README.md
```

The public backend-neutral API owns persistent static navigation topology and exposes:

```text
AgentEnvelope
RegionInput
PortalInput
StaticSpaceUpdate
LocalPatch
queryPoint
queryCorridor
invalidateBounds
stats
```

The first deterministic CPU reference uses free-space AABB regions + explicit portals internally. Storage/search representation remains replaceable behind PImpl; public API has no GLM/OpenGL/GLFW/game/render dependency.

Fresh target-machine evidence on canonical `main`:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
 - one backend-neutral API owns persistent static navigation topology
 - public header is isolated from GLM/OpenGL/game/render state
 - CPU reference uses deterministic free-space regions + portals
 - agent-envelope clearance and narrow-portal admission are explicit
 - local invalidation + transactional patching are owned by the block
 - project state/task agree on NAV-V2-SPACE-1

navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Accepted reference behavior:

- point admission uses agent radius + additional clearance;
- region bounds and authored clearance cap both constrain admission;
- portal clearance rejects oversized agents;
- corridor search returns deterministic region/portal paths;
- disconnected and invalidated topology fail closed;
- invalidation is bounded to intersecting regions plus touching portals;
- local patches are transactional; rejected patches do not partially mutate state.

## Active measurement gate — static-space scaling

New benchmark surface:

```text
benchmarks/navigation_space/
    main.cpp
    CMakeLists.txt
    run_mingw64.sh
    README.md

tests/architecture_contracts/check_navigation_space_benchmark.py
```

Deterministic `open_*` and `hub_*` region/portal lattices are generated at approximately 1k / 5k / 10k regions. The benchmark measures full publication, point lookup, corridor search, bounded invalidation and one-region transactional patch separately.

Current reference limitations deliberately left visible for measurement:

```text
point lookup       linear region scan
corridor search    BFS + complete portal-map scan per visited region
invalidation       complete region + portal scan
local patch        transactional copy of region + portal maps
```

No optimization is accepted yet. Target-machine benchmark evidence will determine whether the first internal improvement is adjacency indexing, spatial point-location/invalidation indexing, bounded copy-on-write topology, or a hierarchy.

## Immediate next step

Run:

```bash
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Do not rerun the already-passed NavigationSpace boundary/behavior gate solely for benchmark-only files. Do not integrate static space into live runtime until this scaling/reference gate has been measured and the necessary internal indexing pass has been selected.

## Later order

1. measure current static-space reference;
2. implement only the measured first internal optimization and remeasure;
3. add costed corridor traversal while keeping the public boundary stable;
4. integrate bounded asynchronous shared NavigationWorld publication;
5. add mass-NPC and precision docking/repair consumers;
6. expose the same completed NavigationWorld truth to guidance/HUD/debug presentation;
7. retire obsolete route-wide navigation only after v2 owns the live path.
