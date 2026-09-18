# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** stage 12A — deterministic end-to-end runtime proving ground  
**Current slice:** 12A-6b3a — verified moving-passage steering authority seam

`main` is the only canonical development branch.

## 1. Architecture summary

Navigation v2 uses one shared **NavigationWorld** for the active play area. It is not one complete independent world/planner per NPC.

```text
AUTHORITATIVE SYSTEM / PHYSICS STATE
            |
            v
shared ship-centered NavigationWorld
    + static NavigationSpace
    |   free space / clearance / portals / cached corridors
    |
    + dynamic NavigationMap
    |   mass P/V/A + angular-motion publication / swept bounds / bins
    |   compact relevant/conflict candidates
    |
    + LocalHorizon / LocalAvoidance
            |
            v
bounded precision trajectory layer
    oriented passages / gaps
    attitude reachability
    static continuous passage
    emergency mitigation / contact severity
    moving-gap prediction
    moving continuous passage
    docking terminal / continuous approach
            |
            v
flight control / Ruckig / authoritative physics
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    cached global corridor search
    deterministic local/precision reference work

GPU
    mass P/V/A prediction
    conservative swept bounds
    spatial bins
    all-agent neighbor/conflict reduction
```

No backend-specific actor tables, graph nodes or GPU buffers cross public navigation boundaries.

## 2. Different work runs at different rates

The system must not recalculate a complete route for every ship every simulation tick.

```text
physics / authoritative actor state
    every simulation tick

dynamic NavigationWorld broadphase / prediction
    every tick or near that rate, shared across actors

local horizon / avoidance
    receding horizon; may be staggered across NPCs

precision passage / emergency / docking
    only for compact conflict or authored precision candidates

global corridor
    revision/event driven, cached and reused
```

No `N ships x full global pathfinding x every tick` design is allowed.

## 3. Coordinate contract

Authoritative physical/system state and navigation working coordinates are separate.

The NavigationWorld origin may translate/rebase with the active domain. Its axes are stable navigation/travel axes and do not roll/pitch/yaw with the controlled hull. Hull-local coordinates belong to vehicle/control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant subset is published into the active NavigationWorld.

## 4. Closed foundations

### `NAV-V2-MAP-2` — CLOSED / ACCEPTED

Accepted 10k target-machine references:

```text
CPU compact queries                    <0.2 ms p95
GPU cruise total                        1.3226 ms p95
GPU hub total                           1.6258 ms p95
```

No synchronous frame-thread GPU dispatch/wait/bulk-readback path is allowed.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Accepted static-space capabilities include sparse free-space regions, explicit portals, envelope clearance, traversable apertures/tunnels/canyons, deterministic costed corridors, local invalidation/patching and turn-aware route state.

Final 10k turn-aware target-machine evidence:

```text
open_10k turn p95   12.0072 ms
hub_10k turn p95    11.9065 ms
```

Gate was `<=40 ms p95` on the worker path.

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Accepted deterministic fan:

```text
15 degree x 8 azimuths
30 degree x 8 azimuths
maximum 16 probes
```

Deliberate `1024 x 17` stress:

```text
519.9286 us p95
```

inside the `<1.0 ms normal peak` CPU design budget.

`ConflictHold` means only that the cheap local layer cannot prove a safe temporary target. It never means navigation must stop producing commands.

## 5. Precision trajectory chain — accepted components

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
EmergencyContactSeverityScorer
MovingGapPredictor
MovingPassageTrajectoryEvaluator
DockingTerminalEvaluator
```

### Oriented passage / bounded gap

Precision uses an oriented hull proxy rather than center + radius only. A conservative sphere may reject a flat slot that a correctly rolled hull can traverse.

`BoundedGapCandidateBuilder` receives one known primary conflict and only already-reduced neighbors. No global all-pairs work is allowed.

Hard output cap:

```text
<= 8 gap candidates
```

Accepted stress:

```text
top8_1024 = 24.0699 us p95
```

### Attitude reachability

A geometric fit is not enough. The requested hull attitude must be physically reachable before entry.

Reference fixture:

```text
90 degree roll
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
closing speed            = 10 m/s

