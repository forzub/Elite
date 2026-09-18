# Navigation v2 — live runtime integration

**Status:** stage 11A + 11B-1 ACCEPTED; stage 11B-2 replicated guidance/debug truth candidate pending target-machine gate  
**Updated:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/PILOT_SKILL_MODEL.md`

## Stage 11 goal

Stages 1–10 established isolated navigation, trajectory, docking and pilot-execution truth. Stage 11 connects that accepted product to the authoritative game runtime without creating a second planner or a second physics model.

The intended live chain is:

```text
NavigationWorld / accepted maneuver intent
        |
        v
NavigationRuntimeControlBridge
        |
        v
PilotSkillExecutor
        |
        v
ShipControlState navigation acceleration demand
        |
        v
SharedShipPhysics / ShipController / DynamicMotionSystem
        |
        v
authoritative ship state
        |
        +----> replication / server truth
        |
        +----> guidance/debug uses the SAME intent/execution revision
```

Stage 11 is split:

```text
11A runtime control seam
11B GameSimulation / NpcAiSystem / guidance ownership
```

This document currently defines 11A and the ownership boundary required for 11B.

## 11A — direct acceleration-demand seam

Legacy `ShipControlState` is primarily a keyboard/control-surface interface:

```text
pitch/yaw/roll inputs
targetSpeedRate
forward/strafe/lift RCS keys
alignment commands
```

Navigation v2 already reasons in physically meaningful linear/angular acceleration demand. Converting that back into fake keyboard presses would lose 6DoF semantics, especially for Newtonian flight and docking.

Therefore `ShipControlState` now has one explicit autopilot/navigation channel:

```text
navigationAccelerationDemandValid
navigationLinearAccelerationDemandMapMps2
navigationAngularAccelerationDemandMapRadPerSec2
navigationIntentRevision
```

These values are demands only. They never bypass vehicle authority.

## Pilot execution bridge

`NavigationRuntimeControlBridge` owns one accepted `PilotSkillExecutor` for one controlled actor.

Input:

```text
intent revision
ideal linear acceleration demand
ideal angular acceleration demand
emergency flag
hazard urgency
```

Output:

```text
ShipControlState direct navigation demand
ExecutionSnapshot
```

The same executed demand is copied into both. This gives stage 11B one source for control and debug/guidance instead of recomputing presentation intent independently.

`ExecutionSnapshot` includes:

```text
intent revision
active pilot target revision
ideal linear/angular demand
executed linear/angular demand
emergency / urgency
reactionBlocked
decisionSampled
queuedCommandApplied
pendingCommandCount
```

The bridge owns no geometry, world search, collision, rendering or vehicle capability.

## Angular authority remains ShipController authority

For direct navigation angular demand, `SharedShipPhysics` calls the dedicated `ShipController::updateControlRates(... angularDemand ...)` overload.

The world-space angular acceleration vector is projected onto the current ship principal axes:

```text
pitch axis = ship right
yaw axis   = ship up
roll axis  = ship forward
```

Then the existing limits remain authoritative:

```text
angularAccel
maxPitchRate
maxYawRate
maxRollRate
maxGs / turnRadius envelope
existing overspeed recovery rule
```

The direct path does not add hidden angular damping because the accepted navigation/controller demand already specifies requested angular acceleration.

Material manual pitch/yaw/roll input wins over navigation angular demand.

## Linear authority remains DynamicMotionSystem authority

`GameSimulation` uses `DynamicMotionSystem::applyWorldAccelerationDemand()` only when:

```text
navigationAccelerationDemandValid == true
AND no material manual translation/cruise/jump/alignment override exists
```

The requested world acceleration is split into real propulsion systems.

Main engine:

```text
forward-only
<= maxLinearGs/maxGs acceleration authority
```

Remaining vector:

```text
six-direction manoeuvre/RCS
<= manoeuvreThrusterAccel
```

Therefore reverse demand cannot invent a reverse main engine. It may use only the real manoeuvre authority unless the trajectory has already rotated the ship.

After this mapping, ordinary `DynamicMotionSystem::updateLocalFrameMotion()` still owns:

```text
controlled-speed envelope
Newtonian vs Assisted semantics
manoeuvre-gas depletion/recharge
actual local/world velocity integration
```

## Manual override

11A deliberately preserves player/control-surface authority.

Angular manual override:

```text
material pitch/yaw/roll input -> manual path wins
```

Linear manual override:

```text
cruise
jump
assisted max-speed command
velocity-alignment command
targetSpeedRate
forward/strafe/lift RCS input
    -> legacy/manual input path wins
