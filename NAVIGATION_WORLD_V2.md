# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-TRAJECTORY-1` — moving / time-varying gap prediction candidate

Repository/branch authority is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. `main` is the only canonical game-development branch.

## 1. Decision summary

Navigation v2 uses one shared **NavigationWorld** for the active play area. It is not a complete independent navigation world/planner per NPC.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
shared ship-centered NavigationWorld
    + static NavigationSpace
    |   free-space / clearance / portals / cached corridors
    |
    + dynamic NavigationMap
    |   mass P/V/A prediction / swept bounds / bins
    |   compact relevant/conflict candidates
    |
    + LocalHorizon / LocalAvoidance
            |
            v
    bounded precision layer
        BoundedGapCandidateBuilder <= 8
        oriented hull fit
        attitude reachability
        static continuous passage
        emergency mitigation / impact-severity ranking
        moving-gap prediction for selected pairs only
            |
            v
    flight control / Ruckig / physics
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    deterministic local/precision reference work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

No backend-specific actor tables, cells, graph nodes or GPU buffers cross public navigation boundaries.

## 2. Different work runs at different rates

The system must not recompute a complete route for every ship every simulation tick.

```text
physics / authoritative actor state
    every simulation tick

dynamic NavigationWorld broadphase / prediction
    every tick or near that rate, shared across actors

local horizon / avoidance
    receding-horizon and may be staggered across NPCs

precision passage / moving-gap / emergency work
    only for compact conflict/narrow-passage candidates

global corridor search
    revision/event driven, cached and reused
```

A typical mass-NPC tick should therefore look like:

```text
all relevant actors
    -> one shared dynamic update
    -> compact per-agent candidate sets
    -> most agents remain on accepted corridor/local fast path
    -> only a small subset enters precision work
```

No `N ships x full global pathfinding x every tick` design is allowed.

## 3. Coordinate contract

Authoritative physical/system state and navigation working coordinates are separate.

The NavigationWorld origin may translate/rebase with the active ship/domain. Its axes are stable navigation/travel axes and do **not** roll/pitch/yaw with the hull. Hull-local coordinates belong to flight control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant subset is transformed/published into the active NavigationWorld.

## 4. `NAV-V2-MAP-2` — CLOSED / ACCEPTED

Production API:

```text
src/world/navigation/map/NavigationMap.h
```

Ingress is a dynamic snapshot with source revision, working frame, and actors carrying compact P/V/A/bounds/flags/revision. Egress is compact candidate data by value.

Accepted target-machine reference at 10k actors:

```text
CPU compact corridor/sphere queries <0.2 ms p95
GPU cruise total median 0.6840 ms, p95 1.3226 ms
GPU hub    total median 1.6097 ms, p95 1.6258 ms
```

No synchronous frame-thread GPU dispatch/wait/bulk-readback path is allowed. Dynamic results are asynchronous/double- or triple-buffered; result age contributes to physical safety margin.

Current mass actor geometry remains intentionally conservative. Precision oriented-hull work is downstream and conditional.

## 5. `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Production API:

```text
src/world/navigation/space/NavigationSpace.h
```

Accepted capabilities:

- sparse free-space regions + explicit portals;
- envelope-dependent clearance;
- traversable apertures/tunnels/canyons;
- deterministic topology and costed corridors;
- local fail-closed invalidation + transactional patch;
- static turn-cost semantics preserving arrival direction;
- private dense/BVH acceleration behind the public API.

Positive-turn search state:

```text
(RegionSlot, incoming PortalId)
```

Final target-machine evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

The turn-aware worker-path gate was `<=40 ms p95`. Static corridor search is closed. Vehicle velocity, braking, traffic or pursuit state must not leak into persistent static cost.

## 6. `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

### Local horizon

Physical horizon principle:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Accepted compact-candidate p95:

```text
clear_64          1.8327 us
clear_256         7.9820 us
clear_1024       36.9641 us
conflict_64       1.9333 us
conflict_256      7.5441 us
conflict_1024    31.8656 us
stale_1024        0.0359 us, 0 candidates examined
```

This one-pass reference scales linearly and is not an optimization target without contrary runtime evidence.

### Same-region avoidance

Accepted deterministic fan:

```text
15 degree deflection x 8 azimuths
30 degree deflection x 8 azimuths
maximum 16 probes
```

Each candidate requires same-publication static proof and dynamic re-evaluation. A changed target is not allowed to pretend that already predicted head-on current-kinematics conflict disappeared.

Multiplied-probe stress:

```text
all_dynamic_rejected_64 p95      35.8900 us
all_dynamic_rejected_256 p95    128.9708 us
all_dynamic_rejected_1024 p95   519.9286 us
```

