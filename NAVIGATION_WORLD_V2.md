# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract / benchmark-first implementation plan  
**Updated:** 2026-09-16 Europe/Kyiv  
**Repository baseline when written:** `main@9d90d86f45fec1ade5ab899ca327fad00bfe95df`  
**Active validation note:** `tests/navigation_map` currently exists in the user's local working tree and passed locally, but was **LOCAL / UNPUSHED** when this document was written.

This document defines the current Navigation v2 direction. Older navigation/path-planning code remains useful implementation material and regression evidence, but it is **not automatically the architectural foundation of v2** where it conflicts with this contract.

## 1. Decision summary

Navigation v2 is built around one shared **NavigationWorld** for the active play area.

It is not an expensive independent full navigation world/planner per NPC. Shared spatial indexing, motion prediction and broadphase/conflict information are computed centrally; individual agents consume compact local candidates, temporary goals or precision-planning requests.

The implementation is **benchmark-first**. Before a large in-game rewrite, a standalone prototype must compare GPU, CPU spatial-index and hybrid approaches with 1k/5k/10k moving actors.

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

The active NavigationWorld is expressed near the player/active ship so local spatial structures do not need to work directly in huge system coordinates.

The origin may translate/rebase with the ship or active area.

### 2.3 Stable navigation axes

Ship-centered does **not** mean ship-body-attitude-centered.

The NavigationWorld axes must remain stable in a system/travel/navigation basis. A player roll, pitch or yaw must not force the whole spatial index to rotate/rebuild. Hull/body-local coordinates are used at the flight-controller boundary, not as the shared NavigationWorld basis.

## 3. Hub remains a separate local navigation domain

Hub/station-local navigation retains its own local contract, including the existing `prograde / radial / normal` semantics.

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

## 4. One central NavigationWorld

Conceptual runtime layout:

```text
                   SHIP NAVIGATION WORLD
                           |
              +------------+------------+
              |                         |
       STATIC SPACE              DYNAMIC ACTORS
     sparse volume/BVH           P,V,A,bounds
              |                         |
              +------------+------------+
                           |
                    shared broadphase
                 spatial bins / prediction
                   swept-volume queries
                           |
                  compact conflict list
                           |
             +-------------+-------------+
             |                           |
       mass NPC steering          precision planner
       cheap local targets         docking/repair
             |                           |
             +-------------+-------------+
                           |
                    temporary goals
                           |
                         Ruckig
                           |
                     controllers
```

The important ownership rule is: **shared world data and broadphase are central; expensive precision work is requested only where needed.**

## 5. Static spatial data vs dynamic actors

Do not encode dynamic velocity/acceleration as dense per-voxel vectors.

A `256^3` dense volume already contains about 16.8 million cells. Merely storing six 32-bit floats (`velocity + acceleration`) costs roughly 384 MiB before occupancy, clearance, IDs, connectivity or alignment. `512^3` multiplies that by eight.

Dynamic motion therefore belongs to actor records / structure-of-arrays storage, conceptually:

```cpp
Actor {
    position;
    velocity;
    acceleration;
    bounds;
    flags;
}
```

Static/spatial structures may contain:

- occupied/free state;
- clearance/safety information;
- connectivity or hierarchy information;
- references to static collision/navigation geometry;
- dynamic bin/list references or ranges.

The exact static representation is intentionally **not frozen yet**. Sparse voxel bricks, hierarchical cells, BVH-like structures and CPU spatial indexes remain valid candidates until benchmark evidence selects the backend.

## 6. Initial GPU responsibilities

GPU work should target tasks with high independent parallelism.

### 6.1 Local occupancy/navigation-volume construction

Where a volume representation proves useful, nearby static collision/navigation geometry can populate sparse bricks or another compact local representation in parallel.

This is an implementation candidate, not a requirement that all navigation become voxel-based.

### 6.2 Dynamic actor spatial binning

