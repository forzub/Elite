# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-LOCAL-1` — local horizon and same-region avoidance behavior accepted; avoidance fan performance gate active; oriented-passage / bounded-gap / attitude-reachability candidates pending target-machine gates

Repository/branch authority is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. `main` is the only canonical game-development branch.

## 1. Decision summary

Navigation v2 uses one shared **NavigationWorld** for the active play area. It is not a complete independent navigation world/planner per NPC.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
ship-centered NavigationWorld
    + static NavigationSpace
    |   free-space / clearance / portals / corridor
    |
    + dynamic NavigationMap
    |   P/V/A / prediction / swept bounds / bins
    |   compact relevant/conflict candidates
    |
    + local horizon / avoidance consumer
        bounded conflict assessment
        statically proven temporary target state
            |
            v
    bounded precision-passage fallback
        one primary conflict + reduced local neighbors
        <= 8 gap candidates
        oriented hull fit
        attitude reachability before entry
            |
            v
    continuous 6DoF maneuver feasibility
            |
            v
    RuckigTrajectorySolver / flight control
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    deterministic precision/local reference work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

The public boundaries remain backend-neutral. No backend-specific actor tables, cells, graph nodes or GPU buffers cross them.

## 2. Coordinate contract

Authoritative physical/system state and navigation working space are separate.

The NavigationWorld origin may translate/rebase with the active ship/domain. Its axes are stable navigation/travel axes and do **not** roll/pitch/yaw with the hull. Hull-local coordinates belong to flight control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant static/dynamic subset is transformed and published into the active ship-centered NavigationWorld.

## 3. Accepted dynamic boundary — `NAV-V2-MAP-2` CLOSED

Production API:

```text
src/world/navigation/map/NavigationMap.h
```

Ingress is a whole dynamic snapshot with source revision, `WorkingFrame`, and actors carrying P/V/A/radius/flags/revision. Egress is compact `queryCorridor()` / `querySphere()` candidate data by value.

`NavigationMap::Candidate` contains map-space P/V/A, predicted endpoint, conservative swept sphere, radius, flags and motion revision. Internal cells and backend state remain private.

Accepted target-machine evidence at 10k actors:

```text
CPU compact corridor/sphere queries <0.2 ms p95
GPU cruise total median 0.6840 ms, p95 1.3226 ms
GPU hub    total median 1.6097 ms, p95 1.6258 ms
```

No synchronous frame-thread dispatch/wait/bulk-readback path is allowed. Dynamic results are asynchronous/double- or triple-buffered; result age contributes to physical safety margin.

Current dynamic actor geometry is intentionally conservative: radius + swept sphere. That is a broadphase/mass-NPC representation, not a claim of final oriented-hull maneuver fidelity. Precision trajectory geometry is specified in `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md` and `src/world/navigation/ORIENTED_PASSAGE_MODEL.md`.

## 4. Accepted static boundary — `NAV-V2-SPACE-1` CLOSED

Production API:

```text
src/world/navigation/space/NavigationSpace.h
```

Accepted capabilities:

- sparse free-space regions + explicit portals;
- agent-envelope clearance;
- traversable apertures/tunnels/canyons;
- deterministic topology and costed corridors;
- local fail-closed invalidation + transactional patch;
- static turn-cost semantics preserving arrival direction;
- private dense/BVH acceleration behind the same public API.

Accepted 10k reference evidence includes:

```text
coarse BFS corridor             ≈7.9-8.0 ms
automatic point p95             <=0.0353 ms
bounded invalidation p95        <=0.0115 ms
costed corridor v1 p95          <=9.6103 ms
```

Positive static turn penalty uses semantic state:

```text
(RegionSlot, incoming PortalId)
```

Final target-machine turn acceptance on commit `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

The pinned gate was `<=40 ms p95`, so `NAV-V2-SPACE-1` is closed. No ship velocity, braking, traffic or pursuit state belongs in persistent static cost.

## 5. Current stage — `NAV-V2-LOCAL-1`

The local layer composes accepted static intent with accepted dynamic reduction.

```text
cached static corridor / nominal local target
        +
NavigationMap compact candidates
        +
agent P/V/A + envelope
        +
completed-result age / latency budget
        |
        v
LocalHorizonPlanner
        |
        +-- Clear / bounded target
        +-- StaleHold
        +-- ConflictHold
                 |
                 v
         LocalAvoidancePlanner
                 |
                 +-- statically proven adjusted target
                 +-- fail-closed hold
        |
        v
bounded precision-passage fallback only when required
        |
        v
continuous 6DoF maneuver feasibility
        |
        v
RuckigTrajectorySolver / flight control
```

The local layer does **not** own another NavigationWorld, does not scan the full scene and does not expand the global route into thousands of samples. Per-agent work starts after shared reduction has produced compact candidates.

### 5.1 Accepted local horizon reference

Baseline physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

`T_latency` includes asynchronous snapshot/result age. The accepted deterministic reference uses bounded relative-motion closest approach plus conservative swept-sphere/segment conflict checks.

Behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39` passed.

