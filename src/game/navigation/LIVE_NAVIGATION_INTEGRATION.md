# Navigation v2 — live runtime integration

**Status:** stage 11A runtime-control seam candidate pending target-machine gate  
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
