# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract / benchmark-first implementation plan  
**Updated:** 2026-09-16 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-MAP-2`

This document defines the current Navigation v2 direction. Older route/path-planning code remains useful migration material and regression evidence, but it is not automatically the architectural foundation of v2 where it conflicts with this contract.

Repository/branch authority is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. `main` is the only canonical game-development branch.

## 1. Decision summary

Navigation v2 is built around one shared **NavigationWorld** for the active play area.

It is not an expensive independent full navigation world/planner per NPC. Shared spatial indexing, motion prediction and broadphase/conflict information are computed centrally; individual agents consume compact local candidates, temporary goals or precision-planning requests.

The active dynamic-map implementation is the backend-neutral `NavigationMap` block under `src/world/navigation/map/`. Its CPU reference implementation is the deterministic behavior oracle. CPU, GPU or hybrid production ownership is selected from measured evidence, not intuition.

`EliteNavigationMap` is intentionally not yet wired into the live `EliteGame` / `EliteServer` runtime path. Isolated backend/space gates come first.

## 2. Coordinate and frame contract

Authoritative physical/system state and the planner's working space are deliberately separate.

```text
AUTHORITATIVE SYSTEM STATE
        |
        | transform relevant state at boundary
        v
SHIP NAVIGATION SPACE
origin = ship / active play area
axes   = stable navigation/travel basis
        |
        +-- static/local geometry
        +-- nearby dynamic actors
        +-- P,V,A,bounds
        +-- predicted swept volumes
        +-- active corridors/conflicts
```

### 2.1 Authoritative system state

Large-scale/system state remains the source of truth for precise positions and transforms between moving frames. Navigation does not redefine world physics merely to obtain numerically convenient local coordinates.

### 2.2 Ship-centered working origin

The active NavigationWorld is expressed near the player/active ship so local spatial structures do not need to work directly in huge system coordinates. The origin may translate/rebase with the ship or active area.

### 2.3 Stable navigation axes

Ship-centered does **not** mean ship-body-attitude-centered. NavigationWorld axes remain stable in a system/travel/navigation basis. Player roll, pitch or yaw must not force the whole spatial index to rotate/rebuild. Hull/body-local coordinates are flight-controller detail.

## 3. Hub remains a separate local navigation domain

Hub/station-local navigation retains its own local contract.

```text
HUB LOCAL SPACE
    station geometry
    docking ports
    scheduled/service actors
    internal traffic
          |
          | transform relevant subset only
          v
SHIP NAVIGATION SPACE
```

The game must not drag/rebuild the entire internal Hub map merely because the ship moves. NavigationWorld imports only station geometry, traffic, corridors and docking/service information that can affect the ship's active safety/planning horizon.

The same ownership model applies to other persistent local domains such as carriers, capital ships, settlements and interiors.

## 4. Shared NavigationWorld ownership

Conceptual runtime layout:

```text
                   SHIP NAVIGATION WORLD
                           |
              +------------+------------+
              |                         |
       STATIC SPACE              DYNAMIC ACTORS
  free-space/clearance/          P,V,A,bounds,
  connectivity/portals           flags,revisions
              |                         |
              +------------+------------+
                           |
                    shared broadphase
                 spatial index/prediction
                    swept-volume queries
                           |
                  compact conflict set
                           |
             +-------------+-------------+
             |                           |
       mass NPC steering          precision planner
       cheap local targets         docking/repair
             |                           |
             +-------------+-------------+
                           |
                    temporary target
                           |
               RuckigTrajectorySolver
                           |
                     flight control
