# Game Runtime Decomposition

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress + OpenGL 4.3 Core accepted; Navigation at `NAV-V2-MAP-2` CPU/GPU measurement gate

## Purpose

Keep deterministic gameplay/runtime boundaries explicit while renderer,
asset-ingress and NavigationWorld infrastructure evolve independently.
Presentation never owns simulation, collision, damage or navigation authority.

## Accepted runtime seams

`EliteAssemblyGeometry` owns shared CPU assembly geometry.

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.

The existing `EliteNavigationGeometry` target contains legacy navigation geometry
utilities. Its existence does not make the old route-wide planner the v2 runtime
architecture.

## Renderer boundary

The graphical client targets OpenGL 4.3 Core. The Core cutover, GPU-P0 System Map
spheres and GPU-P0.1 instanced circles/orbits are accepted locally.

Navigation v2 may use OpenGL compute for a shared dynamic spatial layer, but GPU
navigation must remain a separate workload from presentation. No renderer pass
owns navigation state merely because both use the same GPU/context.

The GPU work under `benchmarks/navigation_gpu` remains an isolated backend
experiment, not a source entry in `EliteGame`. Production integration is blocked
until the CPU/GPU comparison is accepted.

## Navigation v2 boundary

Authoritative design:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

The previous live chain:

```text
GeometricPathPlanner
    -> route-wide Ruckig/trajectory materialization
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

is retained only as legacy/migration code while Navigation v2 is developed. It
is not the ownership template for new work.

The v2 runtime boundary is instead:

```text
Authoritative system/world simulation state
        |
        | publish/transform relevant subset
        v
ship-centered NavigationWorld working domain
    NavigationMap
    static free-space / clearance representation
    dynamic actor P/V/A/bounds table
    spatial index / prediction
    route-corridor filtering
    conflict candidates
        |
        +-> mass NPC local steering
        +-> precision local planner
        +-> temporary target state
        |
        v
local kinematic motion / flight control
```

Ruckig can remain a local state-to-state motion primitive at the bottom of this
chain. It does not own static navigation-space construction, global topology,
dynamic actor culling or obstacle avoidance.

## Coordinate/runtime-domain decomposition

Do not conflate storage coordinates with working navigation coordinates.

### System/world authority

Long-lived precise simulation/replication state remains system/world based.
Celestial and persistent-object motion is not rewritten around the player.

### Active ship-centered domain

Ordinary/deep-space player flight uses a ship-centered NavigationWorld working
frame. The origin may translate/rebase with the active ship/domain. The
navigation axes are stable system/travel axes rather than instantaneous hull
attitude, so rolling/pitching/yawing the ship does not force spatial-index
rebuilds.

This is the primary active navigation working space when the ship is away from a
station. It is not derived from Hub-local coordinates.

### Hub-local domain

Hub geometry, ports and scheduled local bots remain in Hub-local coordinates.
The Hub is a private local simulation/navigation domain for its own objects and
traffic. When interaction becomes relevant, the Hub publishes/transforms only the
subset needed by the active ship NavigationWorld. The Hub's entire private map is
not rebased around the player each frame.

The same publication boundary applies to other persistent local domains such as
carriers, capital ships, settlements or interiors.

### Hull-local / presentation domains

Hull-local coordinates belong to execution/control. Player-relative/render
coordinates belong to presentation. Neither is an authority for cached
free-space topology.

## NavigationMap runtime ownership

The first v2 production-shaped boundary now exists under:

```text
src/world/navigation/map/
```

`NavigationMap` owns working-frame conversion, the dynamic actor snapshot,
prediction and the spatial index behind a narrow PImpl API. Callers publish an
owned `DynamicWorldUpdate` and receive compact corridor/sphere query products by
value.

No internal actor table, cell structure, prediction cache or future GPU resource
crosses the module boundary. This is required so CPU, GPU or hybrid backends can
be exchanged without changing gameplay/planner call sites.

The CPU reference contract has passed on the user's MinGW64 machine:

```text
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

## NavigationWorld and NPC ownership

Player and NPC navigation should consume one shared active NavigationWorld rather
than each agent performing its own full-scene scan.

Dynamic actors expose compact state:

```text
P / V / A
bounds
optional angular/motion metadata
flags / revisions
```

The shared spatial layer determines local neighbors and corridor-relevant actors.
More expensive prediction/planning occurs only for that reduced set.

This enables navigation LOD:

