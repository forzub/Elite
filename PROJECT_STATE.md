# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / emergency contact severity + moving-frame continuation  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-TRAJECTORY-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; only the relevant subset is published into a translating working space with stable navigation/travel axes.

Accepted hybrid ownership remains:

```text
CPU: static free-space, portals, corridor search, deterministic precision/local work
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, shared conflict reduction
```

Legacy route-wide navigation remains migration/reference code. `RuckigTrajectorySolver` remains downstream kinematics, not free-space authority.

## Closed stages

### `NAV-V2-MAP-2` — CLOSED

Compact CPU query and GPU dynamic-map performance accepted.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final turn-aware 10k p95 is ~`12 ms`, below the `40 ms` worker-path gate.

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Local horizon + deterministic 16-probe avoidance behavior/scaling accepted. Deliberate `1024 x 17` stress remains below the `<1.0 ms normal peak` budget.

## `NAV-V2-TRAJECTORY-1` — ACTIVE

Accepted chain:

```text
ConflictHold / explicit aperture / docking corridor
        -> BoundedGapCandidateBuilder (<=8)
        -> OrientedPassageEvaluator
        -> AttitudeReachabilityEvaluator
        -> ContinuousPassageTrajectoryEvaluator
        -> collision-free command
           OR EmergencyPassageMitigator when safe motion is too late
```

### Bounded gaps — ACCEPTED

`top8_1024 p95 = 24.0699 us`.

### Emergency passage mitigation — ACCEPTED

Project invariant:

```text
no collision-free proof != no navigation command
```

Stopping is preferred when possible. Otherwise navigation emits an explicit non-safe mitigation command and physics owns actual contact/ricochet.

### Continuous static passage — BEHAVIOR + PERFORMANCE ACCEPTED

Behavior:

```text
5/5 trajectory CTest PASS
continuous contract PASS
```

Target-machine performance on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
straight_newton p95                4.0458 us
rolled_newton p95                  6.5235 us
lateral_newton p95                 4.1593 us
elite_aligned p95                  4.1820 us
geometry_blocked_roll p95          6.5133 us
full_precision_batch8 p95         41.3527 us
```

The eight-query batch is `0.04135 ms p95`, far inside the `<0.5 ms typical` budget. The static continuous verifier is frozen unless live evidence later contradicts this result.

## Current trajectory gap

Emergency mitigation still ranks reachable attitudes primarily by passage geometry. It does not yet explicitly rank unavoidable contacts by:

```text
relative normal contact speed
impact-energy proxy
tangential/glancing incidence
```

That is the current isolated task.

## Remaining order

1. static emergency-contact severity scoring;
2. moving/time-varying obstacle gaps and relative-motion contact prediction;
3. moving/rotating terminal docking with explicit mating frames and bottom-to-bottom orientation;
4. deterministic NPC pilot execution/skill;
5. live `EliteGame` / `EliteServer` / guidance integration;
6. end-to-end stress/debug validation;
7. retire legacy route-wide navigation only after v2 owns and passes the live path.

## Final system acceptance

Navigation v2 is considered complete only when the live runtime demonstrates, within the established budgets, ordinary travel, static/dynamic avoidance, oriented-gap traversal, truthful Elite/Newton reachability, least-severity unavoidable-contact behavior, recovery from post-impact truth, moving/rotating docking, NPC-scale execution, and guidance/debug driven by the same accepted navigation state.

No synchronous GPU readback or unbounded all-pairs precision search is allowed on the frame path.
