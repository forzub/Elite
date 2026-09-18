# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** stage 10 — deterministic `PilotSkillProfile` execution

## Progress

```text
[██████████████████░░░░░] 9 / 12 major stages closed

1  NavigationMap / mass dynamic P/V/A               ACCEPTED
2  NavigationSpace / global corridors               ACCEPTED
3  LocalHorizon / LocalAvoidance                     ACCEPTED
4  oriented passage / bounded gaps / attitude        ACCEPTED
5  continuous static passage                         ACCEPTED
6  emergency mitigation + contact severity           ACCEPTED
7  moving gap prediction                             ACCEPTED
8  continuous ship passage through moving gap        ACCEPTED
9  moving/rotating docking 6DoF                      ACCEPTED
   9A terminal 6DoF capture                          ACCEPTED
   9B continuous final docking approach              ACCEPTED
10 PilotSkillProfile                                 ACTIVE
11 live game/server/guidance + physics hookup         PENDING
12 end-to-end stress/debug + legacy retirement        PENDING
```

## Closed foundation performance

```text
NavigationMap CPU compact queries            <0.2 ms p95
NavigationMap GPU 10k heavy scene            <1.7 ms p95
NavigationSpace 10k turn-aware corridor      ~12 ms p95 (<40 ms worker gate)
LocalAvoidance deliberate 1024 x 17 stress   519.9286 us p95
BoundedGap top8_1024                          24.0699 us p95
Continuous static full_precision_batch8       41.3527 us p95
```

## Accepted precision/control primitives

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
DockingApproachEvaluator
```

Accepted invariant:

```text
no collision-free proof != no navigation command
```

Exact CCD/TOI/manifold/impulse/ricochet remain physics authority.

## Latest accepted gate — docking stage 9

Stage 9A:

```text
835271539619b7dd02efc54ff54df51d64b49fce
NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
9/9 CTest PASS
```

Stage 9B final rerun:

```text
c90a66d6c64bdf3acc037208a000b1955d40e6c3
NAVIGATION TRAJECTORY DOCKING APPROACH CONTRACT: PASS
10/10 CTest PASS
100% tests passed
```

The first 9B failure remains documented as a repaired regression-fixture error. The production continuous verifier was not weakened. Moving/rotating docking mathematics is now closed.

## Active candidate — `PilotSkillExecutor`

Authority:

```text
src/world/navigation/PILOT_SKILL_MODEL.md
src/world/navigation/control/PilotSkillExecutor.h
src/world/navigation/control/PilotSkillExecutor.cpp
```

Purpose:

```text
ideal accepted control intent
    -> reaction delay
    -> decision/sample cadence
    -> command latency
    -> deterministic precision error
    -> gain / damping / slew response
    -> executed acceleration demand
```

Pilot skill never changes:

```text
world geometry
hull dimensions
vehicle capability
capture tolerances
collision equations
physics truth
```

### Execution profile

```text
reactionDelaySeconds
perceptionDecisionRateHz
commandLatencySeconds
responseFrequencyHz
dampingRatio
commandGain
linear/angular command slew
deterministic command-space error + seed
emergency response threshold / delay scale
```

### Policy profile

```text
anticipationSeconds
riskPreference01
comfortPreference01
```

Policy fields are intentionally not consumed by the executor. They belong to upstream maneuver selection/live integration.

### Bounded/replay-safe design

```text
fixed latency queue <= 256
integration substeps <= 64
step <= 0.25 s
explicit time + dt consistency
integer-hash deterministic noise
no std::random
no world scan
no steady-state dynamic allocation
```

### Pinned behavior

```text
reaction delay + latency
decision cadence / sample-and-hold
emergency shortened reaction
same seed -> identical replay
different seed -> deterministic variation
critical damping -> little/no overshoot
low damping -> overshoot/ringing
poor profile -> repeated docking-like target crossings
policy-only changes -> identical execution output
```

## Ownership boundary

```text
Navigation / trajectory
    truthful intent and feasibility

PilotSkillExecutor
    delayed/imperfect deterministic execution

Flight control / thruster allocation
    enforce real vehicle authority

Physics / Collision
    integrate real motion; CCD/TOI/manifold/impulse

Damage / Structural
    actual consequences
```

## Next after stage 10 acceptance

1. close/freeze deterministic pilot execution;
2. connect accepted navigation + pilot execution to live `EliteGame` / `EliteServer` / guidance and authoritative physics;
3. run end-to-end stress/debug/performance;
4. retire legacy route-wide navigation only after v2 owns the stable live path.
