# Navigation v2 — deterministic NPC pilot skill execution

**Status:** stage 10 isolated candidate pending target-machine gate  
**Updated:** 2026-09-18 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`

## Purpose

Navigation and trajectory feasibility now produce physically truthful intent. Stage 10 adds one separate layer that answers:

```text
How quickly and how faithfully does this particular NPC pilot execute
an already-selected ideal control intent?
```

Implementation:

```text
src/world/navigation/control/PilotSkillExecutor.h
src/world/navigation/control/PilotSkillExecutor.cpp
```

The executor is stateful per pilot but bounded and deterministic. It owns no world search, geometry, hull dimensions, vehicle capability, collision equations or physics integration.

## Ownership split

The accepted separation remains:

```text
world truth
    geometry / obstacles / actual actor state

vehicle capability
    thrust / rotation / hull / damage truth

navigation intent
    route / temporary target / passage / emergency / docking target

pilot skill
    reaction / decision cadence / latency / damping / precision

flight control
    clamp/allocate requested acceleration to real thrusters

physics / collision
    integrate actual motion and resolve contacts
```

Poor pilot behavior must therefore emerge from delayed or imperfect command execution. Static space, capture tolerances and collision shapes remain unchanged.

## Input command

The executor consumes an already-produced acceleration demand:

```text
linear acceleration demand
angular acceleration demand
intent revision
emergency flag
hazard urgency [0,1]
```

The demand is not a new trajectory planner. It is the ideal control request from the accepted navigation/control product.

An intent `revision` identifies a materially new maneuver. Reaction delay restarts when the revision changes. The command may continue to evolve inside the same revision at the pilot's decision cadence.

## PilotSkillProfile

The game-facing profile is explicitly split into two groups.

### ExecutionProfile

Consumed by `PilotSkillExecutor`:

```text
reactionDelaySeconds
perceptionDecisionRateHz
commandLatencySeconds

responseFrequencyHz
dampingRatio
commandGain

maxLinearCommandSlewMetersPerSec3
maxAngularCommandSlewRadPerSec3

deterministicLinearNoiseAmplitudeMetersPerSec2
deterministicAngularNoiseAmplitudeRadPerSec2
deterministicSeed

emergencyResponseThreshold01
emergencyReactionDelayScale
```

### PolicyProfile

Published in the same skill profile but deliberately **not consumed by the executor**:

```text
anticipationSeconds
riskPreference01
comfortPreference01
```

These belong to upstream maneuver selection/live integration. For example, risk preference may decide whether to accept a narrow margin or go around, but it may not secretly change ship acceleration or collision geometry.

Two profiles with identical `ExecutionProfile` and different policy preferences must produce identical execution output.

## Reaction, perception and latency

For a new intent revision:

```text
intent observed
    -> reaction delay
    -> next perception / decision sample
    -> command latency queue
    -> active command target
    -> second-order pilot/controller response
```

The decision layer is sample-and-hold. Changes inside one intent are only observed at:

```text
1 / perceptionDecisionRateHz
```

A command already sampled may remain queued during `commandLatencySeconds`.

The queue is fixed-size and bounded. Profiles are validated so nominal configured latency × decision rate cannot require more queue slots than the executor owns.

## Emergency response

The ordinary reaction delay remains the baseline.

When:

```text
command.emergency == true
AND
hazardUrgency >= emergencyResponseThreshold01
```

effective delay becomes:

```text
reactionDelaySeconds * emergencyReactionDelayScale
```

This permits an alert expert or trained combat pilot to react to a high-urgency emergency faster without making geometry or vehicle authority more favorable.

## Command response / damping

After latency, the pilot does not teleport the command to its target. Each linear/angular acceleration-demand vector follows a second-order response:

```text
y'' = wn^2 * (target - y) - 2*zeta*wn*y'
```

where:

```text
wn   = 2*pi*responseFrequencyHz
zeta = dampingRatio
```

`commandGain` scales the sampled ideal command before response.

Command-rate state is clamped by the configured slew limits. This produces a real execution distinction:

```text
high damping / high update rate / low latency
    -> tracks ideal intent closely

low damping / delayed low-rate decisions / aggressive gain
    -> overshoot and oscillation
```

The executor uses bounded internal integration substeps:

```text
<= 64 substeps per simulation call
step duration <= 0.25 s
```

so work remains bounded.

## Deterministic precision error

Precision variation is applied in **command space**, never by directly moving the ship or corrupting world state.

Noise is generated from:

```text
deterministicSeed
intent revision
decision sequence
axis
```

using a fixed integer hash. No `std::random`, wall clock or frame-global random source participates.

Therefore identical inputs and seed reproduce identical command output. Different seeds may give different but replay-stable pilots.

## Why oscillation is allowed to become a real collision

The executor does not add fake position error.

A poor pilot can instead generate a physically real sequence:

```text
late correction
    -> aggressive command
    -> overshoot
    -> delayed opposite correction
    -> another overshoot
```

The authoritative physics system integrates those commands. If the resulting actual P/V/A consumes all navigation margin, the existing emergency/collision stack deals with the real situation.

Other ships react to the NPC's actual published state, not to the ideal command it failed to execute.

## Docking-like closed-loop fixture

The isolated behavior gate contains a simple one-dimensional relative docking/tracking plant.

The same ideal PD intent is fed through two profiles.

Expert fixture:

```text
reaction delay = 0
decision rate  = 60 Hz
latency        = 0
response       = 4 Hz
damping        = 1.0
gain           = 1.0
```

It must settle near the target by the fixed deadline.

Poor fixture:

```text
reaction delay = 0.20 s
decision rate  = 8 Hz
latency        = 0.15 s
response       = 1 Hz
damping        = 0.20
gain           = 1.40
limited command slew
```

It must cross the target repeatedly and remain visibly less settled at the same deadline.

This fixture proves that poor piloting can create genuine under-damped motion without changing docking tolerances or physics truth.

## Fixed-work / replay contract

The executor is bounded by:

```text
fixed queue <= 256 commands
fixed integration substeps <= 64
one current desired command per call
no world scan
no dynamic allocation in steady-state execution
```

Time input is explicit and must agree with `deltaSeconds`. This prevents replay drift caused by inconsistent caller timing.

## Result diagnostics

Each step publishes:

```text
status
executed linear acceleration demand
executed angular acceleration demand
observed intent revision
active target revision
reactionBlocked
decisionSampled
queuedCommandApplied
pendingCommandCount
```

These diagnostics are intended for future live debug/guidance and NPC behavior inspection.

## Stage-10 acceptance fixtures

The isolated gate pins:

```text
profile validation
reaction delay + command latency
decision cadence / sample-and-hold
emergency shortened reaction
same seed -> identical replay
different seed -> deterministic variation
critical damping -> little/no overshoot
low damping -> visible overshoot/ringing
poor profile -> real closed-loop docking-like oscillation
policy-only changes -> identical execution output
```

## Scope boundary

`PilotSkillExecutor` does not own:

```text
route search
gap/docking geometry
vehicle capability
thruster allocation
physics integration
CCD / TOI / collision response
damage
NPC strategic decision making
```

The next major stage after acceptance is live `EliteGame` / `EliteServer` / guidance integration, where accepted navigation intent, pilot execution, flight control and authoritative physics are connected end to end.