The `1024 x 17` case is deliberate stress and remains inside the `<1.0 ms normal peak` CPU budget. Keep the fan unchanged.

`ConflictHold` means only that this cheap local layer cannot prove a safe temporary target. It does not disable navigation.

## 7. `NAV-V2-TRAJECTORY-1` — ACTIVE

Detailed authorities:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
src/world/navigation/MOVING_GAP_MODEL.md
```

### 7.1 Oriented passage geometry — ACCEPTED

`OrientedPassageEvaluator` performs O(1) precision OBB fit against an oriented passage. A conservative sphere may reject a flat slot that an appropriately rolled hull can traverse; the precision layer is allowed to recover that valid route.

Passage sources:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

### 7.2 Bounded emergent gaps — ACCEPTED

`BoundedGapCandidateBuilder` receives one known primary conflict and only already-reduced neighbors. It never performs global all-pairs or neighbor-neighbor discovery.

Hard output cap:

```text
<= 8 gap candidates
```

Target-machine stress:

```text
reject_1024 p95   11.6730 us
top8_1024 p95     24.0699 us
```

Do not micro-optimize this accepted stage without contrary evidence.

### 7.3 Attitude reachability — ACCEPTED

`AttitudeReachabilityEvaluator` determines whether the required entry attitude is reachable before the passage.

Pinned reference:

```text
90 degree roll
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
rest-to-rest time        = 2 s
closing speed            = 10 m/s

30 m -> ReachableCoast
15 m -> ReachableWithBraking at 5 m/s^2
 8 m -> UnreachableBeforeEntry
```

`UnreachableBeforeEntry` is not a no-command state.

### 7.4 Emergency passage mitigation — ACCEPTED

Project invariant:

```text
no collision-free proof != no navigation command
```

Emergency priority:

```text
1. collision-free maneuver if reachable
2. stop before contact if reachable
3. otherwise least-severity active contact mitigation
```

`EmergencyMitigatedContact` is explicitly non-safe. Physics/collision owns exact CCD/TOI/impulse/ricochet; damage owns consequences; navigation replans from actual post-impact truth.

### 7.5 Continuous static passage — BEHAVIOR + PERFORMANCE ACCEPTED

`ContinuousPassageTrajectoryEvaluator` checks one complete static/extruded passage maneuver:

```text
translation: cubic Hermite P/V -> P/V
attitude: shortest-arc smoothstep
33 pose samples
32 conservative continuous interval proofs
```

Point samples alone are not accepted. Each interval bounds center-curve deviation and oriented-hull rotational sweep. Vehicle authority is checked for body-axis forward/reverse/lateral/vertical acceleration plus angular speed/acceleration.

Control semantics remain separate:

```text
Newtonian
    velocity and hull attitude may diverge

EliteAssisted
    same physical thrust authority
    plus controller-policy slip-angle bound
```

Target-machine behavior on `574a2e98fd7a75ebf562bbfa476fba367fb888d1`:

```text
5/5 navigation_trajectory CTest PASS
```

Target-machine performance on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
full_precision_batch8 median 36.7902 us
full_precision_batch8 p95    41.3527 us = 0.04135 ms
```

The gate was `<0.5 ms typical`; the static verifier is frozen.

### 7.6 Emergency contact severity — ACCEPTED

`EmergencyContactSeverityScorer` ranks already-predicted contact witnesses, hard-bounded to:

```text
<= 8 candidates
<= 4 witnesses per candidate
```

Contact-point relative velocity:

