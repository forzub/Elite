# Elite — CURRENT STATE

**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Last target-machine verified baseline:** `a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d`

> The verified-baseline hash is intentionally not called "current HEAD": a documentation commit changes HEAD by definition. Record the commit that was actually built/tested.

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — ACCEPTED
- 12A-6a dynamic angular-motion publication — ACCEPTED
- 12A-6b live moving-gap / moving-passage composition — ACTIVE
- 12A-6b1 runtime moving-precision observe/prove seam — ACCEPTED
- 12A-6b2 exact-static proof of accepted moving trajectory — ACTIVE

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

## 12A-6b1 candidate now on main

Implementation candidate through code/contract commit
`616f5b868795439fb42308d2c2d13ffc87218cba` adds an **observe/prove** precision seam:

```text
real NavigationMap bounded candidate result
    -> nominal dynamic conflict identity
    -> BoundedGapCandidateBuilder (hard cap <= 8)
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> feasible/not-feasible diagnostics
```

Two deterministic runtime fixtures are pinned:
- an open translating gap must reach continuous moving-passage feasibility;
- a gap that closes inside the horizon must fail in prediction and never reach passage acceptance.

The moving-passage evaluator now also publishes the first acceleration sample from the exact Hermite trajectory it proved, so the next authority step can execute the same trajectory rather than solve a second similar curve.

**Important:** 12A-6b1 is intentionally non-authoritative. A feasible moving passage does not yet replace the accepted LocalAvoidance command. Before steering authority is enabled, that moving trajectory must also be composed with exact-static HitVolume safety. This prevents a dynamically valid gap trajectory from cutting through stationary geometry.

Target-machine partial verification on `6badb5f74ea65900a38a48854d66f16f34127a89`:

```text
navigation_runtime_control    PASS
navigation_runtime_planner    PASS
navigation_replication_truth  PASS
3/3 runtime tests PASS
```

The architecture script failed before runtime execution with:

```text
[FAIL] exact obstacle geometry must link in isolated and production NavigationSpace targets
```

Root cause: the checker matched the literal CMake text
`PUBLIC EliteNavigationGeometry`; adding `EliteNavigationTrajectory` changed
the line formatting but not the actual link graph. The successful build/link of
`navigation_runtime_planner_tests.exe` confirms this was not a linker/runtime
defect.

The CMake formatting was restored and the checker made whitespace-stable.
The corrected target-machine rerun on the updated `main` passed:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
```

Together with the already green runtime suite, 12A-6b1 now has:
- architecture contract PASS;
- navigation_runtime 3/3 PASS.

Remaining before acceptance: trajectory regression, navigation_map regression,
full game/server build, and unchanged live server self-test.

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


## 12A-6b1 acceptance

Full target-machine gate completed from:

```text
a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d
```

Accepted evidence:

```text
architecture contract              PASS
navigation_runtime                 3/3 PASS
navigation_trajectory             11/11 PASS
navigation_map                     1/1 PASS
EliteGame                          BUILD PASS
EliteServer                        BUILD PASS
headless navigation self-test      PASS
exact_static_violation             0
replication_error_mps2             0
canonical_replication_error_mps2   0
```

The live self-test preserved exact-static ownership, rotating dynamic publication,
physical execution and same-tick replication while the new moving precision seam
remained non-authoritative.

Therefore 12A-6b1 is accepted.

## Active 12A-6b2 boundary

The next candidate must prove the **same Hermite trajectory** accepted by
`MovingPassageTrajectoryEvaluator` against persistent exact-static
`NavigationSpace` / HitVolume geometry.

Requirements:
- bounded work only;
- continuous-between-samples proof, not point sampling;
- no second independently re-derived trajectory;
- fail closed on static collision;
- keep moving precision observe-only until this static proof is target-machine green.