30 m -> ReachableCoast
15 m -> ReachableWithBraking
 8 m -> UnreachableBeforeEntry
```

`UnreachableBeforeEntry` is not a no-command state.

### Emergency mitigation

Accepted invariant:

```text
no collision-free proof != no navigation command
```

Priority:

```text
1. collision-free maneuver if reachable
2. stop before contact if reachable
3. otherwise least-severity explicit non-safe mitigation
```

`EmergencyMitigatedContact` is never `Clear`.

### Continuous static passage

`ContinuousPassageTrajectoryEvaluator` verifies one complete analytic ship segment:

```text
translation: cubic Hermite P/V -> P/V
attitude: shortest-arc smoothstep
33 pose samples
32 conservative between-sample proofs
```

Vehicle authority remains explicit by body axis plus angular rate/acceleration.

Control semantics:

```text
Newtonian
    velocity and hull attitude may diverge

EliteAssisted
    same physical authority
    plus supplied velocity-to-forward slip policy
```

Accepted eight-candidate batch performance:

```text
41.3527 us p95 = 0.04135 ms
```

The static verifier is frozen without contrary live evidence.

### Emergency contact severity

Already-predicted unavoidable contacts are ranked by rigid-body contact-point relative motion:

```text
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Hard bound:

```text
<= 8 candidates
<= 4 contact witnesses per candidate
```

Peak normal closing speed outranks energy/momentum/glancing/progress tie-breakers.

## 6. Moving gaps — ACCEPTED

`MovingGapPredictor` receives one already-selected obstacle pair and compact motion:

```text
P / V / A
angular velocity
conservative radius
snapshot revision
```

Fixed work:

```text
33 time samples
32 continuous intervals
```

Whole-interval gap width uses exact distance to the relative-position chord minus the constant-acceleration deviation bound:

```text
|a_rel| * dt^2 / 8
```

so hidden close/reopen events between samples fail closed.

Per sample it publishes gap center/velocity, separation axis/rate and both boundary surface normals/material velocities including `omega x r`.

Target-machine acceptance:

```text
bc84940ff23209d9731a3d5332b8af3794182790
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 trajectory CTest PASS
```

## 7. Moving continuous ship passage — ACCEPTED

`MovingPassageTrajectoryEvaluator` composes one accepted moving-gap prediction with one concrete ship P/V/attitude/OBB/capability segment.

It synchronizes:

```text
33 ship states <-> 33 gap states
32 ship intervals <-> 32 moving-gap intervals
```

Continuous proof includes changing gap width, transverse ship/gap-center motion, passage-frame rotation, relative OBB sweep and body-axis authority. Point samples alone never prove safety.

Target-machine acceptance:

```text
18799ab2c026b6d6dce3da11f9225eae3a3c5f35
NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
8/8 trajectory CTest PASS
```

## 8. Collision / damage ownership

Navigation performs predictive feasibility and may emit expected-contact evidence. It is **not** the authoritative collision solver.

```text
Navigation / trajectory
    intent
    collision-free proof when possible
    expected-contact witness when not

Physics / Collision
    authoritative broadphase / narrow phase
    CCD / TOI / contact manifold
    impulse / friction / restitution / ricochet

Damage / Structural
    damage / breach / detach / destruction
```

After real contact, navigation replans from actual physics state.

## 9. Moving / rotating docking 6DoF — ACCEPTED

Docking is relative pose/motion matching between explicit ship and dock interface frames, not center-point arrival.

### Stage 9A — terminal 6DoF capture ACCEPTED

Authority:

```text
src/world/navigation/DOCKING_TERMINAL_MODEL.md
```

Both interfaces expose:

```text
surface semantic
local port offset
mating normal
referenceUp / rollReference
```

Current required pairing:

```text
Bottom(ship) -> Bottom(dock)
```

For a moving/rotating dock port:

```text
p_origin(t) = p0 + v0*t + 0.5*a*t^2
v_origin(t) = v0 + a*t
v_port      = v_origin + omega x r
```

Terminal capture requires bounded:

```text
relative port position
relative port linear velocity
mating normals anti-aligned
referenceUp / roll aligned
relative angular velocity
```

A 180-degree rolled ship is invalid even when mating normals oppose correctly.

Target-machine acceptance:

```text
835271539619b7dd02efc54ff54df51d64b49fce
NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
9/9 trajectory CTest PASS
```

The single unused-helper warning seen during that build has been removed. Dock-port prediction is now a shared bounded helper for 9B.

### Stage 9B — continuous final docking approach ACCEPTED

Authority:

```text
src/world/navigation/DOCKING_APPROACH_MODEL.md
```

New component:

```text
DockingApproachEvaluator
```

#### Dock-local trajectory

The final precision segment is expressed in the moving/rotating dock frame rather than as a world-space rest-to-rest target.

In dock-local coordinates:

```text
corridor is static
terminal relative pose is fixed
relative terminal angular rate -> 0
```

Therefore:

```text
omega_ship(capture) = omega_dock
```

naturally follows from the trajectory representation.

Endpoint relative velocity uses the derivative of a rotating frame:

```text
v_rel_world = v_ship - v_port - omega x (p_ship - p_port)
```

#### Continuous corridor proof

The accepted `ContinuousPassageTrajectoryEvaluator` is reused as a dock-local geometry oracle with:

```text
PassageSource::DockingCorridor
33 relative poses
32 conservative continuous intervals
```

The internal geometry call receives effectively unbounded capabilities and Newtonian policy so rotating-frame coordinates are never mistaken for physical thrust authority.

#### World inertial authority

The accepted relative curve is transformed back to world kinematics:

```text
v_world = v_port + omega x r + v_rel

a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_rel
        + a_rel
```

Centripetal and Coriolis terms are therefore paid by real vehicle authority.

Between samples, body-axis thrust projection receives a conservative world-jerk + body-axis-rotation margin. Physical authority is not sample-only.

#### Angular authority

For relative attitude change `theta` over `T`:

```text
omega_rel_peak = 1.5 * theta / T
alpha_rel_peak = 6.0 * theta / T^2
```

World conservative bounds:

```text
omega_world_peak <= |omega_dock| + omega_rel_peak
alpha_world_peak <= alpha_rel_peak + |omega_dock| * omega_rel_peak
```

A sufficiently fast rotating station may therefore be physically uncapturable by a low-authority vehicle.

#### Terminal composition

After continuous geometry + inertial linear/angular authority pass, the generated final state is submitted to the accepted `DockingTerminalEvaluator`.

Only:

```text
FeasibleForCapture
```

claims both final-segment feasibility and terminal 6DoF capture.

Normal docking aborts/goes around when a safe/capturable final segment cannot be proven. Destructive impact is never successful docking.

### Stage 9B first target-machine attempt — NOT ACCEPTED

The first run built successfully and retained 9/9 prior behavior, but the new docking-approach test failed. Investigation proved the pinned fixture itself was invalid: the true Hermite excursion (`0.9622504486 m`) was smaller than the old available center travel (`0.975 m`), so a correct verifier could not reject it.

The repaired regression uses `0.9618 m` available center travel. It preserves the intended hard condition:

```text
all 33 sampled poses fit
but the analytic curve exits between samples
=> CorridorBlocked
```

The implementation's continuous proof was not weakened. The repaired rerun on `c90a66d6c64bdf3acc037208a000b1955d40e6c3` passed the architecture contract and all 10/10 trajectory tests. Stage 9B and the complete docking stage are accepted.

## 10. NPC pilot skill — ACCEPTED

Authority:

```text
src/world/navigation/PILOT_SKILL_MODEL.md
src/world/navigation/control/PilotSkillExecutor.h/.cpp
```

Vehicle capability and pilot skill remain separate. The executor consumes ideal linear/angular acceleration demand and changes only command execution:

```text
reaction delay
decision/sample cadence
command latency
second-order response frequency
damping / gain
linear/angular command slew
deterministic seeded command-space error
urgent-emergency reaction scaling
```

A maneuver revision restarts reaction delay. Changes inside one revision are sample-and-hold at the configured decision rate.

`PolicyProfile` publishes anticipation/risk/comfort but the executor deliberately does not consume those fields. They belong to upstream maneuver selection.

The execution response is a deterministic second-order system. Low damping and delayed low-rate decisions may therefore create genuine overshoot/oscillation. The executor never directly perturbs P/V, hull geometry or collision truth; authoritative physics must integrate the resulting command.

Bounded work:

```text
fixed queue <= 256
integration substeps <= 64
step <= 0.25 s
no world scan
no std::random
```

Pinned stage-10 fixtures include replay identity, decision/latency timing, emergency reaction, damping overshoot, a poor-pilot docking-like oscillation, and proof that policy-only preference changes do not alter execution.

Target-machine stage-10 acceptance:

```text
b042321b65084950aa91784a5e02344430e5c2bc
NAVIGATION PILOT SKILL CONTRACT: PASS
11/11 trajectory/pilot CTest PASS
```

## 11. Live integration — ACCEPTED

Authority:

```text
src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md
```

Stage 11A introduced one explicit runtime control seam:

```text
Navigation intent
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> ShipControlState navigation acceleration demand
    -> SharedShipPhysics / ShipController / DynamicMotionSystem
```

The direct demand is not a physics override. Angular demand is clamped by the existing ship angular authority. Linear demand maps onto the real forward-only main engine plus bounded manoeuvre thrusters, then passes through existing speed/resource integration.

Material manual controls override the corresponding navigation demand.

`ExecutionSnapshot` mirrors the exact executed demand/revision sent to control. Stage 11B must use this same truth for guidance/debug instead of re-solving a presentation path.

Stage 11A target-machine acceptance:

```text
d7c77d5868b3178be3c392f0a8fecad5b57e3b69
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation_trajectory/pilot 11/11 PASS
EliteGame + EliteServer canonical build PASS
```

Stage 11B-1 now replaces the placeholder NPC steering authority. `NpcAiSystem` publishes only `NpcNavigationGoal` plus `PilotSkillProfile`; it no longer emits `ShipControlState` or control-surface inputs.

`NpcNavigationIntentController` owns the nominal acceleration-intent conversion, and `GameSimulation` owns persistent per-NPC `NavigationRuntimeControlBridge` state. Activation-decimated elapsed time is advanced in exact bounded `<=0.25 s` pieces.

The exact latest `ExecutionSnapshot` used for control is retained per NPC as the server truth seam for stage 11B-2 replication/guidance. No direct-steering fallback is allowed.

Stage 11B-2 replicates this truth sparsely:

```text
ShipSnapshot.navigationExecution =
    variant<monostate, NavigationExecutionSnapshot>

absent execution -> one wire tag byte
present execution -> tag + execution payload
```

The client mirror refreshes only when the accepted server snapshot tick changes. It is read-only to all client planning/trajectory components.

### Stage 11B-1 first target-machine attempt — NOT ACCEPTED

The architecture contracts passed and trajectory/pilot remained 11/11, but the full integration gate exposed two build-target wiring defects:

```text
isolated navigation_runtime
    missing GLM_ENABLE_EXPERIMENTAL

headless EliteServer
    missing live-navigation implementation sources
```

Repaired source wiring:

```text
tests/navigation_runtime/CMakeLists.txt
    GLM_ENABLE_EXPERIMENTAL

EliteServer
    NavigationRuntimeControlBridge.cpp
    NpcNavigationIntentController.cpp
    PilotSkillExecutor.cpp
```

The ownership checker now pins both requirements. No behavior or safety contract was weakened.

### Stage 11B-1 second target-machine attempt — NOT ACCEPTED

