# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-TRAJECTORY-1` — oriented passage / emergent gap / emergency mitigation

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
    + accepted local horizon / avoidance
            |
            v
    bounded precision passage layer
        <= 8 emergent gap candidates
        oriented hull fit
        attitude reachability
        emergency impact mitigation when safe entry is too late
            |
            v
    continuous 6DoF maneuver feasibility
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
    deterministic precision/local reference work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

No backend-specific actor tables, cells, graph nodes or GPU buffers cross public navigation boundaries.

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

Current dynamic actor geometry remains intentionally conservative: center + radius + swept sphere. It is broadphase/mass-NPC fidelity, not final oriented-hull maneuver fidelity.

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

This one-pass loop is accepted and is not an optimization target without contrary runtime evidence.

### 5.2 Same-region adjusted-target behavior

`LocalAvoidancePlanner` acts only after nominal `ConflictHold`.

Candidate fan:

```text
15 degree deflection x 8 azimuth samples
30 degree deflection x 8 azimuth samples
maximum 16 probes
```

Each candidate requires:

1. static same-region/same-publication proof from `NavigationSpace`;
2. dynamic re-evaluation through the accepted `LocalHorizonPlanner`.

Behavior acceptance on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
```

### 5.3 Multiplied fan performance — ACCEPTED

Fresh target-machine benchmark on `e0817d157ba5d8c9c329576236310507bda13364`:

```text
scenario                     p95_us
nominal_clear_64               1.8333
early_adjust_64                4.2795
all_static_rejected_64         4.2655
all_dynamic_rejected_16       12.9500
all_dynamic_rejected_64       35.8900
all_dynamic_rejected_256     128.9708
all_dynamic_rejected_1024    519.9286
```

Design budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

`1024 x 17` is deliberate stress, not normal compact-neighbor load, and still fits the normal-peak budget. Decision: keep the deterministic 16-probe fan unchanged.

Authority:

```text
benchmarks/navigation_local_avoidance/RUN_LOG.md
```

### 5.4 Meaning of `ConflictHold`

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
```

### 6.1 Oriented passage geometry

Implementation:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h/.cpp
```

A passage can originate as:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

Precision fit uses body-local OBB projection against the oriented passage plane. This recovers valid cases where the conservative broadphase sphere intentionally rejects the route, e.g. a flat ship rolled into a flat slot.

### 6.2 Emergent gaps between nearby obstacles

Implementation:

```text
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h/.cpp
```

Project invariant: two nearby obstacles may create **positive free space between them**.

The builder accepts:

```text
one already-known primary conflict
x already-reduced local neighbors
```

and returns at most:

```text
8 deterministic ObstacleGap candidates
```

Explicitly rejected:

```text
global all-pairs scan
neighbor-neighbor pair discovery
mixed snapshot evidence
overlapping conservative bounds inventing a gap
front/back pairs masquerading as a transverse slot
```

Target-machine bounded-gap performance on `e0817d...`:

```text
scenario       p95_us
reject_16       0.2459
reject_64       0.7550
reject_256      4.6499
reject_1024    11.6730

top8_16         0.7883
top8_64         1.8260
top8_256        6.3609
top8_1024      24.0699
```

Even `top8_1024` stress is only `0.0241 ms p95`. Gap extraction is performance-accepted.

Authority:

```text
benchmarks/navigation_trajectory_gap/RUN_LOG.md
```

### 6.3 Attitude reachability before entry

Implementation:

```text
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h/.cpp
```

A passage may fit geometrically but the requested collision-free entry attitude may be too late.

Inputs include current/required attitude, current angular velocity, angular acceleration/rate authority, distance to entry, closing speed and longitudinal braking authority.

Result classes:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Pinned reference:

```text
90 deg roll
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
rest-to-rest time        = 2 s
closing speed            = 10 m/s

30 m -> ReachableCoast
15 m -> ReachableWithBraking at 5 m/s^2
 8 m -> UnreachableBeforeEntry
```

`UnreachableBeforeEntry` means only that the requested collision-free attitude is not proven reachable in time. It is not a no-command state.

### 6.4 Target-machine behavior evidence

On `e0817d...` the then-registered suite passed:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time: 0.12 sec
```

The one bounded-gap architecture failure was only a stale exact Markdown marker while C++ behavior passed. The current checker asserts stable invariants instead.

### 6.5 Emergency passage / contact mitigation — ACTIVE CANDIDATE

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
1. use a collision-free entry pose if physically reachable
2. stop before contact if physically possible
3. otherwise minimize impact severity and keep useful progress
```

Current bounded intent:

```text
maximum useful braking
aim at gap center
desired travel along passage axis
choose best physically reachable hull attitude
explicit expected-contact result when unavoidable
```

Result classes:

```text
SafeEntryPose
EmergencyStopBeforeEntry
EmergencyMitigatedContact
InvalidInput
```

`EmergencyMitigatedContact` is a valid navigation/control command, but **never** a safe/`Clear` result.

The first implementation samples exactly 17 physically reachable attitudes along the shortest arc toward the preferred passage pose and scores them by passage clearance / geometric deficit. Equal-severity poses prefer greater correction toward the passage attitude.

If stopping before the passage is physically possible, intentional contact is rejected in favor of `EmergencyStopBeforeEntry`.

If stopping and collision-free entry are both impossible, contact/ricochet is allowed as a least-severity outcome. Physics/collision owns exact CCD/TOI/contact impulse/ricochet; damage/structural owns consequences. The next navigation solve consumes the actual post-impact state.

Current emergency scoring is only geometric. Full 6DoF work must additionally minimize predicted **relative normal contact speed / impact-energy proxy**.

### 6.6 Performance invariant

The ordinary fast path must remain unaware of expensive precision work unless it is needed.

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
        O(1) oriented fit / timing
                |
                v
        fixed-size emergency attitude sampling only if necessary
                |
                v
        continuous 6DoF solve only for surviving/urgent candidates
```