For thousands of actors, compute spatial hash/grid/bin membership centrally so later queries inspect nearby candidates rather than the whole actor set.

### 6.3 Future motion and swept bounds

For each actor, predict motion over the safety horizon. A constant-acceleration baseline is:

```text
P(t) = P0 + V*t + 0.5*A*t^2
```

The output may be swept capsules, AABBs/OBBs, conservative sampled bounds or a later model appropriate to the actor's known route/controller.

### 6.4 Corridor filtering

Given an accepted/coarse ship corridor, test dynamic swept bounds in parallel and return a compact relevant-actor list. The precision planner should see the few actors that can actually intersect the corridor, not all actors in the scene.

### 6.5 Shared all-agent broadphase

A single dispatch/index update may produce compact potential-conflict pairs for many NPCs at once:

```text
agent A <-> actor B
agent A <-> obstacle 57
agent C <-> cargo 22
...
```

This shared broadphase is preferable to repeating `agent × samples × all obstacles` independently for every actor.

## 7. Work that stays on CPU initially

Do not move single-agent A*/Theta*/SIPP-style precision graph search to GPU merely because a GPU exists.

These searches tend to be:

- branch-heavy;
- priority-queue/frontier heavy;
- irregular in memory access;
- too small to occupy many GPU lanes for a single query.

Initially, sparse/rare graph search belongs on an asynchronous CPU worker after the shared spatial stage has reduced the problem.

Example target reduction:

```text
5000 dynamic actors
        |
        | shared binning / corridor filter
        v
5-20 relevant dynamic conflicts

millions of possible spatial cells
        |
        | hierarchy / visibility / coarse space
        v
tens or hundreds of route nodes
```

If later measurements show hundreds of simultaneous graph searches can be batched efficiently, GPU graph-search experiments may be reconsidered.

## 8. Asynchronous pipeline — never wait synchronously for GPU

This architecture forbids a frame-critical pattern such as:

```text
dispatch compute
memory barrier
read back result
CPU waits
```

Use double/triple buffering or equivalent asynchronous result ownership:

```text
frame N:
    submit NavigationWorld update N

frame N+1:
    consume last completed safe result
    submit next update
```

Exact latency may differ by backend, but the render/main thread must not stall waiting for navigation GPU readback or a long worker planner.

Compact GPU outputs are preferred: candidate actor IDs, conflict pairs, ranges, flags or small route-query buffers rather than copying the full spatial world back to CPU.

## 9. Safety horizon includes compute latency

Pipeline delay is part of the physical safety calculation, not an implementation footnote.

At minimum, conservative distance budgeting includes:

```text
D_safe = v * T_latency + v^2 / (2 * a_brake) + margin
```

Where:

- `v` is relevant relative/ship speed;
- `T_latency` covers navigation observation/compute/result age;
- `a_brake` is available conservative braking deceleration;
- `margin` covers geometry/model/prediction uncertainty and policy clearance.

Higher-fidelity relative-motion models may replace this baseline, but they must preserve the same principle: stale asynchronous results consume part of the safety budget.

## 10. Performance contract

The current target budget is:

```text
CPU main-thread navigation:
    < 0.5 ms typical
    < 1.0 ms acceptable peak

GPU dynamic NavigationWorld update:
    target < 1 ms
    ~2 ms heavy-scene ceiling before reconsidering representation/workload

CPU worker coarse/global route:
    several ms acceptable, asynchronous

rare docking / precision replan:
    ~10-30 ms worker time may be acceptable
    never blocks a frame
```

GPU time is not free: compute competes with rendering. The goal is not “put navigation on GPU”; the goal is to place massively parallel work where it reduces total frame cost and problem size.

## 11. Ruckig contract

Ruckig is **not** responsible for discovering a path around obstacles.

NavigationWorld / route planners first determine an accepted safe path, corridor, temporary goal and motion constraints. Ruckig then generates a kinematically feasible trajectory/controller target sequence consistent with velocity/acceleration/jerk limits.