Target-machine compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
scenario         p95_us     p95_ns/candidate
clear_16          0.4814          30.0873
clear_64          1.8327          28.6362
clear_256         7.9820          31.1798
clear_1024       36.9641          36.0977

conflict_16       0.5073          31.7062
conflict_64       1.9333          30.2078
conflict_256      7.5441          29.4693
conflict_1024    31.8656          31.1188

stale_1024        0.0359          0 candidates examined
```

This loop is accepted and is not a performance bottleneck. Do not optimize it further without new runtime evidence.

### 5.2 Same-region adjusted-target behavior — ACCEPTED

`LocalAvoidancePlanner` may act only after the accepted nominal evaluation reports `ConflictHold`.

Candidate fan:

```text
15 degree deflection x 8 azimuth samples
30 degree deflection x 8 azimuth samples
maximum 16 probes
```

Each target must pass two proofs:

1. **Static proof:** the current agent point and candidate target are traversable for the same envelope, resolve to the same `NavigationSpace` region, and come from the same static space/source revision.
2. **Dynamic proof:** the candidate is re-evaluated against the same compact `NavigationMap::QueryResult` through `LocalHorizonPlanner`.

Fresh target-machine behavior acceptance on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

The same-region rule remains deliberately conservative and may reject valid portal-crossing maneuvers.

### 5.3 Current-kinematics limitation

The accepted closest-approach reference evaluates the ship's current P/V/A. A changed target alone cannot erase an already predicted head-on/crossing collision. Current-kinematics collision cases remain `ConflictHold` until a trajectory-aware maneuver is demonstrated.

This is a safety contract, not an algorithmic limitation to hide with optimistic prediction.

### 5.4 Active performance gate — bounded fan cost

Behavior is accepted; multiplied-probe performance is measured separately in:

```text
benchmarks/navigation_local_avoidance/
```

Pinned cases:

```text
nominal_clear_64
    0 probes / 1 horizon evaluation

early_adjust_64
    first probe accepted

all_static_rejected_64
    16 static rejections

all_dynamic_rejected_16/64/256/1024
    16 statically valid probes
    16 failed dynamic rechecks
    17 horizon evaluations
```

`1024` is deliberate stress. No fan-performance acceptance is claimed until target-machine median/p95 output is captured.

### 5.5 Fail-closed ownership

If a safe local target cannot be demonstrated, the local layer reports a fail-closed result. It must not invent free space from render geometry or bypass `NavigationSpace`/`NavigationMap` authority.

However, `ConflictHold` does not semantically mean that collision is always unavoidable. A later precision layer may prove a different maneuver class, such as passing through an oriented gap between relevant obstacles.

Pursuit remains a later consumer.

### 5.6 Oriented passage / emergent gap / attitude timing — PENDING TARGET-MACHINE GATES

Detailed contract:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
```

Prepared isolated components:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h/.cpp
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h/.cpp
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h/.cpp
```

A passage can originate as:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

The key semantic addition is that two nearby obstacles can form **positive free space between them**. If ordinary lateral avoidance fails because remaining time/distance is insufficient, a bounded precision fallback may evaluate the local gap as an oriented passage instead of treating the obstacles only as two separate repulsive constraints.

#### Bounded gap extraction

The gap builder consumes one already-known primary conflict and already-reduced local neighbors. It never runs global or neighbor-neighbor all-pairs discovery.

Required complexity/ownership:

```text
O(local_neighbors * 8)
hard output cap = 8
same snapshot revision
backend-neutral compact witnesses
```

It rejects overlapping conservative bounds, front/back pairs masquerading as transverse slots, and candidates outside the configured forward/centerline window.

#### Oriented hull fit

`OrientedPassageEvaluator` performs constant-size OBB projection math and reports whether the real hull fits the cross-section at the supplied attitude/offset.

This specifically recovers cases such as:

```text
flat ship + flat slot
sphere broadphase -> conservative reject
correctly oriented OBB -> Fits
```

#### Attitude reachability before entry

`AttitudeReachabilityEvaluator` checks whether the required passage attitude can be reached before crossing the entry plane using declared angular acceleration/rate authority and available longitudinal braking.

Result classes:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Pinned reference semantics include a 90 degree roll that takes 2 s at 90 deg/s^2 and 90 deg/s limits: 30 m at 10 m/s is coast-reachable, 15 m is reachable with 5 m/s^2 braking, and 8 m is fail-closed as unreachable before entry.

Existing angular velocity is conservatively settled first and consumes additional margin.

#### Performance invariant

Precision passage logic must not turn the accepted fast path into an all-pairs geometry system.

```text
cheap broadphase/local avoidance
        |
        +-- normal safe result -> done
        |
        +-- ConflictHold / explicit narrow aperture / docking
                |
                v
        bounded gap candidates <= 8
                |
                v
        O(1) oriented fit
                |
                v
        O(1) attitude reachability
                |
                v
        full continuous 6DoF sweep only for survivors