```

The navigation demand remains explicit state, but is not applied for that control sample.

## No physics bypass

Forbidden 11A behavior:

```text
navigation writes ShipTransform position
navigation writes velocity directly
navigation writes angular rates directly
bridge clamps or invents ship capability
pilot skill changes geometry/collision truth
guidance calculates a separate maneuver
```

The bridge can request acceleration. Only existing ship-control/physics code may turn that request into motion.

## 11A pinned tests

The isolated runtime gate verifies:

```text
bridge emits direct demand and matching execution snapshot
bridge does not synthesize legacy keyboard inputs
main engine remains forward-only
main acceleration respects maxLinearGs/maxGs
remaining/reverse vector respects manoeuvreThrusterAccel
direct angular demand respects existing angular envelope
manual attitude input overrides navigation angular demand
PilotSkillExecutor output reaches real ShipController capability clamps
```

## Stage 11A target-machine acceptance

Accepted on:

```text
d7c77d5868b3178be3c392f0a8fecad5b57e3b69
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation_trajectory/pilot 11/11 PASS
canonical EliteGame build PASS
canonical EliteServer build PASS
```

The only compiler warning introduced by the first candidate was an unused helper in `SharedShipPhysics.cpp`; it is removed in the stage-11B branch.

## 11B-1 — authoritative NPC motion ownership

The previous NPC runtime returned a `ShipControlState` directly and synthesized steering with:

```text
yawInput = sin(position.x * ...)
forwardInput / targetSpeedRate
```

That path is retired.

`NpcAiSystem` now owns only:

```text
NpcNavigationGoal
PilotSkillProfile selection
```

It does not emit control-surface input or acceleration demand.

The initial goal product is deliberately minimal:

```text
Hold
MaintainForwardCruise
```

This is an ownership fixture, not the final NPC behavior catalog. Mission, traffic, repair, chase and attack systems may later choose richer goals without becoming steering solvers.

### Nominal Navigation v2 intent

`NpcNavigationIntentController` converts one goal plus actual ship state into a nominal acceleration intent.

For `MaintainForwardCruise`:

```text
desired relative velocity = current ship forward * desiredForwardSpeed
linear demand =
    (desired relative velocity - actual relative velocity)
    * velocityResponsePerSecond
```

For `Hold`, desired relative velocity is zero.

Angular demand damps actual body angular rates and is published back in world axes:

```text
-right   * pitchRate * angularDamping
-up      * yawRate   * angularDamping
-forward * rollRate  * angularDamping
```

This controller is a nominal local-control product. It owns no world search and no collision response. NavigationMap/LocalHorizon/precision layers may override the nominal intent in the end-to-end composition; NPC AI itself may not.

### Per-NPC runtime bridge

`GameSimulation` now owns persistent per-entity:

```text
NavigationRuntimeControlBridge
last navigation execution time
latest ExecutionSnapshot
```

At each activation-authorized NPC decision:

```text
NpcAiSystem::computeGoal
 -> NpcNavigationIntentController::buildIntent
 -> per-NPC NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation demand
```

No fallback to the retired direct steering path is allowed. A bridge failure clears control and fails closed.

### Activation cadence correctness

`PilotSkillExecutor` limits one step to 0.25 seconds. A distant/coarse NPC may wake after a larger accumulated activation interval.

The server therefore advances the exact elapsed interval as deterministic bounded pieces:

```text
while lastTime < authoritative executionTime:
    dt = min(0.25 s, remaining elapsed time)
    bridge.step(lastTime + dt, dt, same intent)
