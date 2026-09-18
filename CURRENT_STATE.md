# Elite — CURRENT STATE

**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Last target-machine verified baseline:** `a0efa9190180043b05da3103a5d466d256744935`

> The verified-baseline hash is intentionally not called "current HEAD": a documentation commit changes HEAD by definition. Record the commit that was actually built/tested.

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — ACCEPTED
- 12A-6a dynamic angular-motion publication — ACCEPTED
- 12A-6b live moving-gap / moving-passage composition — ACTIVE NEXT SLICE

## 12A-6a acceptance

Target-machine acceptance was run from:

```text
a0efa9190180043b05da3103a5d466d256744935
```

The full architecture/runtime gate remained green and the live self-test proved the rotating infrastructure DTO/frame path:

```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error=0
```

The previously accepted static-ownership / exact-geometry / replication invariants also stayed green:

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

replication_error_mps2=0
canonical_replication_error_mps2=0
```

The live run also verified that `GUIDANCE DOCK CUBE A` carries its authored rotation through the hub/world/NavigationMap working-frame conversion without corrupting the authoritative execution path.

Therefore 12A-6a is closed.

## Current boundary

The runtime now has proven live compact motion state for time-varying infrastructure, including angular velocity.

What is **not** yet accepted live:

```text
dynamic candidate pair
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> authoritative maneuver selection/execution
```

The predictor/evaluator algorithms are already accepted as isolated Navigation v2 components. 12A-6b must compose them into the Stage-12 live bounded path without adding a second planner, a global all-pairs scan, or any physics-authority bypass.

## Project-state recording rule

Every state-affecting event must be recorded in Markdown before work proceeds to the next slice. This includes:
- new candidate/slice activation;
- target-machine gate result, including failed gates and root cause;
- acceptance/closure;
- architecture or ownership-contract change;
- change of current task or next step.

At minimum keep these synchronized:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- the authoritative stage document (currently `src/game/navigation/STAGE12_END_TO_END.md`).

Record the **last actually verified code baseline**, not a self-invalidating "current HEAD" value.
