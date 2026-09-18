# Navigation Planning Architecture

**Updated:** 2026-09-17  
**Status:** Navigation v2 architecture authority  
**Current implementation wave:** `NAV-V2-LOCAL-1` — dynamic conflict + local receding horizon

## Previous live planner is migration code

The old route-wide chain is not the v2 foundation:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route sampling
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

It proved useful experimentally, but route-wide materialization and synchronous validation scale with route length and can block the client update. Navigation v2 reduces the scene first and performs expensive reasoning only on the bounded relevant subset.

`SmoothPathOptimizer`, `GeometricPathPlanner`, route-wide `TrajectoryGenerator` usage and current guidance plumbing remain legacy/migration code. `TacticalCollisionMonitor` and `SmallCraftNavigation` are pre-v2 GLM/old-scene implementations and are not new ownership boundaries.

Ruckig remains a downstream local kinematic primitive after navigation has selected a temporary target state. It is not a free-space/path-search engine.

## Coordinate authority

Long-lived simulation state remains in precise system/world coordinates. Navigation v2 operates in a ship-centered working space with a translating/rebasing origin and stable navigation/travel axes. The axes do not rotate with instantaneous hull attitude.

Hub/station/carrier/interior navigation remains private to its local domain. Only relevant static geometry and dynamic actors are transformed/published into the active NavigationWorld.

## Shared NavigationWorld

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
ship-centered NavigationWorld
    + NavigationSpace
    |   static free-space / clearance
    |   connectivity / portals
    |   cached global corridor
    |
    + NavigationMap
    |   dynamic P/V/A actor table
    |   prediction / swept bounds
    |   spatial bins
    |   compact candidate reduction
    |
    + local horizon consumer
        bounded conflict assessment
        temporary target state
            |
            v
       Ruckig / flight control
```

Ordinary agents must not independently scan the full scene or build complete private navigation maps. Per-agent work begins only after shared spatial reduction.

## NavigationMap boundary — ACCEPTED

`src/world/navigation/map/NavigationMap.h` is the production-facing dynamic boundary. Callers publish authoritative actor snapshots by value; the block owns system/world -> map transform, actor storage, prediction and spatial indexing.

`queryCorridor()` and `querySphere()` return compact `Candidate` values containing map-space P/V/A, predicted endpoint, conservative swept sphere, radius, flags and motion revision. Internal cells, actor tables and GPU buffers never cross the API.

`NAV-V2-MAP-2` selected hybrid ownership from target-machine evidence:

```text
CPU
    static topology/search/reference work

GPU
    dynamic P/V/A prediction
    swept bounds
    spatial bins
    all-agent conflict reduction
```

At 10k actors the accepted GPU total was about 0.684 ms median cruise / 1.610 ms median hub, with p95 below 1.7 ms. CPU compact corridor/sphere queries were below 0.2 ms p95.

GPU work must remain asynchronous. This is prohibited:

```text
dispatch -> wait -> blocking bulk readback -> continue frame
```

## NavigationSpace boundary — ACCEPTED

`src/world/navigation/space/NavigationSpace.h` owns persistent static free-space topology.

Accepted capabilities:

- free-space regions and explicit portals;
- per-agent clearance;
- outside/interior passages, apertures, tunnels and canyons;
- deterministic topology and policy-aware corridor queries;
- local fail-closed invalidation;
- static turn-cost state `(RegionSlot, incoming PortalId)`.

Final target-machine positive-turn acceptance:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

The pinned gate was `<=40 ms p95`; `NAV-V2-SPACE-1` is closed. The accepted implementation prepublishes static geometry/clearance/turn angles so expanded transitions are cheap. Tree-backed state and weak Euclidean A* remain rejected historical paths.

Dynamic velocity, braking, traffic and pursuit prediction do not belong in persistent `NavigationSpace` cost.

## Global route and local physical horizon

A global route is sparse corridor intent through connected free-space regions/portals. It is cached and invalidated by revisions. It is not expanded into a dense trajectory to destination.

The local planner operates on a bounded receding physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

`T_latency` includes asynchronous snapshot/result age.

Normal sequence:

```text
cached static corridor / nominal local target
    -> NavigationMap corridor query
    -> compact predicted conflict candidates
    -> relative-motion / swept-volume assessment
    -> temporary local target or fail-closed result
    -> Ruckig local kinematics
    -> execute first part
    -> repeat on receding horizon
