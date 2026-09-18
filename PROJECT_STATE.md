# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / Stage 12A-6b live moving-gap / moving-passage composition
**Canonical development branch:** `main`

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed
```

Stages 1–11 are accepted. Stage 12 is active.

### 12A-1 — ACCEPTED

Shared `NavigationRuntimePlanner` composition from NavigationSpace/NavigationMap
through LocalHorizon/LocalAvoidance into PilotSkillExecutor.

### 12A-2 — ACCEPTED

Target-machine gate on
`7b4db95788d80c95afb3b57c109c671cb7a41366` passed architecture,
`navigation_runtime 3/3`, EliteGame and EliteServer.

One isolated Active diagnostic NPC is therefore accepted as authoritative
`GameSimulation` ownership using the existing NAV STRESS physical scene and
HitVolume-derived navigation geometry.

### 12A-3 — ACCEPTED

Target-machine acceptance on
`9246530e5eb539e227a1af69461113f2b30ec93a` proved the complete live CUBE 08
chain through authoritative physics and exact sparse/canonical replication.

The working-frame defect is closed: planner map-frame acceleration is
transformed into the Stage-11 world-space control seam. The accepted run stayed
under the configured ~60 m/s target, produced ~5.51 m/s² physically applied
lateral acceleration, retained 129.59 m conservative clearance, progressed
3.50 km in 58.9 s and replicated execution with zero same-tick error.

### 12A-4 — CANDIDATE

The next geometry slice moves persistent static precision into
`NavigationSpace` using the authoritative HitVolume-derived
`NavigationObstacle` OBBs.

`NavigationSpace` now owns exact static point/segment tests alongside its
coarse region/portal topology. `LocalAvoidance` must prove both the nominal
bounded segment and every adjusted probe against that geometry, including when
the dynamic `NavigationMap` reports no conflict.

A deterministic regression proves the intended narrow-passage rule: overlapping
conservative bounding spheres may not erase a genuine OBB aperture. A fitting
agent crosses; an oversized envelope fails.

The live NAV STRESS fixture publishes the same HitVolume OBBs in the
NavigationMap working frame after authoritative transforms/HitVolume rebuild.
The server self-test requires publication, non-zero exact-static query work and
an observed nominal static block, so live PASS cannot be satisfied by merely
storing unused OBB data.

For this intermediate gate, static objects remain in the existing
NavigationMap sphere candidate set as well. Removing those static spheres is a
separate post-acceptance cleanup so broadphase ownership and precision geometry
are not changed in the same acceptance step.


#### 12A-4 corrected gate after first target run

First target-machine run proved exact static publication and live consumption,
but exposed three independent issues:

1. one new local test fixture violated the existing dynamic-candidate invariant;
2. valid corridor portal centres were rejected by generic region-boundary
   envelope clearance;
3. conservative sphere clearance was still incorrectly treated as exact-static
   acceptance truth.

The fixture is fixed. Corridor-selected portal endpoints carry an explicit
boundary-proof exception into the nominal precision query. Actual authoritative
fixed-step motion is now swept through exact HitVolume geometry, and that swept
proof replaces enclosing-sphere clearance as the safety acceptance condition.

Conservative spheres remain useful for broadphase diagnostics until their static
duplication is removed in the post-12A-4 ownership cleanup.


### 12A-4 — ACCEPTED

Accepted target-machine run on
`8f5801b9e26cfbb8e4e5e3174587a900292e8394` proved:
- exact static OBB publication and query;
- correct portal-boundary behavior;
- exact-static local avoidance;
- 3431 swept authoritative-motion samples with zero exact-static violations;
- successful CUBE 08 avoidance and >3.5 km progress;
- exact sparse/canonical replication equality.

### 12A-5 — CANDIDATE

Static/dynamic ownership is being separated.

Stationary infrastructure is no longer duplicated into NavigationMap as
conservative spheres. Its navigation authority is NavigationSpace exact
HitVolume geometry.

Only time-varying infrastructure remains in the dynamic broadphase until the
moving/rotating exact-geometry stage.

The live gate specifically requires stationary CUBE 08 to be absent as a
NavigationMap candidate/conflict while still being identified as the exact
static blocker that causes the adjusted maneuver.


#### 12A-5 first live ownership run

The first ownership-cleanup live run correctly removed stationary CUBE 08 from
NavigationMap:

```text
obstacle_candidate=0
obstacle_conflict=0
dynamic_queries=6000
max_dynamic_candidates=1
```

Exact static geometry remained healthy and actual swept motion had zero
HitVolume violations.

The run did not produce an exact-static block because the proving actor traveled
about 3.1 km before reaching CUBE 08 and accumulated about 500 m of natural
lateral drift. The real OBB therefore lay off the bounded nominal segment by the
time it mattered.

The proving fixture is corrected rather than weakening ownership:
- initial obstacle lead reduced to 1300 m, inside the first local horizon;
- start coordinates are derived from the obstacle coordinates;
- publication-time query proves the configured centerline intersects the exact
  CUBE 08 HitVolume;
- self-test fails fast if that proof is absent.

Stationary-sphere duplication remains forbidden.


#### 12A-5 second live run

Run on `54bd19ef647f0eca8dcc738be4342052ee164683` confirmed:
- stationary CUBE 08 is absent from NavigationMap;
- configured exact start->goal route intersects CUBE 08;
- exact static layer remains active and collision-free;
- ship reaches the final goal.

However the runtime planner never reports a nominal exact-static block.

The visual/tactical frame conversion was re-audited against
`HubFrameBasis.h` and `HubNavigationFrame.h`; the axis permutation is
correct.

The next candidate therefore adds:
- a live-scale runtime regression independent of GameSimulation;
- an exact first-live bounded-segment probe using current ship position,
  actual goal, current physical horizon and the same envelope as planner;
- fail-fast diagnostics distinguishing live-geometry mismatch from
  planner-composition loss.


### 12A-5 — ACCEPTED

Accepted live run proved stationary/dynamic ownership separation end to end:
CUBE 08 stayed out of NavigationMap, exact OBB avoidance remained authoritative,
physical motion had zero exact-static violations, and sparse/canonical execution
replication matched exactly.

### 12A-6a — ACCEPTED

Target-machine acceptance on
`a0efa9190180043b05da3103a5d466d256744935` proved that live rotating
infrastructure publishes angular velocity through the authoritative frame chain
into NavigationMap without regressing the already accepted Stage-12 behavior.

Accepted live evidence:

```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error=0