Conceptually:

```text
world + conflicts
      |
coarse/local/precision navigation
      |
accepted safe path / temporary goal
      |
Ruckig
      |
trajectory/controller commands
```

## 12. Guidance tunnel contract

The manual guidance tunnel is a visual projection of the **accepted trajectory/corridor actually used by navigation/control**.

It must not run an unrelated planner or present a pretty path that the controller cannot or will not follow.

For manual flight the tunnel is the primary human-readable navigation output; for autopilot the same trajectory data can drive control without requiring the tunnel to be visible.

## 13. Immediate implementation: standalone backend benchmark

Before integrating/replacing the in-game navigator, build a standalone benchmark/prototype with at least:

```text
1,000 moving actors
5,000 moving actors
10,000 moving actors

ship-centered 3D working volume
actor P/V/A/bounds
spatial binning/indexing
future prediction
swept bounds
player-corridor filtering
all-agent neighbour/conflict queries
```

Compare at least:

1. CPU spatial-index implementation;
2. GPU binning/prediction/broadphase implementation;
3. hybrid design where GPU performs broadphase/prediction and CPU performs sparse precision search.

### Required benchmark metrics

Record independently:

- GPU execution time;
- CPU submission/setup time;
- CPU-only index/query time;
- memory usage;
- candidate-pair count;
- corridor candidate count;
- readback bytes;
- readback/wait stalls (must be zero on the frame path in the intended architecture);
- scaling from 1k -> 5k -> 10k actors.

The benchmark result is a **decision gate**. Do not commit the production NavigationWorld backend to GPU, CPU or hybrid by intuition alone.

## 14. Integration roadmap after benchmark

### Phase 0 — backend benchmark

Measure GPU vs CPU vs hybrid. Current task.

### Phase 1 — freeze interfaces

Freeze NavigationWorld ownership/data APIs based on measured backend choice. Keep backend details behind interfaces where practical so benchmark code does not leak arbitrary representation into gameplay.

### Phase 2 — ship-centered spatial world

Implement static/local geometry ingestion, dynamic actor arrays/bins, frame-boundary transforms and asynchronous update ownership.

### Phase 3 — conflicts + temporary goals / receding horizon

Generate cheap local goals/corridors for mass traffic and focused conflict sets for the active player/precision planners. Expensive detail is limited to the current safety horizon rather than the entire route.

### Phase 4 — precision planner

Use sparse CPU-worker planning for docking, repair/service approaches, difficult obstacle topology and rare hard replans.

### Phase 5 — Ruckig integration

Convert accepted route/goal constraints into kinematically feasible trajectories/controllers.

### Phase 6 — guidance tunnel

Render the same accepted trajectory/corridor for manual navigation.

## 15. Current validation status

On 2026-09-16 the user reported:

```text
bash tests/navigation_map/run_mingw64.sh
1/1 Test #1: navigation_map ... Passed 0.04 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.05 sec
```

At the time this architecture document was committed, those `tests/navigation_map` files were not present in the observed remote `main@9d90d86`; therefore this evidence is labelled **LOCAL / UNPUSHED**.

Once the local work is pushed, this section must be updated with the pushed commit SHA and remote-visible test paths.

## 16. Documentation Definition of Done

Navigation work is not complete when only code/tests change.

For every meaningful NavigationWorld iteration:

1. update this file whenever architecture, ownership, performance budgets, frame contracts or roadmap decisions change;
2. update the project context `CURRENT_STATE.md` and `CURRENT_TASK.md`;
3. record actual test/benchmark evidence in the append-only iteration log;
4. update accepted decisions when a contract changes;
5. distinguish local-only work from pushed repository state explicitly;
6. perform the documentation freshness check **before** declaring an iteration complete or starting the next coding slice.

A stale current-task/state document is a project-state defect and blocks handoff to the next slice.
