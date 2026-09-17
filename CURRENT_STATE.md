# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — moving / time-varying gap prediction candidate

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
EmergencyContactSeverityScorer
```

### Bounded gaps — ACCEPTED

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

### Emergency passage mitigation — ACCEPTED

Accepted invariant:

```text
no collision-free proof != no navigation command
```

If stopping is impossible, `EmergencyMitigatedContact` remains an explicit non-safe control intent. Physics/collision owns actual contact/ricochet and damage owns consequences.

### Continuous static passage — BEHAVIOR + PERFORMANCE ACCEPTED

Behavior gate on `574a2e98fd7a75ebf562bbfa476fba367fb888d1`: `5/5` trajectory CTest PASS.

Performance gate on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

This is about 8.3% of the `<0.5 ms typical` navigation CPU budget. Decision: freeze the static continuous verifier; do not micro-optimize it without contrary live evidence.

### Emergency contact severity — ACCEPTED

Target-machine gate on `7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e`:

```text
NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS
6/6 navigation_trajectory CTest PASS
100% tests passed, 0 failed
Total Test time: 0.27 sec
```

Accepted scorer is hard-bounded to `<=8` emergency candidates and `<=4` predicted contact witnesses per candidate. It ranks by contact-point relative motion including `omega x r`, moving-surface velocity, peak closing normal speed, normal-energy/momentum proxies and glancing incidence. Exact CCD/TOI/manifold/impulse/ricochet remain physics authority.

## Active candidate — moving / time-varying gap prediction

New component:

```text
MovingGapPredictor
```

Authority:

```text
src/world/navigation/MOVING_GAP_MODEL.md
```

It receives **one already-selected gap pair** from the accepted bounded snapshot stage. It does not perform pair discovery or scene-wide search.

Per boundary it consumes compact motion:

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
one already-selected obstacle pair
```

Sample state publishes:

```text
gap center + gap-center velocity
separation axis / separation rate
clear separation
both boundary surface points
normals toward free space
surface material velocities including omega x r
```

Continuous gap width is not accepted from samples alone. For constant relative acceleration, each interval uses:

```text
center curve deviation from endpoint chord <= |a_rel| * dt^2 / 8
```

plus exact origin-to-chord distance to obtain a conservative whole-interval clear-separation lower bound. A gap that closes and reopens between adjacent samples must therefore fail.

The predictor also continuously bounds `abs(separationAxis dot travel)` so a pair that rotates into a longitudinal/non-passage arrangement fails as `AlignmentLost`.

Candidate code/docs/tests/architecture gate are on public `main`; target-machine compile/behavior evidence is pending.

## Remaining trajectory work before live integration

1. accept `MovingGapPredictor` behavior;
2. combine moving gap states with the ship's continuous P/V/attitude/hull sweep to prove a complete moving passage;
3. moving/rotating docking in a relative 6DoF frame with explicit `bottom of ship -> bottom of dock` mating semantics;
4. deterministic `PilotSkillProfile` execution;
5. integrate into live `EliteGame` / `EliteServer` / guidance;
6. end-to-end stress/debug acceptance;
7. retire the legacy route-wide path only after v2 owns live navigation.

## Completion criterion for Navigation v2

Navigation v2 is complete only when the live game repeatedly demonstrates ordinary flight inside CPU/GPU budgets, static/dynamic avoidance, oriented static and moving gaps, truthful Elite/Newton authority, active least-severity commands through unavoidable collisions, recovery from actual post-impact state, stationary/moving/rotating docking to the correct mating pose, shared guidance/debug truth and intended NPC traffic scaling without synchronous GPU waits or unbounded precision search.
