# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-TRAJECTORY-1` — continuous static passage accepted; emergency contact severity candidate pending target-machine gate

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
    + LocalHorizon / LocalAvoidance
            |
            v
    bounded precision passage layer
        <= 8 emergent gap candidates
        oriented hull fit
        attitude reachability
            |
            v
    continuous vehicle trajectory feasibility
        analytic translation + attitude
        continuous swept oriented-hull proof
        body-axis physical authority
        Elite/Newton control semantics
            |
            +-- collision-free candidate -> downstream control
            |
            +-- safe motion too late
                    |
                    v
              emergency mitigation
                    |
                    +-- stop if possible
                    |
                    +-- otherwise bounded contact-severity ranking
            |
            v
    RuckigTrajectorySolver / flight control / physics
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    deterministic local/precision work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

No backend-specific actor tables, graph nodes or GPU buffers cross public navigation boundaries.

## 2. Coordinate contract

Authoritative physical/system state and navigation working coordinates are separate.

The NavigationWorld origin may translate/rebase with the active ship/domain. Its axes are stable navigation/travel axes and do **not** roll/pitch/yaw with the hull. Hull-local coordinates belong to flight control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant subset is transformed/published into the active ship-centered NavigationWorld.

## 3. `NAV-V2-MAP-2` — CLOSED / ACCEPTED

Production API:

```text
src/world/navigation/map/NavigationMap.h
```

Ingress is a whole dynamic snapshot with source revision, `WorkingFrame`, and actors carrying P/V/A/radius/flags/revision. Egress is compact corridor/sphere candidate data by value.

Accepted target-machine reference at 10k actors:

```text
CPU compact corridor/sphere queries <0.2 ms p95
GPU cruise total median 0.6840 ms, p95 1.3226 ms
GPU hub    total median 1.6097 ms, p95 1.6258 ms
```

No synchronous frame-thread GPU dispatch/wait/bulk-readback path is allowed. Dynamic results are asynchronous/double- or triple-buffered; result age contributes to physical safety margin.

Current dynamic actor geometry intentionally remains center + radius + conservative swept sphere. It is broadphase/mass-NPC fidelity, not final oriented-hull maneuver fidelity.

## 4. `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

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

Positive turn state:

```text
(RegionSlot, incoming PortalId)
```

Final target-machine turn evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

The gate was `<=40 ms p95`. Static turn search is closed. Vehicle velocity, braking, traffic or pursuit state must not leak into persistent static cost.

## 5. `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

### 5.1 Local horizon reference

Physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Accepted compact-candidate scaling on `4b94048...`:

```text
clear_1024 p95     36.9641 us
conflict_1024 p95  31.8656 us
stale_1024 p95      0.0359 us, 0 candidates examined
```

### 5.2 Same-region adjusted-target avoidance

`LocalAvoidancePlanner` acts only after nominal `ConflictHold`.

Candidate fan:

```text
15 degree deflection x 8 azimuth samples
30 degree deflection x 8 azimuth samples
maximum 16 probes
```

Each candidate requires same-publication static proof from `NavigationSpace` and dynamic re-evaluation through the accepted `LocalHorizonPlanner`.

Fresh multiplied-probe target-machine evidence on `e0817d...`:

```text
nominal_clear_64 p95               1.8333 us
all_dynamic_rejected_64 p95       35.8900 us
all_dynamic_rejected_256 p95     128.9708 us
all_dynamic_rejected_1024 p95    519.9286 us
```

Design budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

The `1024 x 17` case is deliberate stress and remains inside the normal-peak budget. Keep the deterministic 16-probe fan unchanged.

Authority:

```text
benchmarks/navigation_local_avoidance/RUN_LOG.md
```

### 5.3 Meaning of `ConflictHold`

`ConflictHold` means the accepted local safe-target layer cannot prove a collision-free temporary target under its own model.

It does **not** mean:

```text
collision is certainly unavoidable
navigation must stop producing commands
```

A trajectory precision layer may prove a different maneuver class or, if safe motion is physically too late, produce an explicit least-severity emergency command.