```

It does not clamp away elapsed time. Pilot reaction/latency dynamics therefore do not change merely because activation decimated the NPC.

### Same execution truth for diagnostics/guidance

`GameSimulation::npcNavigationExecutionSnapshots()` exposes the exact latest `ExecutionSnapshot` used to create the NPC's live `ShipControlState`.

This is the server-side truth seam for 11B-2 replication/guidance. The client must receive this revision/product rather than recomputing an independent NPC maneuver.

## Stage 11B-1 first target-machine attempt — NOT ACCEPTED

The first 11B-1 target-machine run proved the ownership contracts but exposed two build-integration defects:

```text
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS
navigation_trajectory/pilot 11/11 PASS
```

but:

```text
navigation_runtime compile FAIL
    isolated test harness did not mirror production
    GLM_ENABLE_EXPERIMENTAL compile contract

EliteServer link FAIL
    headless source list omitted:
      NavigationRuntimeControlBridge.cpp
      NpcNavigationIntentController.cpp
      PilotSkillExecutor.cpp
```

These failures are integration wiring defects, not accepted-behavior failures.

Repairs:

```text
tests/navigation_runtime/CMakeLists.txt
    add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

EliteServer source set
    + NavigationRuntimeControlBridge.cpp
    + NpcNavigationIntentController.cpp
    + PilotSkillExecutor.cpp

check_navigation_live_npc_ownership.py
    now pins both headless-server source completeness
    and the isolated GLM compile contract
```

Stage 11B-1 remains pending a fresh full target-machine rerun.

## Stage 11B-1 second target-machine attempt — NOT ACCEPTED

The second rerun proved that server build wiring was repaired:

```text
live runtime architecture PASS
live NPC ownership architecture PASS
navigation trajectory/pilot 11/11 PASS
EliteGame build PASS
EliteServer build PASS
```

The only remaining failure was the isolated `navigation_runtime` link. The test fixture instantiated a full `Ship`, which pulled `ShipCore`, equipment, reactor/cooling, damage and other unrelated vtables into what should be a narrow navigation-runtime unit boundary.

This is repaired architecturally rather than by linking unrelated ship subsystems into the test.

New lightweight contract:

```text
NpcNavigationGoal
        +
NpcNavigationKinematicState
    relative world velocity
    forward/right/up
    pitch/yaw/roll rates
        |
        v
NpcNavigationIntentController
```

`NpcNavigationIntentController` no longer includes or accepts the full live ship runtime. `GameSimulation` is the adapter from authoritative `Ship` state to `NpcNavigationKinematicState`.

The isolated runtime fixtures now construct only the compact state, so the unit target does not require reactor/equipment/damage/Ship vtables.

The ownership checker now pins this decoupling.

## Stage 11B-1 third target-machine attempt — NOT ACCEPTED

The third rerun proved the production integration path is now green:

```text
live runtime architecture PASS
live NPC ownership architecture PASS
navigation trajectory/pilot 11/11 PASS
EliteGame build PASS
EliteServer build PASS
```

The only failing item was one isolated runtime assertion:

```text
NPC nominal intent must damp roll through world angular demand
```

The controller implementation was correct. The fixture incorrectly asserted a raw world-Z sign for roll damping.

Elite's identity-frame convention is:

```text
right   = +X
up      = +Y
forward = -Z
```

A positive roll rate is therefore angular velocity along `-Z`. Correct damping points along `+Z`, and `ShipController` projects that world vector back onto `forward=-Z` to obtain a negative local roll acceleration.

The repaired fixture is frame-invariant and checks local-axis projections instead:

```text
dot(angularDemand, right)   = -pitchRate * damping
dot(angularDemand, up)      = -yawRate   * damping
dot(angularDemand, forward) = -rollRate  * damping
```

Production runtime behavior was not changed.

## Stage 11B-1 target-machine acceptance

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

This closes authoritative NPC runtime ownership. `NpcAiSystem` remains goal/policy only, `GameSimulation` owns persistent per-NPC pilot/runtime state, and there is no fallback to the retired direct steering path.

## 11B-2 — replicated guidance/debug truth candidate

11B-2 carries the exact server execution product through the existing replication pipeline.

### Replicated DTO

`ShipSnapshot` now carries:

```text
NavigationExecutionSnapshot
    valid
    intentRevision
    activeTargetRevision

    ideal linear/angular acceleration demand
    executed linear/angular acceleration demand

    emergency / urgency
    reactionBlocked
    decisionSampled
    queuedCommandApplied
    pendingCommandCount