```

Shared world data and broadphase are central; expensive precision work is requested only where needed.

## 5. NavigationMap block boundary

Production-facing API:

```text
src/world/navigation/map/NavigationMap.h
```

Implementation/build boundary:

```text
src/world/navigation/map/NavigationMap.cpp
src/world/navigation/map/CMakeLists.txt
src/world/navigation/map/README.md
```

Ingress is an owned snapshot by value:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

Egress is compact derived data by value:

```text
queryCorridor()
querySphere()
stats()
```

The block owns world/system -> ship-centered conversion, actor storage, prediction, spatial indexing and backend resources. Internal cells, actor tables and future GPU buffers never cross the public API. The public header remains independent of GLM/OpenGL/GLFW/game/render/scene state and uses PImpl.

The current CPU reference provides constant-acceleration endpoint prediction, conservative swept spheres and a sparse 3D cell hash. It is the correctness oracle against which future backend changes are tested.

## 6. Static spatial data vs dynamic actors

Do not encode dynamic velocity/acceleration as dense per-voxel vectors.

A `256^3` dense volume already contains about 16.8 million cells. Six 32-bit floats for velocity + acceleration alone cost roughly 384 MiB before occupancy, clearance, IDs, connectivity or alignment; `512^3` multiplies that by eight.

Dynamic motion therefore belongs to actor records / structure-of-arrays storage. Static/spatial structures may contain occupied/free state, clearance/safety information, connectivity/hierarchy, references to static navigation geometry, and compact dynamic-bin references.

The exact static representation is not frozen yet. Sparse bricks, hierarchical cells, BVH-like structures and CPU spatial indexes remain valid until benchmark evidence and `NAV-V2-SPACE-1` select the representation.

## 7. GPU candidate responsibilities

GPU work should first target high independent parallelism:

```text
P/V/A actor prediction
conservative future swept bounds
spatial hash/binning
corridor/local-horizon relevance filtering
all-agent neighbour/conflict candidate generation
```

Single-agent A*/Theta*/SIPP-style precision graph search is not the first GPU target. These searches are branch-heavy with irregular frontier/memory access. Initially they remain asynchronous CPU-worker candidates after the shared spatial stage has reduced the problem.

A hybrid backend is explicitly valid: CPU may own static topology/precision search while GPU owns dynamic actor prediction, binning and conflict reduction.

## 8. Asynchronous pipeline — never block the frame thread

This architecture forbids frame-critical behavior such as:

```text
dispatch compute
barrier/fence
blocking bulk readback
continue frame
```

Use double/triple buffering or equivalent asynchronous ownership:

```text
frame N:   submit NavigationWorld update N
frame N+1: consume last completed safe result; submit N+1
```

Detailed products should remain backend-resident where useful or cross to CPU only as bounded compact asynchronous results. Navigation result age must be observable and included in safety budgeting.

## 9. Safety horizon includes compute/result latency

Pipeline delay is part of the physical safety calculation.

Baseline conservative budget:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Higher-fidelity relative-motion models may replace the simple baseline, but stale asynchronous results always consume part of the safety margin.

## 10. Performance contract

Current design targets:

```text
main-thread navigation CPU       < 0.5 ms typical
                                 < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred
                                 < 2.0 ms heavy-scene target
full/precision route solve       asynchronous only
```

GPU time is not free: navigation compute competes with rendering. The objective is total frame/system cost and bounded latency, not moving work to GPU for its own sake.

## 11. Ruckig contract

Ruckig is **not** responsible for discovering a path around obstacles.

NavigationWorld / route planners first determine an accepted safe corridor, local route or temporary target state. `RuckigTrajectorySolver` then produces kinematically feasible local motion under velocity/acceleration/jerk limits.

```text
world + conflicts
      |
coarse/local/precision navigation
      |
accepted corridor / temporary target
      |
RuckigTrajectorySolver
      |
flight control
```

The old whole-route `RuckigRoutePlanner`/dense materialization path is migration code, not v2 authority.

## 12. Guidance tunnel contract

The manual guidance tunnel is a visual projection of the **accepted trajectory/corridor actually used by navigation/control**.

It must not run an unrelated planner or present a visually attractive path that the controller cannot or will not follow. Manual and autopilot modes consume the same navigation truth; only presentation/control ownership differs.

## 13. Active implementation gate — `NAV-V2-MAP-2`

Canonical isolated programs now present on `main`:

```text
tests/navigation_map/
benchmarks/navigation_map/
benchmarks/navigation_gpu/
```

User target-machine behavioral evidence already reported before branch reconciliation:

```text
bash tests/navigation_map/run_mingw64.sh
1/1 Test #1: navigation_map ... Passed
100% tests passed, 0 tests failed
```

The audited implementation/test tree is now represented in `main`; a fresh post-reconciliation run remains the next verification step.

CPU and GPU benchmark harnesses use matched deterministic `cruise` and `hub` scenarios at 1k / 5k / 10k moving actors with a 3 s default prediction horizon.

Required measurements include:

- CPU publication/rebuild median and p95;
- CPU corridor/local query median and p95;
- cells visited, actors examined and candidate counts;
- GPU bin/prediction/corridor time;
- GPU neighbor/conflict time;
- GPU total median and p95;
- CPU submission cost;
- memory footprint;
- overflow/rejected/out-of-bounds diagnostics;
- readback bytes and any wait/stall behavior.

The benchmark result is a **decision gate**. Do not commit production dynamic NavigationWorld ownership to CPU, GPU or hybrid by intuition alone.

## 14. Route architecture after backend selection

The intended route mechanism is:

```text
cached sparse global corridor through free-space regions/portals
        |