## 6. `NAV-V2-TRAJECTORY-1` — ACTIVE

Detailed authorities:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
```

### 6.1 Oriented passage geometry — ACCEPTED

Implementation:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h/.cpp
```

Passage sources:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

Precision fit uses body-local OBB projection against the oriented passage plane. A broadphase sphere may reject a route that an anisotropic hull can safely traverse after roll/pitch/yaw; precision pose proof is therefore separate from broadphase.

### 6.2 Emergent gaps — ACCEPTED

Implementation:

```text
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h/.cpp
```

The builder accepts one already-known primary conflict against already-reduced local neighbors and returns at most eight deterministic `ObstacleGap` candidates.

Explicitly rejected:

```text
global all-pairs scan
neighbor-neighbor discovery
mixed snapshot evidence
overlapping bounds inventing a gap
front/back pairs masquerading as transverse slots
```

Target-machine stress:

```text
reject_1024 p95   11.6730 us
top8_1024 p95     24.0699 us
```

Authority:

```text
benchmarks/navigation_trajectory_gap/RUN_LOG.md
```

### 6.3 Attitude reachability — ACCEPTED

Implementation:

```text
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h/.cpp
```

Result classes:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Pinned 90-degree reference:

```text
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
rest-to-rest time        = 2 s
closing speed            = 10 m/s

30 m -> ReachableCoast
15 m -> ReachableWithBraking at 5 m/s^2
 8 m -> UnreachableBeforeEntry
```

`UnreachableBeforeEntry` is not a no-command state.

### 6.4 Emergency passage mitigation — ACCEPTED

Implementation:

```text
src/world/navigation/trajectory/EmergencyPassageMitigator.h/.cpp
```

Architecture rule:

```text
no collision-free proof != no navigation command
```

Emergency priority:

```text
1. collision-free maneuver if physically reachable
2. stop before contact if physically reachable
3. otherwise least-severity contact mitigation with active control intent
```

`EmergencyMitigatedContact` is valid navigation/control intent but is **never** `Clear`. Physics/collision owns exact contact response and damage owns consequences.

Target-machine acceptance on `b29a03d...`:

```text
NAVIGATION TRAJECTORY BOUNDED GAP CONTRACT: PASS
NAVIGATION TRAJECTORY EMERGENCY PASSAGE CONTRACT: PASS
4/4 trajectory CTest PASS
```

### 6.5 Continuous static-passage verifier — BEHAVIOR + PERFORMANCE ACCEPTED

Implementation:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h/.cpp
```

Authority:

```text
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
benchmarks/navigation_trajectory_continuous/RUN_LOG.md
```

The verifier checks one complete bounded static/extruded passage maneuver:

```text
translation = cubic Hermite start P/V -> end P/V
attitude    = shortest orientation arc, smooth rest-to-rest timing
33 pose samples
32 continuous interval proofs
```

Point samples alone are not collision proof. Every interval bounds center-curve deviation and oriented-hull rotational sweep.

Physical gates include forward/reverse/lateral/vertical acceleration and angular speed/acceleration. `Newtonian` allows velocity/attitude divergence; `EliteAssisted` adds controller-policy slip limits without inventing thrust.

Behavior acceptance on `574a2e98...`:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS
5/5 navigation_trajectory CTest PASS
```

Performance acceptance on `6f854362...`:

```text
straight_newton p95                4.0458 us
rolled_newton p95                  6.5235 us
lateral_newton p95                 4.1593 us
elite_aligned p95                  4.1820 us
geometry_blocked_roll p95          6.5133 us
full_precision_batch8 p95         41.3527 us = 0.04135 ms
```

The full eight-candidate precision batch uses about 8.3% of the `<0.5 ms typical` CPU budget. Decision: freeze this verifier unless live evidence later contradicts the isolated result.

### 6.6 Emergency contact severity scorer — ACTIVE CANDIDATE

Implementation:

```text
src/world/navigation/trajectory/EmergencyContactSeverityScorer.h/.cpp
```

Authority:

```text
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
```

