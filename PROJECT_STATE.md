# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / moving time-varying gaps  
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

Accepted chain so far:

```text
ConflictHold / explicit aperture / docking corridor
        -> BoundedGapCandidateBuilder (<=8)
        -> OrientedPassageEvaluator
        -> AttitudeReachabilityEvaluator
        -> ContinuousPassageTrajectoryEvaluator
        -> collision-free command
           OR EmergencyPassageMitigator
                -> EmergencyContactSeverityScorer for unavoidable contacts
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

Behavior: continuous contract PASS and `5/5` trajectory CTest PASS.

Target-machine performance on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

The static continuous verifier is frozen unless live evidence later contradicts this result.

### Emergency contact severity — ACCEPTED

Target-machine gate on `7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e`:

```text
NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS
6/6 navigation_trajectory CTest PASS
100% tests passed, 0 failed
Total Test time: 0.27 sec
```

The fixed scorer ranks already predicted unavoidable contacts by rigid-body relative contact-point velocity, peak normal closing speed, coarse normal energy/momentum and incidence angle. Impact severity outranks route progress. Exact CCD/TOI/manifold/impulse/material response stay physics authority.

## Active candidate — `MovingGapPredictor`

Authority:

```text
src/world/navigation/MOVING_GAP_MODEL.md
```

The predictor is intentionally downstream of snapshot pair discovery. It receives one already-selected pair and advances compact boundary motion over a short horizon:

```text
P / V / A
angular velocity
conservative radius
snapshot revision
```

Work is fixed:

```text
33 time samples
32 continuous intervals
```

It publishes time-varying gap center/velocity, width, separation axis/rate and both boundary surface normals/material velocities.

Continuous width uses exact distance to the endpoint relative-position chord minus the quadratic constant-acceleration deviation bound:

```text
|a_rel| * dt^2 / 8
```

so a gap that closes between adjacent samples is rejected. Continuous transverse alignment is also bounded, preventing a pair that rotates into the travel direction from remaining a valid side-by-side passage.

This slice still does **not** prove that the controlled oriented hull can traverse the moving gap. That is the next downstream composition step.

Target-machine architecture/build/behavior acceptance is pending.

## Remaining order

1. accept moving-gap P/V/A prediction + continuous pair proof;
2. moving continuous ship-passage proof against the time-varying gap;
3. moving/rotating terminal docking with explicit mating frames and bottom-to-bottom orientation;
4. deterministic NPC pilot execution/skill;
5. live `EliteGame` / `EliteServer` / guidance integration;
6. end-to-end stress/debug validation;
7. retire legacy route-wide navigation only after v2 owns and passes the live path.

## Final system acceptance

Navigation v2 is complete only when the live runtime demonstrates, within the established budgets, ordinary travel, static/dynamic avoidance, static and moving oriented-gap traversal, truthful Elite/Newton reachability, least-severity unavoidable-contact behavior, recovery from post-impact truth, moving/rotating docking, NPC-scale execution, and guidance/debug driven by the same accepted navigation state.

No synchronous GPU readback or unbounded all-pairs precision search is allowed on the frame path.