The build-target wiring was repaired and both canonical targets built. The isolated runtime unit still linked full `Ship` construction, pulling unrelated equipment/reactor/damage vtables.

The boundary was corrected:

```text
GameSimulation
    authoritative Ship
    -> NpcNavigationKinematicState
    -> NpcNavigationIntentController
```

The controller no longer depends on the full live ship runtime.

### Stage 11B-1 third target-machine attempt — NOT ACCEPTED

Architecture PASS, trajectory/pilot 11/11 PASS, EliteGame build PASS and EliteServer build PASS. The sole failure was a roll-damping assertion that checked a raw world-Z component.

With identity orientation `forward=-Z`, correct positive-roll damping is a world `+Z` vector. `ShipController` projects that vector onto `forward` and obtains negative local roll acceleration.

The regression now checks:

```text
dot(a_ang,right)   = -pitchRate*damping
dot(a_ang,up)      = -yawRate*damping
dot(a_ang,forward) = -rollRate*damping
```

No production logic was changed.

### Stage 11B-1 target-machine acceptance

Accepted on:

```text
fb83b8d80f29c6c5e4e12b8a2fca731ffea7b8e8

NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation_trajectory/pilot 11/11 PASS
EliteGame build PASS
EliteServer build PASS
```

11B-1 is frozen unless 11B-2/stage-12 integration reveals a real defect.

### Stage 11B-2 — replicated guidance/debug truth ACCEPTED

The exact server execution product now follows the existing replication boundary:

```text
GameSimulation per-NPC ExecutionSnapshot
    -> ShipSnapshot.navigationExecution
    -> ordered binary wire schema
    -> ClientShipState.navigationExecution
    -> ReplicatedNavigationExecutionState
    -> ClientNavigationWorkspace
    -> GuidanceHudPresentation
```

Replicated fields include intent/active-target revisions, ideal and executed linear/angular acceleration demand, emergency/urgency and pilot timing diagnostics.

The simulation snapshot wire schema is intentionally bumped to version 8.

The client mirror is indexed by both transient `EntityId` and stable `ShipInstanceId`. Route START executors therefore resolve server execution through stable `NavigationAssetRef::Ship` identity.

The workspace exposes one explicit sync ingress and only a const replicated-state view to presentation/planning consumers.

`GuidanceCorridorHudPresentation` exposes the authoritative execution metadata for the selected route executor, but `LocalGuidancePlanner` and `DockingPathPlanner` are forbidden from consuming or rewriting this replicated execution product.

New runtime acceptance test:

```text
navigation_replication_truth
```

pins binary round-trip, stable-identity lookup and guidance presentation without fabricating a corridor.

### Stage 11 target-machine acceptance

Stage 11B-2 and therefore all of stage 11 are accepted on target-machine evidence from:

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713

NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS
NAVIGATION LIVE REPLICATION/GUIDANCE CONTRACT: PASS
wire schema architecture PASS
navigation_runtime 2/2 PASS
navigation_trajectory/pilot 11/11 PASS
wire_data_plane_contracts 1/1 PASS
EliteGame build PASS
EliteServer build PASS
```

The sparse absent execution representation is pinned to exactly one variant-tag byte. Stage 11 is frozen unless stage-12 end-to-end evidence exposes a real integration defect.

Stage 12 authority:

```text
src/game/navigation/STAGE12_END_TO_END.md
```

The first stage-12 slice is a deterministic station-adjacent proving ground driven through the production runtime chain, followed by interactive Shift+F12 visualization of the same completed navigation truth.

## 12. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/global precision solve      asynchronous only
```

These are target-machine design budgets, not portable timing assertions.

## 12. Guidance/debug contract

Guidance visualizes the accepted navigation/trajectory/control intent; it must not run a separate planner.

`Shift+F12` raw NavigationWorld debug should eventually expose static regions/portals, dynamic predictions, local conflicts, gaps, moving-gap states, accepted continuous trajectory, emergency severity, docking frame/tolerances and pilot/controller execution state.

## 13. Progress / roadmap