```

The server fills this DTO directly from the same `GameSimulation::npcNavigationExecutionSnapshots()` row used to create live NPC control. No presentation-side reconstruction is involved.

### Wire contract

`NavigationExecutionSnapshot` participates in the canonical ordered binary schema through `ShipSnapshot.navigationExecution`.

The simulation snapshot wire version is now:

```text
SimulationSnapshotWireSchemaVersion = 8
```

This intentionally rejects accidental cross-version decoding rather than silently interpreting a changed ship payload layout.

### Client hydration

`ClientWorldState::ClientShipState` retains the replicated `navigationExecution` for both newly hydrated and already-known ships.

Sparse replication semantics remain unchanged:
- omitted ship row -> retain previous state;
- updated ship row -> replace with newest execution truth;
- explicit ship removal -> remove the client ship and its presentation source.

### Stable route-executor binding

`ReplicatedNavigationExecutionState` mirrors valid replicated entries using both:

```text
EntityId
ShipInstanceId
```

`ShipInstanceId` is the stable lookup used by a `NavigationAssetRef::Ship`. This lets the navigation HUD/debug resolve the current runtime entity for the route START executor without storing transient `EntityId` in the authored route.

The workspace exposes:

```text
syncReplicatedNavigationExecution(...)
const replicatedNavigationExecution()
```

There is deliberately no mutable `replicatedNavigationExecution()` getter. Client planners may inspect server truth but may not rewrite it.

### Guidance presentation

`GuidanceCorridorHudPresentation` now carries optional authoritative execution metadata for the selected route executor:

```text
hasAuthoritativeExecution
authoritativeExecutionEntityId
authoritativeIntentRevision
authoritativeActiveTargetRevision
authoritativeExecutedLinearAcceleration
authoritativeExecutedAngularAcceleration
authoritativeEmergency
authoritativeReactionBlocked
```

This metadata is populated from the read-only replicated execution state before corridor presentation is resolved.

Important separation:

```text
client LocalGuidancePlanner / DockingPathPlanner
    may build manual/advisory player guidance

replicated NPC execution truth
    is server-produced read-only observation
    is not fed back into either planner
```

Therefore an NPC debug/HUD view can show the exact command that the server is executing without running a second NPC planner on the client.

### 11B-2 pinned behavior

The runtime suite adds `navigation_replication_truth`, which pins:

```text
ShipSnapshot execution -> wire encode/decode round-trip
stable ShipInstanceId -> current replicated execution lookup
selected route executor -> GuidanceHudPresentation authoritative metadata
no corridor published -> no fake corridor becomes visible
```

The existing canonical wire-data-plane contract is also extended with navigation execution fields.

## 11B after acceptance

11B will wire the seam into authoritative runtime ownership:

```text
GameSimulation
    shared NavigationWorld state
    -> bounded maneuver intent per controlled NPC
    -> per-NPC NavigationRuntimeControlBridge
    -> ShipControlState
    -> existing fixed-step motion

NpcAiSystem
    stops being the current sin(position) placeholder
    becomes policy/goal input to Navigation v2 rather than a second steering solver

Guidance/debug
    consumes the same accepted intent/execution snapshot/revision
    never replans separately
```

Player manual guidance may remain advisory, but any autopilot/follower path must use the same accepted trajectory/control intent that is displayed.

## Collision ownership

Stage 11 does not move exact collision into navigation.

```text
Navigation
    predicts / selects intent

Pilot + flight control
    execute demand

Physics / Collision
    authoritative CCD / TOI / manifold / impulse / ricochet

Damage
    authoritative structural consequences
```

After contact, NavigationWorld consumes the actual post-impact state and replans.
