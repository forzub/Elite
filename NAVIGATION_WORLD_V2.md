# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-16 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-SPACE-1`

Repository/branch authority is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. `main` is the only canonical game-development branch.

## 1. Decision summary

Navigation v2 is built around one shared **NavigationWorld** for the active play area.

It is not a complete independent navigation world/planner per NPC. Shared static-space topology, dynamic spatial indexing, motion prediction and broadphase/conflict information are computed centrally; agents consume compact local products, coarse corridors or precision-planning requests.

The dynamic `NavigationMap` boundary under `src/world/navigation/map/` is accepted. `NAV-V2-MAP-2` target-machine measurements selected a **hybrid backend**:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

The public NavigationWorld/NavigationMap boundaries remain backend-neutral. `EliteNavigationMap` is intentionally not yet wired into the live game/server path; the isolated static-space gate comes first.

## 2. Coordinate and frame contract

Authoritative physical/system state and navigation working space are separate.

```text
AUTHORITATIVE SYSTEM STATE
        |
        | publish relevant transformed state
        v
SHIP NAVIGATION SPACE
origin = active ship / active play area
axes   = stable navigation/travel basis
        |
        +-- static navigation space
        +-- nearby dynamic actors
        +-- P,V,A,bounds
        +-- predicted swept volumes
        +-- active corridors/conflicts
```

The NavigationWorld origin may translate/rebase. Its axes do **not** roll/pitch/yaw with the hull. Hull/body-local coordinates are flight-controller detail; render/player-relative coordinates are presentation detail.

## 3. Local-domain ownership

Hub/station/carrier/interior navigation remains a separate local domain.

```text
LOCAL DOMAIN
    geometry
    docking/service ports
    scheduled/internal traffic
    local damage/openings
          |
          | publish relevant subset
          v
SHIP NAVIGATION SPACE
```

The game must not rebuild or drag an entire station/interior map merely because the ship moves. Only geometry/topology/traffic relevant to the active NavigationWorld is published across the boundary.

## 4. Shared NavigationWorld ownership

```text
                 SHIP NAVIGATION WORLD
                         |
             +-----------+-----------+
             |                       |
       STATIC SPACE             DYNAMIC ACTORS
 free-space/clearance/          P,V,A,bounds,
 connectivity/portals           flags,revisions
             |                       |
             +-----------+-----------+
                         |
                  shared reduction
             static corridor + dynamic
             prediction/conflict set
                         |
            +------------+------------+
            |                         |
     mass NPC steering          precision planner
      cheap local target        docking/repair/etc.
            |                         |
            +------------+------------+
                         |
                  temporary target
                         |
              RuckigTrajectorySolver
                         |
                   flight control
```

## 5. Dynamic NavigationMap boundary — accepted

Production-facing API:

```text
src/world/navigation/map/NavigationMap.h
```

Ingress:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

Egress:

```text
queryCorridor()
querySphere()
stats()
```

The block owns system/world -> ship-centered conversion, actor storage, prediction, dynamic spatial indexing and backend resources. Internal cells, actor tables and GPU buffers never cross the public API. The public header remains independent of GLM/OpenGL/GLFW/render/game state.

The CPU implementation remains the deterministic behavior oracle even though production dynamic reduction is allowed to use GPU internals.

## 6. `NAV-V2-MAP-2` measured evidence — accepted

### CPU 100-iteration pass

```text
horizon_s=3 warmup=10 iterations=100
```

At 10k actors:

```text
cruise rebuild  med/p95 = 2.7892 / 3.0958 ms
       corridor med/p95 = 0.0552 / 0.1713 ms
       sphere   med/p95 = 0.0346 / 0.1329 ms

hub    rebuild  med/p95 = 2.1775 / 2.4924 ms
       corridor med/p95 = 0.0328 / 0.1551 ms
       sphere   med/p95 = 0.0324 / 0.0471 ms
```

Compact CPU queries are cheap. Whole-snapshot rebuild is the CPU cost center and must not become a synchronous per-frame 10k path.

### GPU 100-iteration pass

Accepted adapter:

```text
OpenGL 4.3
NVIDIA Corporation
Quadro RTX 3000/PCIe/SSE2
```

At 10k actors:

```text
cruise bin=0.0106 ms neighbor=0.6737 ms total=0.6840 ms p95=1.3226 ms
hub    bin=0.0105 ms neighbor=1.5991 ms total=1.6097 ms p95=1.6258 ms
```

All GPU scenarios reported:

```text
overflow=0
out_of_bounds=0
valid=1
readback_bytes=32
```

Both 1k scenarios exactly matched the independent CPU pair/corridor reference. 10k memory was about 16.63 MiB.

The prediction/binning pass is approximately 0.01–0.02 ms; neighbor/conflict reduction dominates. Dense Hub traffic is therefore the main dynamic optimization target.

Raw evidence: `benchmarks/navigation_gpu/RUN_LOG.md`.

## 7. Hybrid backend contract

The CPU and GPU harnesses measure complementary workloads, not equivalent totals. Hybrid ownership is selected because the measurements support specialization without changing the public boundary.

### CPU owns

- persistent static free-space / obstacle topology;
- clearance/agent-envelope logic;
- explicit region/portal connectivity;
- sparse cached global corridor search;
- precision local search after spatial reduction;
- deterministic reference queries and diagnostics.

### GPU owns