obstacle_candidate=0
obstacle_conflict=0
exact_obstacle_block=1
adjusted=1
exact_static_violation=0

replication_error_mps2=0
canonical_replication_error_mps2=0
```

The deterministic live fixture remains `GUIDANCE DOCK CUBE A` with authored
hub-local angular velocity `(0,0,2) deg/s`. The run verifies that the
hub/world/NavigationMap working-frame conversion preserves that motion exactly.

12A-6a closes the live compact motion DTO/frame boundary. It does **not** yet
prove live moving-gap or moving-passage behavior.

### 12A-6b — ACTIVE

The next slice composes the already accepted precision components into the
bounded Stage-12 runtime path:

```text
NavigationMap relevant dynamic candidates
    -> selected moving obstacle pair / gap
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> bounded authoritative maneuver result
    -> NavigationRuntimePlanner
    -> mapIntentToWorld(...)
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> authoritative physics / same-tick replication
```

The live gate must prove actual predictor/evaluator consumption and a real
maneuver/feasibility consequence. DTO publication alone is no longer sufficient.

Existing ownership remains frozen unless live evidence proves a defect:
- stationary infrastructure -> NavigationSpace exact HitVolume geometry;
- moving/rotating infrastructure -> NavigationMap dynamic publication;
- no global all-pairs moving-gap scan;
- no presentation-side second planner;
- navigation never writes authoritative P/V directly.

#### 12A-6b1 — CANDIDATE / target-machine pending

Candidate code through `616f5b868795439fb42308d2c2d13ffc87218cba`
composes the accepted moving precision components into the shared runtime planner
without granting them steering authority yet.

The bounded chain is now:

```text
NavigationMap::QueryResult
    -> nominal dynamic conflict identity
    -> BoundedGapCandidateBuilder (hard <= 8)
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> runtime feasibility diagnostics
```

The runtime regression set includes:
- an open translating gap that must become continuously feasible for the test
  hull;
- a gap that closes inside the horizon and must fail before moving-passage
  acceptance.

`MovingPassageTrajectoryEvaluator::Result` now publishes the first linear
acceleration sample from the exact Hermite segment that it verified. This avoids
a future authority step accepting one curve and then executing a separately
re-derived approximation.

12A-6b1 deliberately remains observe/prove only. The current missing safety
composition is exact-static proof of the **same moving trajectory** against
NavigationSpace HitVolume geometry. Until that exists, LocalAvoidance/exact
static remains the only steering authority.

#### 12A-6b1 first target-machine run — PARTIAL PASS

Run from `6badb5f74ea65900a38a48854d66f16f34127a89` produced:

```text
navigation_runtime_control    PASS
navigation_runtime_planner    PASS
navigation_replication_truth  PASS
3/3 runtime tests PASS
```

The only reported failure in the submitted gate was the architecture script:

```text
[FAIL] exact obstacle geometry must link in isolated and production NavigationSpace targets
```

This was a checker-format defect. The CMake dependency still linked and the
runtime executable was built successfully; the checker searched for the exact
text `PUBLIC EliteNavigationGeometry`, while the newly added trajectory link
changed that formatting.

The CMake layout has been restored and the architecture checker made
whitespace-stable. No navigation behavior or safety contract was weakened.

12A-6b1 remains pending the corrected architecture rerun plus the remaining
trajectory/map/build/live regression gate.

## State-recording protocol

A project-state transition is not considered recorded until the Markdown context
is synchronized.

For every candidate activation, failed gate/root-cause finding, acceptance,
ownership/architecture contract change, or next-task change, update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- the authoritative stage document, currently
  `src/game/navigation/STAGE12_END_TO_END.md`.

Record the **last target-machine verified baseline** rather than a field named
"current HEAD": committing documentation changes HEAD and would immediately make
such a value stale.