```text
[██████████████████████░] 11 / 12 major stages closed

1  NavigationMap / mass dynamic P/V/A               CLOSED
2  NavigationSpace / global corridors               CLOSED
3  LocalHorizon / LocalAvoidance                     CLOSED
4  oriented passage / bounded gaps / attitude        CLOSED
5  continuous static passage                         CLOSED
6  emergency mitigation + severity                   CLOSED
7  moving gap prediction                             CLOSED
8  moving continuous ship passage                    CLOSED
9  moving/rotating docking 6DoF                      CLOSED
   9A terminal capture                               CLOSED
   9B continuous final approach                      CLOSED
10 PilotSkillProfile                                 CLOSED
11 live EliteGame / EliteServer / guidance + physics CLOSED
   11A runtime control seam                           CLOSED
   11B-1 authoritative NPC runtime ownership           CLOSED
   11B-2 replicated guidance/debug truth               CLOSED
12 end-to-end stress/debug + legacy retirement       ACTIVE
```

Legacy route-wide navigation is retired only after v2 owns the stable live path.

## 14. Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant model / benchmark contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG/DECISIONS
```

Stale state/task documentation or branch ambiguity blocks handoff.


## Deferred higher-level planner

Galaxy-scale gravity/fuel/jump routing is not part of Local NavigationWorld v2.
Its future contract is `src/game/navigation/GALACTIC_ROUTE_PLANNING.md`.

NavigationWorld remains the local dynamic/static safety layer and receives
handoffs from higher-level strategic/system planning rather than becoming a
light-year-scale obstacle planner.


## Oriented finite-depth portal traversal

A navigation portal may represent more than a zero-thickness topological
boundary. Narrow doors, hangar mouths, ravines and tunnels may require a
capture condition before the vehicle is allowed to enter.

`NavigationSpace::PortalTraversalInput` is the static contract for such a
passage. It can publish an oriented route-direction normal, an approach
distance, transit speed and entry constraints for velocity direction,
cross-track velocity and vehicle-forward alignment.

The runtime execution sequence is:

```text
coarse corridor selects oriented portal
    -> approach/staging point before entry
    -> reduce cross-track position/velocity
    -> align flight-path vector with portal normal
    -> align vehicle forward axis when required
    -> PortalTransit
    -> maintain passage axis/corridor to exit portal
```

For reverse traversal the same authored A->B normal is automatically reversed
by NavigationSpace.

Topology and physical collision truth are separate:
- region/portal queries decide where traversal is legal;
- `SegmentQuery::exactObstaclesOnly` tests only persistent exact HitVolume
  geometry for already topology-authorized trajectories and actual physical
  sweeps.

Virtual region partitions must never behave as collision walls.

This is a generic vehicle-independent contract. Vehicle hull dimensions and
linear/angular capability remain runtime inputs; no Cobra-specific acceleration
constant belongs in the portal planner.


## Bounded visibility steering for ordinary local transit

Normal local travel is receding-horizon visibility steering, not precision
passage solving:

```text
current vehicle A -> current accepted target B
    direct hull-sized corridor inside physical horizon?
        yes -> steer toward B
        no  -> smallest safe deflection around nearest conflict
                  -> temporary pass-through target
next update:
    test direct B first again
        -> immediately recover to the direct line when clear
```

The "line of sight" is a corridor, not an infinitesimal ray. Its envelope is the
vehicle hull plus safety clearance, and its length is bounded by the physical
receding horizon derived from current speed, result age, braking distance,
turn-distance allowance and safety margin.

Dynamic obstacles contribute predicted/swept occupancy only inside that bounded
window. The default transit layer does not attempt route-wide centimeter-level
prediction.

Precision MovingPassage remains available but is not the default response to two
free-space obstacles. It is entered only when an accepted route requires a
specific constrained passage or terminal geometry, such as an authored portal,
tunnel, docking mouth, gate or ravine.

This separation keeps ordinary transit reactive and cheap while retaining exact
continuous proof where geometry genuinely requires it.


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
