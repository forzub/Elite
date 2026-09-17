# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / local avoidance performance + first trajectory precision geometry  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-LOCAL-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` is downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, deterministic precision/local work
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit is specified in `src/world/navigation/PURSUIT_HORIZON.md`; runtime pursuit remains later.

## World economy / traffic direction

The formalized game-design direction for trade flows, causal NPC traffic, civilized navigation infrastructure, taxes/fees/services, insurance and the core-vs-frontier scale contrast remains recorded in:

```text
WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md
```

Reference observations from `Objects in Space` remain separate in:

```text
Notes/OBJECTS_IN_SPACE_ECONOMY_REFERENCE.md
```

This design material does not change the active Navigation v2 implementation milestone.

## `NAV-V2-MAP-2` — CLOSED

`NavigationMap` exposes compact candidate products and hides backend/GPU storage. Accepted target-machine evidence includes CPU compact queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

Mass/broadphase actor geometry intentionally remains center P/V/A + radius + conservative swept sphere. That representation is safe and cheap but may reject valid tight oriented passages.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final target-machine turn-aware implementation on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero-turn p95    8.4498 ms
open_10k turn-aware p95  12.0072 ms
hub_10k  zero-turn p95    8.4125 ms
hub_10k  turn-aware p95  11.9065 ms
turn portals examined    329,660
```

Static turn search is closed; do not spend more work there without new runtime evidence.

## `NAV-V2-LOCAL-1` — horizon + avoidance behavior ACCEPTED

Target-machine compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

Fresh `LocalAvoidancePlanner` behavior gate on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

Accepted local reference:

```text
nominal ConflictHold
    -> bounded 15 deg x 8 + 30 deg x 8 target fan
    -> same-region / same-publication static proof
    -> compact dynamic recheck
    -> AdjustedClear or fail closed
```

Current-P/V/A head-on/crossing remains fail-closed until trajectory-aware feasibility exists.

## Active local gate — multiplied avoidance cost

Dedicated harness:

```text
benchmarks/navigation_local_avoidance/
```

It measures nominal clear, first-probe adjustment, all-static rejection, and all-dynamic rejection at 16/64/256/1024 compact candidates.

The `1024` case is deliberate stress. Performance remains pending target-machine evidence.

Local CPU budgets remain:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## First trajectory precision candidate — oriented passage / obstacle gap

Architecture authorities:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
```

Candidate code:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h
src/world/navigation/trajectory/OrientedPassageEvaluator.cpp
```

The first slice is intentionally tiny and backend-neutral. Given an already-selected passage and body-local OBB proxy, it performs constant-size projection math to answer whether the oriented hull fits the passage cross-section at the supplied pose.

This supports three passage sources through one abstraction:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

New project invariant: **two nearby obstacles may form a positive passage between them**. If ordinary side-step avoidance fails because speed is high and remaining distance is short, that does not by itself prove collision is unavoidable. A bounded precision fallback may test the free gap between relevant obstacles and, if the oriented hull fits, pass the candidate to full 6DoF feasibility.

### Performance protection

Do not run precision oriented geometry for every actor and do not perform an unbounded all-pairs search.

Intended flow:

```text
accepted cheap broadphase/local avoidance
        |
        +-- normal safe result -> done
        |
        +-- ConflictHold / explicit narrow aperture / docking
                |
                v
        bounded local gap-candidate extraction
        primary conflict + local adjacency only
        initial design target <= 4-8 candidates
                |
                v
        O(1) OrientedPassageEvaluator per candidate
                |
                v
        only plausible fits -> expensive continuous 6DoF proof
```

The exact bounded gap-candidate builder is not implemented yet. A naive `N x N` obstacle-pair scan on the frame path is explicitly rejected.

### Pending behavior gate

Fixtures now pin:

```text
flat hull + flat slot / correct attitude -> Fits while sphere rejects
same hull rolled 90 degrees              -> rejected
two-obstacle gap / wide attitude         -> rejected
two-obstacle gap / thin rolled attitude  -> Fits
off-center hull                           -> rejected
degenerate frame                          -> fail closed
```

Target-machine architecture/build/behavior evidence is still required before acceptance.

## Planned trajectory/control/docking fidelity

After the current gates the precision layer continues with:

```text
oriented hull + attitude / angular state
body-axis or thruster acceleration authority
rotation time before braking / vector change
Elite-assisted versus Newtonian free flight
rotate-then-thrust / flip-and-burn
continuous swept-body passage proof
moving/time-varying obstacle gaps
terminal 6DoF docking against stationary/moving/rotating ports
explicit bottom-to-bottom mating-frame orientation
NPC PilotSkillProfile execution
```

Docking remains a relative pose-and-motion problem, not a center-point target. A rotating port requires the ship to match the predicted port frame and its tangential/angular motion at capture.

Poor NPC skill is modeled through delayed/under-damped execution, not by falsifying geometry or vehicle dimensions.

## Next order

1. run the local avoidance benchmark gate on the target machine;
2. run `check_navigation_trajectory_passage.py` and `tests/navigation_trajectory/run_mingw64.sh`;
3. accept/fix the oriented fit candidate from real build/test evidence;
4. implement a **bounded** gap-candidate extractor with no all-pairs frame-path scan;
5. add rotation-time/thrust-aware continuous 6DoF passage/head-on feasibility;
6. extend the same pose/sweep machinery into moving/rotating docking;
7. add pursuit/receding-intercept later;
8. integrate accepted NavigationWorld products into live game/server only after the isolated gates are green.

Do not add vehicle velocity/braking/traffic/pursuit state to persistent `NavigationSpace` static cost.
