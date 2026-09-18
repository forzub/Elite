# Elite — CURRENT STATE

**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Last target-machine verified baseline:** `25dc4369b95b4872d9a70a2d236db5e482cf45e2`

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
- 12A-6b2 exact-static proof of accepted moving trajectory — ACCEPTED
- 12A-6b3a verified moving-passage steering authority seam — CANDIDATE / target-machine pending

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



## 12A-6b2 candidate now on main

Candidate implementation baseline before documentation commits:

```text
61f62e9d096542ae52680cf47d6da1c03b0bb8c0
```

The accepted moving-passage evaluator now publishes a compact witness of the
**same Hermite trajectory it proved**:
- 33 exact center samples;
- 32 continuous centerline-to-chord deviation bounds;
- the conservative radius containing the complete precision hull at any
  orientation.

For each interval, the deviation bound is:

```text
max(|a_i|, |a_i+1|) * dt^2 / 8
```

because Hermite acceleration is linear over the interval and its endpoint
magnitudes bound the acceleration norm continuously.

The runtime then proves each of those 32 chord capsules against
`NavigationSpace::querySegment()`. Exact static HitVolume geometry remains the
obstacle truth; the ship is conservatively contained by its full hull radius
plus the interval curve-deviation bound and the existing static clearance.

New runtime diagnostics distinguish:
- dynamically feasible moving passage;
- static proof attempted;
- number of Hermite intervals proven;
- static obstacle work;
- exact blocking obstacle identity;
- complete static-safe result.

A deterministic regression places a static beam across a dynamically valid
moving passage. The moving-gap/moving-passage proof must stay feasible while the
same-trajectory exact-static proof rejects it.

12A-6b2 remains observe-only. Steering authority is unchanged until this
candidate is accepted on the target machine.


## 12A-6b2 first target-machine gate — COMPILE FAIL

The first target-machine run on `8c30ebc1c7a724c06113b5b1e4704a7a172b6d93`
passed the Stage-12 architecture script and retained `navigation_map 1/1 PASS`,
but both trajectory and runtime builds stopped at compile time.

Root causes were mechanical interface mistakes:
1. `TrajectoryWitness` was declared, but `Result` did not actually contain
   `TrajectoryWitness trajectory {}`, while implementation/tests referenced
   `result.trajectory`;
2. MinGW rejected assignment of a naked braced initializer list to the existing
   `NavigationSpace::Vec3d` object in the static-proof query setup.

Because the full build failed, the subsequent headless-server PASS came from the
previously built executable and is **not** acceptance evidence for 12A-6b2.

Corrections through code/contract baseline:

```text
52a5fa9e239d8ea00923cbb65a293cca5863567b
```

- attach `trajectory` to `MovingPassageTrajectoryEvaluator::Result`;
- use explicit `NavigationSpace::Vec3d { ... }` assignments;
- strengthen the architecture checker to require the result member itself, not
  merely the witness type/field names.

12A-6b2 remains non-authoritative and unaccepted pending target-machine rerun.


## 12A-6b2 acceptance

Corrected target-machine rerun was executed from:

```text
25dc4369b95b4872d9a70a2d236db5e482cf45e2
```

Accepted evidence:

```text
Stage-12 architecture contract   PASS
navigation_trajectory            11/11 PASS
navigation_runtime               3/3 PASS
navigation_map                   retained 1/1 PASS from first 6b2 gate
EliteGame                        BUILD PASS
EliteServer                      BUILD PASS
server --self-test-navigation    PASS
exact_static_violation           0
replication_error_mps2           0
canonical_replication_error_mps2 0
```

This accepts the continuous same-trajectory static proof:
the exact Hermite maneuver accepted against the moving aperture is also bounded
between samples and checked against NavigationSpace exact HitVolume geometry.

## Active 12A-6b3a boundary

The next slice may finally grant steering authority, but only under the exact
gate already proved:

```text
movingPassageFeasible
    && movingPassageStaticSafe
    -> execute movingPassageInitialAccelerationMapMps2
```

No second trajectory or velocity target may be solved after that decision.
Closing gaps, statically blocked moving passages, stale results, unsupported
initial spin, or failed proof continue to fall back to the existing accepted
LocalAvoidance/fail-closed path.

12A-6b3a will first pin the planner/control seam deterministically. A separate
live execution gate is still required before claiming the whole moving-gap
authority chain accepted end-to-end.


## 12A-6b3a candidate now on main

Candidate implementation baseline before documentation commits:

```text
5e96b59ab99a22d3d1fa94cf16577b5ff71c84bb
```

Authority is now deliberately split from precision evaluation:

```text
movingPassage.enabled
    -> may evaluate/prove only

movingPassage.allowSteeringAuthority
    && fresh dynamic result
    && movingPassageFeasible
    && movingPassageStaticSafe
    -> MovingPassageClear
    -> exact first proved Hermite acceleration sample becomes planner intent
```

`allowSteeringAuthority` defaults to false, so existing callers that enable
precision for diagnostics cannot silently change steering.

The authority path returns before the ordinary desired-velocity solve. It uses
`movingPassageInitialAccelerationMapMps2` directly, keeps the proved Hermite
endpoint for diagnostics, and reuses only the existing angular stabilization
semantics.

Deterministic regressions now pin:
- precision enabled but authority disabled -> still observe-only;
- open dynamically + statically proven passage with authority enabled ->
  `MovingPassageClear` and exact sample ownership;
- closing moving gap -> never gains authority;
- dynamically feasible passage blocked by exact-static beam -> never gains
  authority;
- accepted sample survives a non-identity map->world transform and crosses the
  PilotSkillExecutor bridge into ShipControlState without re-planning.

This is not yet the live physics/replication acceptance of moving-passage
authority. 12A-6b3a must first pass the target-machine deterministic/build gate.
