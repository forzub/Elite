# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — emergency contact severity candidate

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
```

### Bounded gap — ACCEPTED

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

Accepted continuous verifier:

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

## Active candidate — emergency contact severity

New bounded component:

```text
EmergencyContactSeverityScorer
```

Authority:

```text
src/world/navigation/EMERGENCY_CONTACT_SEVERITY_MODEL.md
```

It consumes already-predicted contact witnesses; it does not perform collision discovery.

Hard bounds:

```text
<= 8 emergency trajectory candidates
<= 4 contact witnesses per candidate
```

For each witness:

```text
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Ranking priority:

```text
no-contact
-> minimum peak normal closing speed
-> minimum normal energy proxy
-> minimum normal momentum proxy
-> more tangential incidence
-> lower geometry deficit
-> higher passage-axis progress
-> deterministic id/index
```

This explicitly prefers a glancing/ricochet-friendly contact over a harder normal impact when collision is unavoidable.

The energy/momentum values are coarse navigation ranking proxies only. Exact CCD/TOI/contact manifold/impulse/material response remain physics authority.

Candidate code/tests are on `main`; target-machine architecture/build/behavior gate is pending.

## Remaining trajectory work before live integration

1. accept emergency contact severity behavior;
2. generate time-varying passage/contact witnesses for moving obstacle gaps;
3. moving/rotating docking in a relative 6DoF frame with explicit `bottom of ship -> bottom of dock` mating semantics;
4. deterministic `PilotSkillProfile` execution;
5. integrate into live `EliteGame` / `EliteServer` / guidance;
6. end-to-end stress/debug acceptance;
7. retire the legacy route-wide path only after v2 owns live navigation.

## Completion criterion for Navigation v2

Navigation v2 is complete only when the live game repeatedly demonstrates ordinary flight inside CPU/GPU budgets, static/dynamic avoidance, oriented gaps, truthful Elite/Newton authority, active least-severity commands through unavoidable collisions, recovery from actual post-impact state, stationary/moving/rotating docking to the correct mating pose, shared guidance/debug truth and intended NPC traffic scaling without synchronous GPU waits or unbounded precision search.