- ordinary/background traffic: cached corridor + cheap local avoidance;
- precision docking/repair/special actors: more expensive local space-time solve;
- all agents: shared broadphase/spatial data instead of N independent world scans.

Dynamic state remains actor-owned. The runtime does not duplicate velocity and
acceleration into every navigation cell. Spatial cells/bins are sparse indexing
structures.

## CPU/GPU backend boundary

`NAV-V2-MAP-2` compares the isolated CPU reference against the existing OpenGL
4.3 compute prototype before any live-game backend choice.

CPU benchmark:

```text
benchmarks/navigation_map/
```

GPU benchmark:

```text
benchmarks/navigation_gpu/
```

Both use deterministic `cruise` / `hub` classes at 1k / 5k / 10k actors.

The first GPU candidates are the massively parallel reduction stages:

```text
actor P/V/A
    -> conservative future swept bounds
    -> spatial bins/hash
    -> selected-corridor/local-horizon filter
    -> all-agent neighbor/conflict candidate generation
```

The GPU is not automatically the owner of single-agent A*/Theta*/SIPP search.
Those algorithms remain CPU-worker candidates until batched measurements justify
a different split. A hybrid design is valid: CPU topology/precision graph search
with GPU prediction/binning/conflict reduction.

### No synchronous GPU stall

Production GPU navigation must be asynchronous and double/triple buffered. A
compute dispatch followed by a blocking fence/readback on the frame thread is not
an acceptable replacement for the old CPU stall.

```text
frame N:   submit NavigationWorld update N
frame N+1: consume last completed bounded result; submit N+1
```

Detailed products should stay GPU-resident where useful or cross through bounded
asynchronous compact transfers. Compute/data age is included in the physical
lookahead horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

## Collision and damage decomposition

Navigation, collision and damage remain separate authorities:

```text
NavigationWorld
    conservative navigation envelope / free-space / predicted conflicts

Physics / Collision
    shared broadphase candidates where useful
    exact CCD / TOI / contact geometry

Damage / Structural
    hit ownership / material response
    detach / breach / destruction
    local navigation invalidation
```

Render, collision, hit/damage and navigation geometry are separate products.

A small visual/projectile hole does not automatically alter navigation. A breach
changes navigation only when it creates agent-sized clearance/connectivity.
Topology changes dirty only the affected static navigation region. Detached
structural fragments register as new dynamic actors and enter shared broadphase.

## Performance/scheduling contract

Navigation is not allowed to own the frame budget.

```text
main-thread navigation CPU      < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld     < 1.0 ms preferred, < 2.0 ms heavy target
precision/global route solve    asynchronous; never a frame-thread blocker
```

These values guide design; GPU thresholds are not portable hard test assertions.
Rendering and navigation compete for GPU time, so CPU, GPU and hybrid backends
must be compared on the same target machine/scenarios.

Different layers may run at different frequencies. Physics/control can update at
60-120 Hz while global route validity is rare/event-driven. There is no rule that
rebuilds a global route every second.

## Current planned order

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. OpenGL 4.3 Core + GPU renderer baseline — accepted.
4. Ruckig isolated/local motion experiment — retained as evidence/component.
5. Reject old route-wide planner as Navigation v2 foundation — accepted.
6. `NAV-V2-MAP-1`: ship-centered NavigationMap boundary + deterministic CPU
   reference — accepted; target MinGW64 contract PASS.
7. `NAV-V2-MAP-2`: measure CPU reference and isolated GPU compute prototype on
   matched 1k/5k/10k `cruise`/`hub` datasets — **active**.
8. Choose CPU/GPU/hybrid dynamic backend behind the same NavigationMap API.
9. `NAV-V2-SPACE-1`: choose/prototype persistent static free-space + clearance,
   connectivity/portals and local invalidation for ship/Hub interaction domains.
10. `NAV-V2-DYN-1`: integrate shared dynamic NavigationWorld with bounded async
    publication and no frame-thread waits.
11. Add mass-NPC avoidance and precision docking/repair planners as separate
    consumers of the shared world.
12. Remove obsolete legacy navigation components once v2 owns the live path.
13. Resume paused renderer/runtime-model decomposition work.

## Current testing commands

CPU boundary/behavior:

```bash
python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
```

CPU benchmark:

```bash
bash benchmarks/navigation_map/run_mingw64.sh
```

GPU architecture/benchmark:

```bash
python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

The legacy Ruckig/navigation tests remain useful regression evidence while old
code still exists, but passing them is no longer acceptance of the overall
Navigation v2 architecture.