NavigationMap corridor/local-horizon query
        |
predicted conflicts for returned actors only
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

A global route is not expanded into thousands of dense samples all the way to destination. Global route validity is revision/event driven; local physical avoidance operates on a bounded receding horizon.

## 15. Integration roadmap

### Stage `NAV-V2-MAP-2` — current

Measure CPU vs GPU vs hybrid dynamic-map behavior on the target machine and select internal backend ownership without changing the public `NavigationMap` API.

### Stage `NAV-V2-SPACE-1`

Add persistent static free-space/clearance, connectivity/portals, local invalidation and agent-envelope queries.

### Live integration

Integrate the bounded asynchronous shared NavigationWorld into runtime only after the isolated map/space gates are accepted.

### Consumers

Add cheap mass-NPC steering/avoidance and more expensive precision docking/repair/special planning as separate consumers of the same shared spatial/prediction layer.

### Legacy retirement

Remove obsolete route-wide planner/materialization/guidance dependencies when Navigation v2 owns the live path. Retain the low-level local kinematic solver where appropriate.

## 16. Navigation / collision / damage boundary

```text
Navigation
    conservative envelopes / free-space / predicted conflicts

Physics / Collision
    broadphase candidates -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local navigation invalidation
```

Render, collision, hit/damage and navigation geometry intentionally differ. A breach is navigable only when clearance admits the requesting agent envelope.

## 17. Raw NavigationWorld 3D debug-view contract

A dedicated diagnostic presentation mode is required so the developer can inspect the **actual NavigationWorld state used by navigation**, not a separately reconstructed or beautified approximation.

Input contract:

- ordinary `F12` keeps its existing Hub/local presentation behavior;
- `Shift+F12` toggles `Hub render <-> NavigationWorld Debug` while in the Hub/local context;
- entering the debug view is presentation-only: it must not mutate simulation/navigation state or force a synchronous NavigationWorld rebuild/readback.

Rendering contract:

- while NavigationWorld Debug is active, normal Hub-map rendering is suppressed;
- the debug renderer consumes the same completed NavigationWorld snapshot that navigation/control consumes;
- there is **no second navigation calculation for visualization**;
- the renderer is backend-neutral so CPU, GPU and hybrid implementations expose the same diagnostic snapshot contract;
- the camera supports useful free 3D inspection independently of normal Hub-map presentation.

The debug view should expose, where available:

- ship-centered origin/axes;
- spatial cells/bricks/bins or equivalent static/index structure;
- static obstacles and clearance/safety representation;
- dynamic actor bounds plus velocity/acceleration vectors;
- predicted positions and swept volumes;
- player safety horizon / accepted corridor;
- broadphase candidate and conflict actors/pairs;
- snapshot/frame/generation identifier and result age/latency.

This is a correctness instrument. When navigation behaves incorrectly, it must show **the world navigation actually believed existed at that completed snapshot**, including stale-result age where applicable.

## 18. Documentation Definition of Done

Navigation work is not complete when only code/tests change.

For every meaningful NavigationWorld iteration:

1. update this contract when architecture, ownership, performance budgets, frame contracts or roadmap decisions change;
2. update `CURRENT_STATE.md`, `CURRENT_TASK.md` and `PROJECT_STATE.md`;
3. record actual test/benchmark evidence in the project iteration log/context as applicable;
4. keep `main` as the only canonical development branch;
5. distinguish genuine local-only work from repository state explicitly;
6. perform documentation freshness checks before declaring the iteration complete or starting the next coding slice.

A stale current-task/state document or branch ambiguity is a project-state defect and blocks handoff.