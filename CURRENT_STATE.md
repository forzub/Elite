# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — emergency contact severity ranking

## Closed foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted target-machine evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

The deterministic 16-probe fan is accepted. Deliberate `1024 x 17` stress is `519.9286 us p95`, inside the `<1.0 ms normal peak` budget.

## `NAV-V2-TRAJECTORY-1` — active precision/control layer

Accepted isolated components:

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
```

### Bounded gap — ACCEPTED

Worst deliberate stress:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

### Emergency passage mitigation — ACCEPTED

Accepted invariant:

```text
no collision-free proof != no navigation command
```

If stopping is impossible, `EmergencyMitigatedContact` remains an explicitly non-safe control intent. Physics/collision owns actual contact/ricochet and damage owns consequences.

### Continuous static passage — BEHAVIOR + PERFORMANCE ACCEPTED

Behavior gate on `574a2e98fd7a75ebf562bbfa476fba367fb888d1`:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS
5/5 navigation_trajectory CTest PASS
100% tests passed, 0 failed
Total Test time: 0.24 sec
```

Performance gate on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
scenario                  p95_batch_us
straight_newton                 4.0458
rolled_newton                   6.5235
lateral_newton                  4.1593
elite_aligned                   4.1820
geometry_blocked_roll           6.5133
full_precision_batch8          41.3527
```

Primary result:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

This is far below the `<0.5 ms typical` navigation CPU budget. Decision: **freeze the static continuous verifier**; do not micro-optimize it without contrary live evidence.

The accepted verifier owns one bounded static/extruded passage proof:

```text
cubic Hermite translation
shortest-arc smoothstep attitude
33 pose samples
32 conservative continuous interval proofs
continuous OBB sweep/curve bounds
body-axis linear authority
analytic angular authority
Newtonian vs Elite-assisted slip semantics
```

## Active work — emergency contact severity ranking

The current emergency fallback can keep a command alive and choose the best reachable entry geometry, but it does not yet distinguish a glancing hit from a hard normal impact using contact kinematics.

Next precision rule:

```text
safe candidate first
    -> stop before impact if possible
    -> otherwise rank unavoidable-contact candidates by predicted severity
```

Severity must include at least:

```text
relative normal contact speed
impact-energy proxy
contact geometry / hull attitude
remaining useful forward progress
```

The intended result is that a near-tangential scrape/ricochet is preferred to a perpendicular hit when collision is unavoidable.

## Remaining trajectory work before live integration

1. emergency impact-severity ranking;
2. moving/time-varying obstacle gaps;
3. moving/rotating docking in a relative 6DoF frame with explicit `bottom of ship -> bottom of dock` mating semantics;
4. deterministic `PilotSkillProfile` execution;
5. integration into live `EliteGame` / `EliteServer` / guidance;
6. end-to-end stress/debug acceptance, then retire the legacy route-wide path only after v2 owns live navigation.

## Completion criterion for Navigation v2

Navigation v2 is not complete merely because isolated planners pass. It is complete when the live game can repeatedly demonstrate all of the following under one authoritative state pipeline:

```text
ordinary free flight stays inside CPU/GPU budgets
static obstacles and apertures are avoided/traversed correctly
emergent gaps can be used with real hull orientation
Elite/Newton vehicle authority is respected
unavoidable collisions produce least-severity active commands, not planner shutdown
moving actors/gaps are handled without stale-world failure
stationary/moving/rotating docking converges to the correct mating pose
NPC skill affects execution without corrupting physical truth
manual guidance visualizes the same accepted trajectory/control intent
post-impact/post-detach state replans from actual physics state
no synchronous GPU readback or unbounded N^2 precision path appears
```

At that point the legacy route-wide navigation can be removed from the live path.