No frame-path `N x N` precision search is allowed.

## 7. Planned continuous vehicle/control/docking fidelity

The next continuous layer consumes authoritative vehicle capability rather than inventing duplicate physics state:

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

May strongly couple requested travel/hull attitude and suppress drift, but never exceeds declared physical authority.

### `Newton` behavior

Velocity and hull attitude are independent. Safe braking or re-vectoring may require coast-while-rotating, rotate-then-thrust or flip-and-burn. Emergency geometry and velocity-normal impact severity must therefore be evaluated independently.

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

Normal docking should abort/go around when safe capture cannot be maintained. Generic emergency impact mitigation applies only when collision has actually become unavoidable, not as a substitute for correct docking capture.

### NPC pilot skill

Vehicle capability and pilot skill remain separate. `PilotSkillProfile` may model reaction delay, decision rate, control smoothing, damping/overshoot, anticipation and deterministic precision error. A poor pilot may consume the available safety margin and genuinely collide; geometry/physics stay truthful.

## 8. Legacy navigation status

The previous route-wide chain remains migration/reference code:

```text
GeometricPathPlanner
 -> route-wide TrajectoryGenerator / RuckigRoutePlanner
 -> dense route sampling
 -> route-wide obstacle validation
 -> GuidanceTunnel
```

Existing `TacticalCollisionMonitor` and `SmallCraftNavigation` may inform tests/reference math but do not own v2 state.

Ruckig remains downstream after navigation/trajectory feasibility has selected an accepted maneuver state. It is not by itself authority that an attitude-coupled vehicle can generate the requested world-space thrust vector in time.

## 9. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/precision global solve      asynchronous only
```

These are design budgets, not portable assertions. Numerical acceptance comes from target-machine output.

Different layers may run at different rates. Global corridor validity is revision/event driven; local avoidance is receding-horizon; precision passage/emergency work is bounded and conditional; mass-NPC work may be staggered.

## 10. Navigation / collision / damage boundary

```text
Navigation / trajectory
    intent + collision-free proof when possible
    explicit emergency contact expectation when not

Physics / Collision
    exact narrow phase / CCD / TOI / contacts / impulse / ricochet

Damage / Structural
    semantic hit ownership -> damage / detach / breach / destruction
    -> navigation static/dynamic publication changes
```

A detached fragment becomes a dynamic `NavigationMap` actor. A topology-changing breach changes `NavigationSpace` only when the opening has sufficient clearance for the requesting envelope.

Emergency contact must never bypass physics or fabricate the post-impact state.

## 11. Guidance/debug contract

Manual guidance visualizes the accepted corridor/trajectory/control intent actually used by navigation. It must not run a separate planner.

Ordinary `F12` keeps Hub/local presentation. `Shift+F12` toggles Hub render <-> raw NavigationWorld Debug. Debug consumes the same completed snapshot used by navigation/control and must not force synchronous readback/replanning.

Useful debug data now includes:

```text
static regions / portals / clearance
dynamic P/V/A / swept bounds
local horizon / conflict set
adjusted target
gap candidates / chosen gap
selected hull attitude
attitude reachability margin
emergency status
predicted contact expectation
recommended braking / gap-center aim / passage-axis travel
snapshot generation / age
```

Later continuous 6DoF debug should also expose swept hull, relative normal contact speed, control-mode reachability and pilot/controller execution state.

## 12. Roadmap

1. **`NAV-V2-MAP-2` — CLOSED.**
2. **`NAV-V2-SPACE-1` — CLOSED.**
3. **`NAV-V2-LOCAL-1` — CLOSED.** Horizon + same-region avoidance + multiplied fan performance accepted.
4. **`NAV-V2-TRAJECTORY-1` — ACTIVE.** Oriented passage + bounded gap + attitude reachability; emergency passage candidate awaiting target-machine gate.
5. continuous static-gap 6DoF feasibility: body-axis translation + rotation + swept oriented hull + `Elite`/`Newton` semantics.
6. emergency ranking by relative normal contact speed / impact-energy proxy.
7. moving/time-varying obstacle gaps.
8. moving/rotating terminal docking + NPC execution skill.
9. pursuit/receding-intercept consumer.
10. raw NavigationWorld debug visualization.
11. live `EliteGame` / `EliteServer` integration.
12. retire legacy route-wide navigation only after v2 owns the live path.

## 13. Documentation Definition of Done

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