This scorer does **not** discover collisions. It consumes already-predicted bounded contact witnesses and ranks at most:

```text
8 emergency trajectory candidates
4 contact witnesses per candidate
```

Rigid-body contact-point kinematics:

```text
r = contactPoint - shipCenter
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Selection priority:

```text
1. no predicted contact beats contact
2. lower peak closing normal speed
3. lower total normal-impact energy proxy
4. lower total normal momentum proxy
5. lower incidence angle from tangent
6. lower geometry deficit
7. higher useful passage-axis progress
8. stable candidate id / input index
```

Impact severity outranks route progress. A glancing scrape/ricochet-friendly contact therefore beats a harder normal impact even if the harder candidate advances farther through the gap.

The energy/momentum terms are navigation ranking proxies. Exact reduced mass, material response, CCD/TOI/manifold/impulse/ricochet remain physics authority.

Pinned behavior includes glancing-vs-normal selection, `omega x r`, moving-surface relative velocity, effective-mass energy tie-break, no-contact preference, normal-incidence diagnostics and hard-bound fail-closed behavior.

Target-machine architecture/build/behavior gate is pending.

### 6.7 Performance invariant

The ordinary fast path must remain unaware of expensive precision work unless needed.

```text
cheap broadphase/local avoidance
        |
        +-- safe result -> done
        |
        +-- ConflictHold / explicit narrow route / docking
                |
                v
        bounded gap candidates <= 8
                |
                v
        O(1) oriented fit / reachability
                |
                v
        continuous trajectory proof for plausible candidates
                |
                +-- collision-free -> control
                |
                +-- no safe result
                        |
                        v
                fixed-size emergency mitigation
                        |
                        v
                contact severity ranking <= 8 x <= 4 witnesses
```

No frame-path `N x N` precision search is allowed.

## 7. Moving trajectory continuation / docking fidelity

The next trajectory slices consume authoritative vehicle capability rather than inventing duplicate physics state:

```text
position / velocity / acceleration
attitude / angular velocity
oriented hull proxy
body-axis linear thrust/braking authority
angular acceleration/rate authority
mass/inertia where needed
active flight-control mode
```

### `Elite` assisted behavior

May couple requested travel/hull attitude and suppress drift, but never exceeds declared physical authority.

### `Newton` behavior

Velocity and hull attitude are independent. Safe braking or re-vectoring may require coast-while-rotating, rotate-then-thrust or flip-and-burn. Emergency hull orientation and contact-point velocity are therefore evaluated independently.

### Moving/time-varying gaps

The next precision step is not another global pathfinder. It generates bounded time-varying passage/contact witnesses from the already-reduced moving obstacle set and feeds the accepted trajectory/emergency layers.

Moving surface velocity is already present in the emergency severity witness contract so the same relative-contact metric extends naturally to moving gaps.

### Docking

Docking is terminal 6DoF pose/motion matching against a stationary/moving/rotating frame:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
mating-frame normal + roll/up convention
```

For a rotating port offset `r`:

```text
v_port = v_origin + omega x r
```

`bottom of ship -> bottom of dock` is explicit mating metadata. A 180-degree rolled/upside-down center-point arrival is invalid.

Normal docking aborts/goes around while safe capture remains possible. Emergency impact mitigation applies only when contact has genuinely become unavoidable.

### NPC pilot skill

Vehicle capability and pilot skill remain separate. `PilotSkillProfile` may model reaction delay, decision rate, control smoothing, damping/overshoot, anticipation and deterministic precision error. Poor execution may consume safety margin and genuinely collide; geometry/physics stay truthful.

## 8. Scheduling / all-ships runtime contract

NavigationWorld data is shared across all relevant ships. The architecture is explicitly **not**:

```text
N ships x full global route solve every simulation tick
```

Instead:

```text
shared dynamic world update
    -> compact per-ship local candidate sets
    -> cheap local horizon for relevant ships
    -> precision work only for conflicts/narrow routes/docking
    -> global corridor recomputation only on invalidation/goal/topology events
```

