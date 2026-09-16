# Game Runtime Decomposition

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Status:** R0 runtime seams + dual-source model ingress + OpenGL 4.3 Core accepted; Navigation work reset to `NAV-V2-GPU-0`

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

`NAV-V2-GPU-0` is therefore an isolated benchmark target under
`benchmarks/navigation_gpu`, not a source entry in `EliteGame`.

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
        v
ship-centered NavigationWorld working domain
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

Ordinary player flight uses a ship-centered NavigationWorld working frame. The
origin may translate/rebase with the active ship/domain. The navigation axes are
stable system/travel axes rather than instantaneous hull attitude, so rolling the
ship does not force spatial-index rebuilds.

### Hub-local domain

Hub geometry, ports and scheduled local bots remain in Hub-local coordinates.
When interaction becomes relevant, the Hub publishes/transforms only the subset
needed by the active ship NavigationWorld. The Hub's entire private map is not
rebased around the player each frame.

### Hull-local / presentation domains

Hull-local coordinates belong to execution/control. Player-relative/render
coordinates belong to presentation. Neither is an authority for cached
free-space topology.

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

## GPU prototype boundary

`NAV-V2-GPU-0` asks only whether the dynamic shared layer is cheap enough on the
target GPU.

The compute prototype measures:

```text
actor P/V/A
    -> conservative future swept sphere
    -> fixed 3D bins
    -> selected-corridor filter
    -> all-agent candidate/conflict query
```

It runs deterministic `cruise` and `hub` scenes at 1k / 5k / 10k actors.
It reports GPU median/p95, CPU submission, candidate counts, memory, overflow and
readback. A 1k O(N^2) CPU reference verifies the GPU aggregate result.

The prototype reads back only a 32-byte aggregate stats block. Production GPU
navigation must be asynchronous/double-buffered and should keep detailed conflict
products GPU-resident or consume them through bounded asynchronous transfer.

No live integration occurs before benchmark evidence is reviewed.

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
Rendering and navigation compete for GPU time, so a CPU BVH/spatial-hash baseline
must be compared if GPU cost is not clearly advantageous.

Different layers may run at different frequencies. Physics/control can update at
60-120 Hz while global route validity is rare/event-driven. There is no rule that
rebuilds a global route every second.

## Current planned order

1. R0 shared runtime seams — accepted.
2. Dual-source runtime model ingress — accepted.
3. OpenGL 4.3 Core + GPU renderer baseline — accepted.
4. Ruckig isolated/local motion experiment — retained as evidence/component.
5. Reject old route-wide planner as Navigation v2 foundation.
6. `NAV-V2-GPU-0`: measure shared dynamic-world GPU prediction/broadphase at
   1k/5k/10k actors.
7. Compare CPU spatial-index baseline if GPU numbers are not decisively useful.
8. `NAV-V2-SPACE-1`: choose/prototype persistent static free-space + clearance
   representation for ship/Hub interaction domains.
9. `NAV-V2-DYN-1`: integrate shared dynamic NavigationWorld with bounded async
   publication.
10. Add mass-NPC avoidance and precision docking/repair planners as separate
    consumers of the shared world.
11. Remove obsolete legacy navigation components once v2 owns the live path.
12. Resume paused renderer/runtime-model decomposition work.

## Current testing commands

Architecture gate:

```bash
python tests/architecture_contracts/check_navigation_gpu_benchmark.py
```

GPU benchmark:

```bash
bash benchmarks/navigation_gpu/run_mingw64.sh
```

The legacy Ruckig/navigation tests remain useful regression evidence while old
code still exists, but passing them is no longer acceptance of the overall
Navigation v2 architecture.
