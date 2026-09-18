# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 11A — live runtime control seam

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed

1  NavigationMap / mass dynamic P/V/A               ACCEPTED
2  NavigationSpace / global corridors               ACCEPTED
3  LocalHorizon / LocalAvoidance                     ACCEPTED
4  oriented passage / bounded gaps / attitude        ACCEPTED
5  continuous static passage                         ACCEPTED
6  emergency mitigation + contact severity           ACCEPTED
7  moving gap prediction                             ACCEPTED
8  continuous ship passage through moving gap        ACCEPTED
9  moving/rotating docking 6DoF                      ACCEPTED
10 deterministic PilotSkillProfile execution         ACCEPTED
11 live game/server/guidance + physics hookup         ACTIVE
   11A runtime control seam                           ACTIVE
   11B authoritative NPC/guidance ownership           PENDING
12 end-to-end stress/debug + legacy retirement        PENDING
```

## Latest accepted gate — stage 10

Target-machine evidence:

```text
b042321b65084950aa91784a5e02344430e5c2bc
NAVIGATION PILOT SKILL CONTRACT: PASS
11/11 navigation_trajectory CTest PASS
100% tests passed
```

`PilotSkillExecutor` is accepted/frozen unless live composition exposes a defect.

Accepted execution semantics:

```text
reaction delay
decision cadence / sample-and-hold
command latency
second-order gain/damping response
linear/angular command slew
seeded deterministic command-space error
urgent-emergency reaction shortening
```

Pilot skill does not alter geometry, vehicle capability, capture tolerances, collision equations or physics truth.

## Accepted trajectory/control chain

```text
NavigationWorld / NavigationSpace / NavigationMap
    -> LocalHorizon / LocalAvoidance
    -> oriented passage / bounded gap
    -> attitude reachability
    -> continuous passage
    -> emergency mitigation / contact severity
    -> moving gap / moving passage
    -> terminal + continuous moving/rotating docking
    -> PilotSkillExecutor
```

## Active stage 11A — live runtime control seam

Authority:

```text
src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md
```

New runtime seam:

```text
NavigationRuntimeControlBridge
    ideal Navigation v2 acceleration intent
    -> accepted PilotSkillExecutor
    -> ShipControlState direct navigation demand
    -> SharedShipPhysics / ShipController / DynamicMotionSystem
    -> authoritative fixed-step motion
```

### Explicit ShipControlState channel

```text
navigationAccelerationDemandValid
navigationLinearAccelerationDemandMapMps2
navigationAngularAccelerationDemandMapRadPerSec2
navigationIntentRevision
```

These are demands, not applied motion.

### Angular path

World angular acceleration demand is projected onto current ship right/up/forward axes and then constrained by the existing:

```text
angularAccel
maxPitchRate / maxYawRate / maxRollRate
maxGs / turnRadius envelope
existing overspeed recovery rules
```

Material manual attitude input wins.

### Linear path

In HubTactical live motion, direct world acceleration demand is split into:

```text
positive forward component
    -> forward-only main engine
    -> maxLinearGs/maxGs limit

remaining vector
    -> six-direction manoeuvre/RCS
    -> manoeuvreThrusterAccel limit
```

Then existing `updateLocalFrameMotion()` still owns speed envelope, Newtonian/Assisted semantics, manoeuvre gas and actual integration.

Material manual translation/cruise/jump/alignment input wins.

### One execution snapshot

The runtime bridge publishes the same executed demand used for control into `ExecutionSnapshot` with the same intent revision. Stage 11B will feed this same snapshot/revision to guidance/debug; presentation must not solve a second maneuver.

## Physics boundary

```text
Navigation       -> intent / prediction
Pilot execution  -> delayed/imperfect command demand
Flight control   -> real vehicle authority
Physics          -> authoritative integration + CCD/TOI/manifold/impulse
Damage           -> structural consequences
```

Navigation never writes authoritative position, velocity or angular rate directly.

## Next

1. target-machine 11A architecture + runtime-control + full canonical build gate;
2. 11B replace placeholder NPC steering with Navigation v2 ownership and publish same intent/execution truth to guidance/debug;
3. stage 12 end-to-end stress/debug/performance;
4. retire legacy navigation only after stable live ownership.