```

Dedicated builder benchmark:

```text
benchmarks/navigation_trajectory_gap/
```

It measures reject/top-8 paths at 16/64/256/1024 local neighbors. `1024` is deliberate stress.

These components remain **candidate-only** until their architecture/build/behavior/performance gates run on the target machine.

They still do **not** prove body-axis translational reachability, continuous swept-body clearance, moving-gap persistence or final docking capture.

### 5.7 Planned vehicle/control/docking fidelity

Detailed contract: `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`.

The trajectory-aware layer must keep these responsibilities separate:

```text
world truth          static/dynamic geometry and actual actor state
vehicle capability   hull proxy, attitude/angular state, thrust/rotation authority
navigation intent    corridor / temporary target / passage choice
control mode         assisted Elite-style vs Newtonian free-flight behavior
docking contract     moving terminal frame / capture tolerances
pilot skill          reaction / update rate / smoothing / damping / precision
control execution    actual force/torque commands and physics
```

For precision feasibility, orientation over time matters. A long ship can clear a point with its center and still strike a wall with its tail while rotating.

Assisted `Elite` behavior may prefer smooth nose/velocity alignment. Newtonian behavior allows velocity and hull attitude to diverge; strong braking may require coast-while-rotating followed by rotate-then-thrust or flip-and-burn.

Docking is terminal 6DoF pose matching against a possibly moving/rotating frame, including relative position, linear velocity, attitude and angular velocity. `bottom of ship -> bottom of dock` is an explicit mating-frame rule; an upside-down center-point arrival is invalid.

NPC skill remains an execution model, not corrupted geometry.

## 6. Legacy navigation status

The previous chain is migration code:

```text
GeometricPathPlanner
 -> route-wide TrajectoryGenerator / RuckigRoutePlanner
 -> dense route sampling
 -> route-wide obstacle validation
 -> GuidanceTunnel
```

Existing `TacticalCollisionMonitor` and `SmallCraftNavigation` are also pre-v2 implementations. Their algorithms may inform tests, but the v2 public boundary stays backend-neutral.

Ruckig remains downstream after the navigation layer has selected an accepted target/maneuver. It is not by itself authority that an attitude-coupled ship can orient and generate the requested thrust vector in time.

## 7. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/precision global solve      asynchronous only
```

These are design budgets, not portable assertions. Numerical acceptance comes from the user's target-machine output.

Different layers may run at different rates. Global corridor validity is revision/event driven; local physical avoidance is receding-horizon; precision passage work is bounded/conditional; mass-NPC work may be staggered.

## 8. Navigation / collision / damage boundary

```text
Navigation
    conservative envelopes / free-space / predicted conflicts / precision passage intent

Physics / Collision
    broadphase candidates -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local static-space invalidation
```

A detached fragment becomes a dynamic NavigationMap actor. A topology-changing breach changes NavigationSpace only when the opening has sufficient clearance for the requesting envelope.

## 9. Guidance/debug contract

Manual guidance visualizes the accepted corridor/trajectory actually used by navigation/control. It must not run an unrelated planner.

Ordinary `F12` keeps Hub/local presentation. `Shift+F12` toggles Hub render <-> raw NavigationWorld Debug. Debug consumes the same completed NavigationWorld snapshot used by navigation/control and must not run a second planner or force synchronous readback.

Useful debug data includes static regions/portals/clearance, dynamic actors P/V/A, swept bounds, active corridor, local horizon, adjusted target, conflicts, passage candidates, selected passage attitude, reachability margin, snapshot generation and age. When continuous trajectory-aware control exists, debug should also expose selected maneuver, attitude path, reachable acceleration/braking envelope and pilot/controller execution state.

## 10. Roadmap

1. **`NAV-V2-MAP-2` — CLOSED:** shared dynamic reduction/backend evidence.
2. **`NAV-V2-SPACE-1` — CLOSED:** static free-space/corridor/turn-aware reference.
3. **`NAV-V2-LOCAL-1` — ACTIVE:** horizon + same-region avoidance behavior accepted; bounded-fan performance measurement active.
4. **Trajectory precision candidates — PENDING GATES:** oriented passage fit + bounded obstacle-gap extraction + attitude reachability before entry.
5. continuous static-gap 6DoF vehicle/control feasibility: body-axis translation + rotation + swept oriented hull + Elite/Newton semantics.
6. moving/time-varying obstacle gaps.
7. moving/rotating terminal docking + NPC execution skill.
8. pursuit/receding-intercept consumer.
9. raw NavigationWorld debug visualization.
10. live `EliteGame` / `EliteServer` integration.
11. retire legacy route-wide navigation only after v2 owns the live path.

## 11. Documentation Definition of Done

For meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant README / benchmark RUN_LOG / contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG
```

Stale state/task documentation or branch ambiguity blocks handoff.