Different layers may run at different rates. Global corridor validity is revision/event driven; local avoidance is receding-horizon; precision passage/emergency work is bounded and conditional; mass-NPC work may be staggered.

## 9. Legacy navigation status

The previous route-wide chain remains migration/reference code:

```text
GeometricPathPlanner
 -> route-wide TrajectoryGenerator / RuckigRoutePlanner
 -> dense route sampling
 -> route-wide obstacle validation
 -> GuidanceTunnel
```

Existing `TacticalCollisionMonitor` and `SmallCraftNavigation` may inform tests/reference math but do not own v2 state.

Ruckig remains downstream after navigation/trajectory feasibility has selected an accepted maneuver state. It is not by itself authority that an attitude-coupled ship can generate the requested world-space thrust vector in time.

## 10. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/precision global solve      asynchronous only
```

These are design budgets, not portable assertions. Numerical acceptance comes from target-machine output.

## 11. Navigation / collision / damage boundary

```text
Navigation / trajectory
    intent + collision-free proof when possible
    bounded predicted contact witnesses when not
    least-severity emergency ranking

Physics / Collision
    exact narrow phase / CCD / TOI / contacts / manifold / impulse / ricochet

Damage / Structural
    semantic hit ownership -> damage / detach / breach / destruction
    -> navigation static/dynamic publication changes
```

A detached fragment becomes a dynamic `NavigationMap` actor. A topology-changing breach changes `NavigationSpace` only when the opening has sufficient clearance for the requesting envelope.

Emergency contact must never bypass physics or fabricate post-impact state.

## 12. Guidance/debug contract

Manual guidance visualizes the accepted corridor/trajectory/control intent actually used by navigation. It must not run a separate planner.

Ordinary `F12` keeps Hub/local presentation. `Shift+F12` toggles Hub render <-> raw NavigationWorld Debug. Debug consumes the same completed snapshot used by navigation/control and must not force synchronous readback/replanning.

Useful debug data includes static regions/portals/clearance, dynamic P/V/A/swept bounds, local horizon/conflict set, adjusted target, gap candidates, chosen hull attitude, attitude reachability, continuous trajectory status/clearance/authority requirement, emergency status/contact expectation, predicted contact normal speed/energy proxy, braking/aim/travel intent, and snapshot generation/age.

Later debug also exposes moving-gap state, moving docking frame and pilot/controller execution state.

## 13. Roadmap

1. **`NAV-V2-MAP-2` — CLOSED.**
2. **`NAV-V2-SPACE-1` — CLOSED.**
3. **`NAV-V2-LOCAL-1` — CLOSED.**
4. **`NAV-V2-TRAJECTORY-1` — ACTIVE.** Oriented passage, bounded gaps, attitude reachability, emergency mitigation and continuous static passage are accepted; emergency contact severity candidate is pending target-machine gate.
5. accept/freeze bounded emergency contact severity ranking;
6. moving/time-varying obstacle gap witness generation;
7. moving/rotating terminal docking;
8. deterministic NPC `PilotSkillProfile` execution;
9. pursuit/receding-intercept consumer;
10. raw NavigationWorld debug visualization;
11. live `EliteGame` / `EliteServer` / guidance integration;
12. end-to-end stress acceptance;
13. retire legacy route-wide navigation only after v2 owns the live path.

## 14. Definition of final success

Navigation v2 is finished only when the **live runtime**, not merely isolated fixtures, repeatedly demonstrates:

```text
ordinary flight inside CPU/GPU budgets
static + dynamic hazard avoidance
narrow/oriented gap traversal when physically possible
truthful Elite/Newton reachability
least-severity active control through unavoidable collision cases
replan from actual post-impact / post-detach physics state
stationary + moving + rotating docking to the correct mating pose
NPC-scale operation without planner stalls or unbounded precision work
guidance/debug driven from the same accepted navigation/control state
```

At that point the legacy route-wide navigation may be removed from the live path.

## 15. Documentation Definition of Done

For meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant README / benchmark RUN_LOG / contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG/DECISIONS
```

Stale state/task documentation or branch ambiguity blocks handoff.