- dynamic actor P/V/A prediction;
- conservative swept bounds;
- spatial binning/hash;
- high-volume all-agent neighbor/conflict candidate generation.

### Shared invariants

- no backend leaks internal data structures through public APIs;
- no frame-thread `dispatch -> wait -> bulk readback` path;
- dynamic results are double/triple buffered or otherwise asynchronous;
- result age/generation is observable;
- stale-result age is included in physical safety distance.

## 8. Asynchronous safety horizon

Navigation result latency consumes physical safety margin.

Baseline conservative budget:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Higher-fidelity relative-motion models may replace this baseline, but asynchronous result age is never ignored.

## 9. Performance contract

```text
main-thread navigation CPU       < 0.5 ms typical
                                 < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred
                                 < 2.0 ms heavy-scene target
full/precision route solve       asynchronous only
```

The 100-iteration Quadro measurements satisfy the current heavy-scene GPU target at 10k actors. This does not remove the need to optimize dense-Hub neighbor reduction because rendering competes for the same GPU.

## 10. Active static-space stage — `NAV-V2-SPACE-1`

The static spatial half of NavigationWorld is now the active implementation task.

Required capability:

```text
persistent sparse static-space representation
obstacle/free-space semantics
clearance by agent envelope
explicit connectivity / portals
bounded local invalidation/rebuild
coarse global corridor query
outside <-> inside transitions
narrow-passage admission by agent size
```

Do not begin with a giant dense system voxel field. Large open volumes and detailed interiors have different spatial characteristics; a sparse/hierarchical or region/portal + local-clearance hybrid representation is explicitly allowed.

The first implementation must preserve representation freedom behind a backend-neutral public API.

### Static-space public concepts

Exact names may differ, but the boundary needs equivalents of:

```text
StaticSpace / NavigationSpace
revision / generation
AgentEnvelope
RegionId
PortalId
Region
Portal
point/containing-region query
clearance query
coarse corridor query
local invalidation/update
stats / diagnostics
```

Public headers must not expose GLM/OpenGL/GLFW/render/game-state dependencies. Internal acceleration structures remain private.

### Static-space behavioral requirements

The deterministic CPU reference must demonstrate:

```text
open-space traversal
solid-obstacle clearance rejection
same geometry with different agent envelope outcomes
connected region corridor
disconnected no-route result
narrow portal oversized-agent rejection
outside -> interior transition
bounded local invalidation/revision behavior
```

A standalone architecture contract, MinGW64 behavioral test and small query/invalidation benchmark are required before live integration.

## 11. Route architecture

The intended route mechanism is:

```text
cached sparse global corridor through free-space regions/portals
        |
static clearance + dynamic NavigationMap local-horizon query
        |
predicted conflicts for relevant actors
        |
local route / velocity correction
        |
temporary target state
        |
RuckigTrajectorySolver
        |
execute first part
        |
repeat on receding physical horizon
```

A global route is not expanded into thousands of dense samples to destination. Global topology validity is revision/event driven; local physical avoidance operates on a bounded receding horizon.

## 12. Ruckig contract

Ruckig is not responsible for obstacle pathfinding.

NavigationWorld first determines an accepted corridor/local target state. `RuckigTrajectorySolver` then creates kinematically feasible local motion under velocity/acceleration/jerk constraints.

The old whole-route `RuckigRoutePlanner`/dense-materialization path is migration code, not v2 authority.

## 13. Guidance tunnel contract

Manual guidance visualizes the accepted corridor/trajectory actually used by navigation/control. It must not run an unrelated planner or display a path that the controller will not follow.

Manual and autopilot modes consume the same navigation truth; presentation/control ownership differs.

## 14. Navigation / collision / damage boundary

```text
Navigation
    conservative envelopes / free-space / predicted conflicts

Physics / Collision
    broadphase candidates -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local static-space invalidation
```

Render, collision, damage and navigation geometry intentionally differ. A breach is navigable only when clearance admits the requesting agent envelope.

## 15. Raw NavigationWorld 3D debug-view contract

Ordinary `F12` keeps existing Hub/local presentation behavior. `Shift+F12` toggles Hub render <-> NavigationWorld Debug in the local context.

The debug view must consume the same completed backend-neutral NavigationWorld snapshot used by navigation/control. It must not run a second planner, force synchronous readback or mutate simulation state.

Useful diagnostics include:

- ship-centered origin/axes;
- static regions/cells/bricks/portals and clearance;
- dynamic actors with P/V/A;
- predicted/swept bounds;
- accepted corridor and physical horizon;
- conflict candidates/pairs;
- snapshot generation and result age.

This remains a documented contract; runtime implementation is not yet present.

## 16. Integration roadmap

### `NAV-V2-SPACE-1` — current

Implement/test/benchmark the static-space boundary and deterministic CPU reference.

### Live shared NavigationWorld

After static-space acceptance, integrate bounded asynchronous publication/consumption into runtime.

### Consumers

Add cheap mass-NPC local steering and expensive precision docking/repair/special planning as separate consumers of the same shared world.

### Legacy retirement

Remove obsolete route-wide planner/materialization/guidance dependencies only after Navigation v2 owns the live path. Retain the low-level local kinematic solver where useful.

## 17. Documentation Definition of Done

For every meaningful NavigationWorld iteration update, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
project-context CURRENT_STATE/CURRENT_TASK/DECISIONS/ITERATION_LOG/SOURCES
```

Stale task/state documentation or branch ambiguity blocks handoff.