```text
r = contactPoint - shipCenter
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Priority:

```text
no contact
-> lower peak normal closing speed
-> lower normal energy proxy
-> lower normal momentum proxy
-> more tangential incidence
-> geometry/progress tiebreakers
```

Target-machine gate on `7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e`:

```text
NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS
6/6 navigation_trajectory CTest PASS
100% tests passed, 0 failed
Total Test time: 0.27 sec
```

The scorer is accepted. Exact collision response remains physics authority.

### 7.7 Moving / time-varying gap prediction — ACTIVE CANDIDATE

Implementation:

```text
src/world/navigation/trajectory/MovingGapPredictor.h/.cpp
```

This stage receives **one already-selected pair**. It does not rediscover pairs or scan the world.

Boundary input:

```text
P / V / A
angular velocity
conservative radius
snapshot revision
```

Fixed work:

```text
33 samples
32 continuous intervals
```

Per sample it publishes:

```text
gap center + gap-center velocity
separation axis + separation rate
clear separation
both physical boundary surface points
normals toward free space
surface material velocities including omega x r
```

#### Continuous width proof

Relative center motion under constant acceleration is quadratic. For each interval the deviation from the endpoint relative-position chord is bounded by:

```text
|a_rel| * dt^2 / 8
```

The exact minimum origin-to-chord distance minus this deviation yields a conservative center-separation lower bound. After subtracting inflated radii, a hidden close-and-reopen event between samples is rejected as:

```text
GapClosesDuringHorizon
```

#### Continuous transverse alignment

A side-by-side gap must remain transverse to travel. `dot(r(t), travel)` is quadratic and its maximum absolute value over an interval is evaluated from endpoints plus any interior stationary point. Dividing by the continuous center-distance lower bound yields a conservative bound on:

```text
abs(separationAxis dot travel)
```

Exceeding policy returns:

```text
AlignmentLost
```

`OpenForHorizon` proves only that this spherical constraining pair remains open/transverse. It does **not** yet prove that the oriented controlled ship can continuously traverse the moving passage.

## 8. Next moving-passage composition

After `MovingGapPredictor` acceptance, the next trajectory slice combines:

```text
ship continuous P/V/attitude/hull sweep
        +
time-varying moving-gap samples/bounds
        +
vehicle linear/angular authority
```

to prove a complete moving passage.

The same relative-frame machinery then feeds moving/rotating docking.

## 9. Docking contract

Docking is terminal relative 6DoF pose/motion matching, not center-point arrival.

A port exposes:

```text
position / orientation
mating normal
roll/up reference
linear velocity / angular velocity
optional acceleration
capture tolerances
```

Final capture requires bounded:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
```

For a rotating port offset `r`:

```text
v_port = v_origin + omega x r
```

`bottom of ship -> bottom of dock` is explicit mating metadata. A 180-degree upside-down center arrival is invalid.

Normal docking aborts/goes around when possible. Generic emergency collision behavior is not successful docking.

## 10. NPC pilot skill

Vehicle capability and pilot skill remain separate. A deterministic `PilotSkillProfile` may affect reaction delay, decision rate, command latency, smoothing, damping/overshoot, anticipation and deterministic execution error without corrupting physical truth.

A poor pilot may genuinely consume safety margin and collide. Other actors react to its actual published state, not its ideal intended trajectory.

## 11. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/precision global solve      asynchronous only
```

These are design budgets, not portable CI timing assertions. Numerical acceptance comes from the target machine.

The ordinary fast path must remain unaware of expensive precision work unless needed. No frame-path unbounded `N x N` precision search is allowed.

## 12. Navigation / collision / damage boundary

```text
Navigation / trajectory
    intent + collision-free proof when possible
    explicit emergency contact expectation when not

Physics / Collision
    exact narrow phase / CCD / TOI / contacts / impulse / ricochet

Damage / Structural
    semantic hit ownership -> damage / detach / breach / destruction
    -> navigation publication changes
```

Emergency contact must never bypass physics or fabricate post-impact state.

## 13. Guidance/debug contract

Manual guidance visualizes the accepted corridor/trajectory/control intent actually used by navigation. It must not run a separate planner.

Ordinary `F12` keeps Hub/local presentation. `Shift+F12` toggles Hub render <-> raw NavigationWorld Debug. Debug consumes the same completed snapshot used by navigation/control and must not force synchronous readback/replanning.

Useful debug data eventually includes static regions/portals, dynamic predictions, conflict candidates, gap candidates, moving gap samples, selected hull attitude, continuous trajectory clearance, emergency contact severity, docking frame and pilot/controller execution state.

## 14. Roadmap

1. **`NAV-V2-MAP-2` — CLOSED.**
2. **`NAV-V2-SPACE-1` — CLOSED.**
3. **`NAV-V2-LOCAL-1` — CLOSED.**
4. **`NAV-V2-TRAJECTORY-1` — ACTIVE.** Static oriented passage, bounded gaps, attitude reachability, emergency mitigation, static continuous passage and emergency contact severity are accepted.
5. accept `MovingGapPredictor` P/V/A + between-sample proof.
6. moving continuous oriented-hull passage.
7. moving/rotating terminal docking + explicit mating frame.
8. deterministic NPC `PilotSkillProfile` execution.
9. pursuit/receding-intercept consumer.
10. raw NavigationWorld debug visualization.
11. live `EliteGame` / `EliteServer` / guidance integration.
12. end-to-end stress/performance acceptance.
13. retire legacy route-wide navigation only after v2 owns the stable live path.

## 15. Documentation Definition of Done

For meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant model README / benchmark RUN_LOG / contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG/DECISIONS
```

Stale state/task documentation or branch ambiguity blocks handoff.