```

Farther route regions remain coarse intent. This prevents route length from linearly exploding per-frame collision/sample work.

## `NAV-V2-LOCAL-1` — ACTIVE

The first local-horizon boundary must be backend-neutral and deterministic. It consumes compact products rather than owning another world.

Required input concepts:

```text
agent map-space P/V/A + radius/clearance
nominal local target from accepted route intent
completed NavigationMap query result / candidates
snapshot/result revision and age
local horizon / safety policy
```

Required output concepts:

```text
status: clear / adjusted / hold-or-fail-closed
bounded temporary target state
primary conflict identity/diagnostics
map/source revision used
horizon and safety-margin diagnostics
```

The first reference may use relative-motion closest approach plus conservative swept-sphere checks. Precision space-time/SIPP/ORCA-family algorithms are not selected in advance; measurements and game constraints decide later.

### Explicit non-goals

- no full-scene scan;
- no second NavigationMap/NavigationWorld snapshot;
- no route-wide dense trajectory generation;
- no GLM/OpenGL/render/game dependencies in the new boundary;
- no synchronous GPU readback;
- no pursuit-specific logic in the generic local layer;
- no migration of dynamic traffic into static NavigationSpace costs.

If no safe local target is demonstrated, the result is fail-closed. Navigation does not lower collision/safety fidelity merely to meet timing.

## Dynamic conflict math

For each compact candidate, the reference can assess bounded relative motion:

```text
r = actorPosition - agentPosition
v = actorVelocity - agentVelocity

tClosest = clamp(-dot(r,v) / dot(v,v), 0, horizon)
dClosest = |r + v*tClosest|
```

Acceleration/predicted swept bounds remain conservative broadphase inputs. This closest-approach calculation is reference math, not authority to ignore the candidate's published swept radius or result age.

The old `TacticalCollisionMonitor` contains a similar closest-approach idea but owns GLM + old `NavigationContact/NavigationScene` data; new v2 code must not depend on that old surface.

## Pursuit and moving terminals

Moving-goal pursuit is a later consumer:

```text
moving target P/V/A
 -> bounded intercept prediction
 -> predicted intercept state/region
 -> reuse valid coarse corridor
 -> generic local horizon
```

Do not regenerate the whole global route every frame. Moving docking terminals likewise publish an entry state/manifold; the terminal planner owns final precision approach.

## Navigation / collision / damage separation

```text
NavigationWorld
    conservative envelopes / predicted conflicts

Physics / Collision
    broadphase -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    hit ownership -> detach / breach / destruction
    -> local static-space invalidation
```

A detached fragment becomes a dynamic NavigationMap actor. A visible hole becomes navigable only when rebuilt static free space admits the requesting envelope.

## Performance contract

| Work | Target |
| --- | ---: |
| Main-thread navigation CPU typical | <0.5 ms |
| Main-thread navigation CPU normal peak | <1.0 ms |
| GPU dynamic NavigationWorld | <1.0 ms preferred |
| GPU heavy scene | <2.0 ms target |
| Full/precision global solve | asynchronous only |

These are design budgets, not portable assertions. Numerical claims are accepted only from the user's target-machine output.

Different layers may run at different rates. Global topology is revision/event driven; local avoidance is bounded/receding; mass-agent updates may be staggered.

## Current gates

```text
NAV-V2-MAP-1   accepted dynamic API/reference boundary
NAV-V2-MAP-2   CLOSED — hybrid CPU/GPU dynamic backend evidence
NAV-V2-SPACE-1 CLOSED — static free-space/corridor/turn-aware reference
NAV-V2-LOCAL-1 ACTIVE — dynamic conflict + local receding horizon
```

`NAV-V2-LOCAL-1` requires an isolated architecture contract and deterministic MinGW64 behavioral tests before live game/server integration. Benchmarking follows after behavior is pinned.

## Non-negotiable v2 rules

- one shared NavigationWorld; no per-agent complete spatial world;
- backend internals never cross public boundaries;
- stable ship-centered navigation axes; hull attitude is not the shared index frame;
- no per-agent full-scene scans;
- no synchronous route-wide dense sampling/validation on the frame thread;
- no fixed arbitrary obstacle-count cap as correctness strategy;
- dynamic P/V/A remains actor-owned;
- GPU acceleration must not introduce synchronous waits/readbacks;
- static free space is cached/baked and locally invalidated;
- dynamic prediction is corridor/horizon filtered before expensive planning;
- Ruckig does not own obstacle topology;
- guidance/HUD consumes accepted products and never owns planning;
- navigation, exact collision and damage remain separate authorities;
- safety fidelity is not reduced to meet a timing budget; representation/algorithm/scheduling changes instead.


## Game-level maneuver decision tree

The navigation architecture now has an explicit owner above route/trajectory
geometry:

```text
global route
    -> local bounded visibility
    -> precision passage
    -> emergency/contact candidate generation
                    |
                    v
          ManeuverDecisionController
                    |
                    v
          selected control intent
```

Full contract: `src/game/MANEUVER_DECISION_TREE.md`.

Important ownership rule: `StaticHold`, `ConflictHold`, or a missing global
corridor describe failure to prove one class of safe progress. They are not
sufficient by themselves to choose the final maneuver. Future Stage-12
composition must expose fallback candidates to the game-level selector instead
of translating those planner states directly into a permanent braking command.
