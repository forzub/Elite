# Elite — CURRENT STATE

**Updated:** 2026-09-18
**Canonical branch:** `main`
**Current public HEAD:** `f556c36a47c4ecb6ebeb713b41f6443527a16f76`

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — ACCEPTED
- 12A-6a dynamic angular-motion publication — CANDIDATE

## 12A-5 acceptance

Target-machine acceptance run proved:

```text
obstacle_candidate=0
obstacle_conflict=0
exact_obstacle_block=1
adjusted=1

exact_static=1
exact_static_obstacles=17
configured_route_exact_block=1
first_live_probe_blocked=1
exact_static_query=1
exact_static_block=1
exact_static_motion_samples=4109
exact_static_violation=0

progress_m=4790.67
replication_error_mps2=0
canonical_replication_error_mps2=0
```

Therefore stationary CUBE 08:
- is absent from NavigationMap dynamic ownership;
- is owned by exact HitVolume geometry in NavigationSpace;
- causes real adjusted avoidance;
- is physically cleared without exact collision;
- preserves exact authoritative replication truth.

12A-5 is closed.

## 12A-6a candidate

Next requirement is honest motion state for time-varying infrastructure.

Current real fixture:
```text
GUIDANCE DOCK CUBE A
hub-local angular velocity = (0,0,2) deg/s
```

NavigationMap now carries:
```text
DynamicActorInput.angularVelocitySystemRadPerSecond
    -> working-frame vector transform
    -> Candidate.angularVelocityMapRadPerSecond
```

GameSimulation converts the authored hub-visual angular vector through the
shared HubFrameBasis before publication.

The live diagnostic performs an independent NavigationMap query at the rotating
actor and compares expected versus returned map-space angular velocity.

Acceptance requires:
```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error<=1e-12
```

This is input plumbing only. MovingGapPredictor / MovingPassageTrajectoryEvaluator
remain unchanged until the live motion DTO is proven correct.
