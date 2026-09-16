# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted locally  
**Navigation:** `NAV-V2-MAP-2` — ship-centered NavigationMap CPU reference accepted; CPU/GPU measurement is the active gate

## Stable baseline outside navigation

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains shared runtime/editor schema authority.
Renderer feature work remains paused while Navigation v2 is established.

## Navigation architecture reset

The legacy route-wide synchronous chain is migration code, not the new foundation:

```text
GeometricPathPlanner
    -> route-wide trajectory materialization
    -> dense obstacle validation
    -> GuidanceTunnel
```

Ruckig remains only a possible local kinematic primitive after routing/avoidance
chooses a target state. It is not free-space/path-search authority.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Accepted coordinate/runtime domains

The whole-game review fixes the domain ownership as follows.

### Authoritative system/world state

Long-lived simulation, replication, celestial and persistent-object state remains
in precise system/world coordinates. This is storage/simulation authority.

### Active ship-centered NavigationWorld

Ordinary flight/navigation work is performed in a ship-centered working domain:

- the origin may translate/rebase with the active ship/domain;
- navigation axes remain stable system/travel axes;
- working axes do **not** follow hull roll/pitch/yaw;
- nearby static navigation geometry, dynamic actors, predicted swept bounds and
  active route corridors are represented in this working domain;
- hull-local coordinates remain flight/control detail;
- render/player-relative coordinates remain presentation-only.

The ship-centered frame is therefore a transient numerical/spatial working frame,
not a replacement for authoritative system coordinates.

### Hub-local space

Hub owns its own local geometry, docking ports and scheduled local bots. That
private station domain is not the universal gameplay coordinate system and is not
rebased around the player every frame.

When a Hub can affect the active ship, only the relevant Hub subset is
transformed/published into the active ship NavigationWorld. The same ownership
pattern applies to other persistent local domains such as carriers, capital
ships, settlements and interiors.

## Shared NavigationWorld ownership

The v2 target is one shared data-oriented NavigationWorld for the active domain,
not one full-world planner per actor.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
ship-centered NavigationWorld
    + static free-space / clearance
    + dynamic actor table { P, V, A, bounds, flags, revisions }
    + spatial index
    + prediction / swept bounds
    + active corridors / local horizons
            |
            v
compact relevant/conflict candidates
      |                         |
      v                         v
mass NPC steering       precision docking/repair planner
      |                         |
      +------------+------------+
                   v
            temporary target
                   |
             local motion
                   |
             flight control
```

This permits navigation LOD while keeping a common spatial/prediction authority:
background traffic uses cheap local steering; precision actors use more expensive
space-time/local planning only after the shared world has reduced the candidate
set.

## NavigationMap ownership boundary

Implemented block:

```text
src/world/navigation/map/
    NavigationMap.h
    NavigationMap.cpp
    CMakeLists.txt
    README.md
```

Public ingress is an owned `DynamicWorldUpdate` by value containing the working
frame, source revision and actor P/V/A/radius/flags/revision data.

The block transforms and owns all resulting state internally. Actor tables,
prediction caches, sparse spatial cells and future CPU/GPU backend resources do
not cross the boundary. `NavigationMap.h` deliberately has no dependency on GLM,
OpenGL, GLFW, scene, game state or renderer code and uses PImpl.

Public egress is only compact derived data by value:

```text
queryCorridor()
querySphere()
stats()
```

The CPU reference backend currently implements constant-acceleration endpoint
prediction, conservative swept spheres and a sparse 3D cell hash. It is the
behavior oracle for any future GPU backend.

Acceptance coverage:

```text
tests/navigation_map/NavigationMapContractTests.cpp
tests/navigation_map/run_mingw64.sh
tests/architecture_contracts/check_navigation_map_boundary.py
```

Target-machine evidence reported 2026-09-16:

```text
navigation_map: 1/1 PASS
100% tests passed, 0 failed
```

The tests cover ownership, large-coordinate rebasing, stable basis conversion,
dynamic corridor filtering, sparse local query behavior, explicit rejected/out-
of-bounds inputs and atomic rejection of invalid frame publication.

## CPU benchmark ready — active measurement gate

Isolated benchmark:

```text
benchmarks/navigation_map/
    CMakeLists.txt
    main.cpp
    run_mingw64.sh
    README.md
```

It consumes only the public NavigationMap API and mirrors the GPU prototype
scenario classes at 1k/5k/10k actors for `cruise` and `hub`. It measures:

```text
snapshot publication/rebuild median + p95
corridor query median + p95
sphere/local query median + p95
candidate counts
cells visited / occupied cells / actors examined
indexed / rejected / out-of-bounds counts
```

The resulting CPU CSV is still required before the dynamic backend is selected.

## GPU feasibility state

`benchmarks/navigation_gpu/` is the existing isolated OpenGL 4.3 compute
prototype. GPU is an **implementation candidate**, not yet the production backend.

The strongly GPU-suitable work is:

```text
P/V/A actor prediction
conservative future swept bounds
spatial binning/hash
corridor/local-horizon relevance filtering
all-agent neighbor/conflict candidate generation
```

The architectural goal is scene reduction: thousands of actors are filtered to a
small relevant/conflicting subset before expensive per-agent planning.

Dynamic P/V/A must remain actor-owned. Do not build a dense 3D field that stores
velocity/acceleration in every cell. At `256^3`, six float channels for V+A alone
are already about 384 MiB before occupancy, clearance, topology or IDs; `512^3`
is eight times larger. Spatial representation therefore remains sparse/
hierarchical and cells contain indexing/occupancy/relevance data.

Single-agent A*/Theta*/SIPP is not the first GPU migration target. Precision/global
graph search remains suitable for asynchronous CPU workers until batched evidence
shows a reason to move it.

## GPU scheduling invariant

No production navigation path may synchronously wait for compute completion or
perform bulk blocking GPU readback on the frame thread.

Required model:

```text
frame N:   submit dynamic NavigationWorld work
frame N+1: consume last completed bounded result; submit next work
```

Use double/triple buffering. Detailed products should remain GPU-resident where
useful or transfer through bounded asynchronous compact buffers.

Compute latency is part of the local physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

## Navigation / collision / damage boundary

These remain separate authorities even where broadphase data is shared:

```text
Navigation
    conservative envelopes / clearance / predicted conflicts

Physics / Collision
    candidate pairs -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local navigation invalidation
```

Render, collision, hit/damage and navigation geometry need not match. A breach is
navigable only when clearance admits the requesting agent envelope.

## Performance targets

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
full/precision route solve       asynchronous; never a frame-thread blocker
```

These are design targets, not cross-machine test assertions. Rendering competes
for the same GPU budget.

## Active decision gate / next step

Current stage remains `NAV-V2-MAP-2`:

1. keep the CPU `NavigationMap` backend as the deterministic correctness oracle;
2. run the CPU benchmark on the user's MinGW64 machine;
3. run/compare the isolated GPU compute benchmark on the same 1k/5k/10k classes;
4. compare CPU/GPU/hybrid time, memory, candidate reduction and transfer cost;
5. choose the internal backend without changing the public NavigationMap API;
6. then start `NAV-V2-SPACE-1` for static free-space, clearance,
   connectivity/portals, local invalidation and agent-envelope queries;
7. only after these isolated gates integrate NavigationWorld into the live game.

No further design should treat Hub space as the universal navigation space, and
no new v2 work should reintroduce periodic heavy synchronous route solving on the
frame thread.
