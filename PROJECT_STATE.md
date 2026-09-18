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

#### 12A-6b1 — ACCEPTED

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

The corrected architecture rerun on the updated `main` is now green:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
```

Combined target-machine evidence for 12A-6b1 is therefore:
- architecture contract PASS;
- navigation_runtime 3/3 PASS.

12A-6b1 remains a candidate until trajectory/map regressions, the full build,
and the unchanged live server self-test are also green.

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


#### 12A-6b1 acceptance

Full target-machine acceptance baseline:

```text
a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d
```

The complete gate passed:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- navigation_map 1/1;
- canonical client/server builds;
- headless live navigation self-test.

The live run retained:
`exact_static_violation=0`,
`replication_error_mps2=0`, and
`canonical_replication_error_mps2=0`.

This accepts the bounded real-runtime chain from dynamic conflict candidates
through MovingGapPredictor and MovingPassageTrajectoryEvaluator as an
observe/prove seam.

#### 12A-6b2 — ACTIVE

Current task is continuous exact-static proof of that same accepted moving
Hermite trajectory. Steering authority remains unchanged until this composition
is proven on the target machine.


#### 12A-6b2 — CANDIDATE / target-machine pending

Candidate baseline:

```text
61f62e9d096542ae52680cf47d6da1c03b0bb8c0
```

The moving-passage evaluator now exports a bounded witness of the exact Hermite
centerline that it accepted. Every one of the 32 intervals carries a continuous
curve-to-chord deviation bound `A*dt^2/8`.

The shared runtime planner combines:
- the same accepted Hermite centerline;
- a conservative radius containing the complete precision hull;
- each interval's curve deviation;
- existing NavigationSpace exact HitVolume segment proof.

Thus a static object between the 33 discrete samples cannot be missed.

A new deterministic runtime fixture requires a dynamically feasible moving
aperture to be rejected by a static HitVolume beam intersecting that same
trajectory.

No authoritative intent changes in this slice. Target-machine verification is
pending.


#### 12A-6b2 first target-machine gate — compile failure

Run on `8c30ebc1c7a724c06113b5b1e4704a7a172b6d93` confirmed the architecture
contract and `navigation_map 1/1 PASS`, then failed compiling the new
same-trajectory witness path.

The defects were interface/wiring errors, not a failed geometric proof:
- `TrajectoryWitness` existed as a type but was not a member of evaluator
  `Result`;
- the MinGW compiler required explicit `NavigationSpace::Vec3d` construction
  for query endpoint assignment.

The later headless self-test from that shell sequence used the previously built
server because the new build had failed. It is therefore excluded from 12A-6b2
evidence.

Corrective baseline `52a5fa9e239d8ea00923cbb65a293cca5863567b` adds the missing result member, makes the
Vec3d conversion explicit, and strengthens the architecture contract to pin the
member. Target-machine rerun is pending.


#### 12A-6b2 — ACCEPTED

Corrected target-machine acceptance baseline:

```text
25dc4369b95b4872d9a70a2d236db5e482cf45e2
```

The full corrected gate passed. The same Hermite centerline that is accepted
against the moving aperture is now continuously enclosed between all 33 samples
and checked against exact static HitVolume geometry, with static blocker
identity retained.

#### 12A-6b3a — ACTIVE

The next bounded slice promotes only a doubly-proven moving passage to planner
steering authority. The authoritative linear acceleration must be the first
sample of the exact accepted Hermite trajectory; no post-proof re-planning is
allowed. This slice first proves the planner/control seam deterministically;
live moving-gap physics/replication evidence remains a subsequent gate.


#### 12A-6b3a — CANDIDATE / target-machine pending

Candidate baseline:

```text
4b117cada10ce416250d917a8bdad2b6b5580ee7
```

The planner now has an explicit opt-in moving-passage authority seam. Precision
evaluation and steering authority are separate policy choices.

A moving passage may supersede the ordinary local hold/avoidance result only
when:
- the dynamic result is not stale;
- the moving Hermite trajectory is feasible through the time-varying aperture;
- the exact same trajectory is continuously safe against static HitVolumes;
- steering authority was explicitly enabled.

The authoritative linear demand is exactly the first acceleration sample of the
proved Hermite maneuver. No second desired-velocity solve is performed.

Runtime regressions cover both veto paths and the positive
map->world->PilotSkillExecutor control path. Live production authority remains
disabled pending deterministic target-machine acceptance.
