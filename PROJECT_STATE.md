# Project State

**Updated:** 2026-09-19 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / Stage 12 architecture hardening — state/API isolation
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
5e96b59ab99a22d3d1fa94cf16577b5ff71c84bb
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


#### 12A-6b3a — ACCEPTED

Target-machine acceptance baseline:

```text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
```

The explicit moving-passage steering-authority seam is accepted. Precision
evaluation by itself remains observe-only; authority requires explicit opt-in,
fresh dynamic state, moving-passage feasibility and same-trajectory exact-static
safety. The executed linear intent is the exact first acceleration sample from
the proved Hermite trajectory.

#### 12A-6b3b — ACTIVE

Current work is the live end-to-end authority fixture: demonstrate that
`MovingPassageClear` drives real physics through PilotSkillExecutor and is
replicated as same-tick server truth while exact-static collision safety remains
green.


## Future roadmap — galactic strategic navigation

A dedicated future contract now exists:

`src/game/navigation/GALACTIC_ROUTE_PLANNING.md`

It separates interstellar strategic routing from station/local obstacle planning.
The future GalacticRoutePlanner will combine jump topology with vehicle/drive
capability, fuel and reserve constraints, gravity-aware trajectory validation,
timing/ephemerides, hazards and replanning.

Execution stays shared:
- manual mode visualizes the accepted route/leg;
- automatic mode executes the same accepted product.

Current Stage-12 work is unchanged and remains focused on live local
NavigationWorld authority, physics, replication and later common manual guidance.


#### 12A-6b3b — CANDIDATE / target-machine pending

Candidate baseline before documentation commits:

```text
d5e1f990aed38ded7a517d719e935b6b6e763d4d
```

The deterministic Stage-12 live scene now contains a genuine two-boundary
moving aperture in addition to the existing exact-static CUBE 08 and rotating
infrastructure probes.

The moving boundaries are physical hub-attached scene objects with deterministic
map-frame translation. They are dynamically owned by NavigationMap and excluded
from the frozen exact-static NavigationSpace snapshot.

The live planner consumes real Cobra hull geometry and real propulsion/angular
authority. The server gate requires the expected pair to produce
`MovingPassageClear`, cross PilotSkillExecutor, drive real authoritative
acceleration, replicate at the same server tick, and physically pass the gap
without exact-static violation.


#### 12A-6b3b first target-machine live gate — FAILED FIXTURE

Target-machine failed candidate: 4df5a4420f4712b0794e6df3940d45e99e6ad363

Observed live evidence:
- architecture contract PASS;
- navigation_runtime 3/3 PASS;
- navigation_trajectory 11/11 PASS;
- EliteGame / EliteServer BUILD PASS;
- moving_gap_pair=1;
- moving_gap_kinematics=1;
- moving_precision=1;
- moving_passage_feasible=0;
- moving_passage_static_safe=0;
- moving_passage_authority=0;
- exact_static_violation=0.

Root cause is fixture design, not the accepted moving-passage algorithm.
The live aperture was authored near z=-2500 while the ship starts near z=-6200.
With duration=30 s the exact Hermite segment must cover about 3.7 km while
ending near the 60 m/s local speed cap. Its terminal acceleration is therefore
about -16.7 m/s^2 along travel, far beyond the Cobra's real ~2 m/s^2 reverse
manoeuvre authority. The evaluator correctly rejects it before static proof.

The same old fixture is also behind CUBE 08 on the nominal route. Therefore,
even if its propulsion envelope were loosened, the same-trajectory exact-static
proof should subsequently reject the Hermite path through CUBE 08.

Correction: move the deterministic moving aperture ahead of the ship but before
CUBE 08, preserving the real Cobra capability and the existing CUBE 08 static
gate. The intended order becomes:
moving passage live authority -> physical gap crossing -> CUBE 08 exact-static
avoidance -> same-tick replication, with no weakening of either proof.


## 12A-6b3b corrected live candidate

Corrective code/contract baseline before documentation commits:

```text
89ead2a4a256052817023f2fbf01ce9a9ca683e0
```

Corrections after the failed `4df5a442...` target run:
- relocate the translating moving aperture from visual z=-2500 to z=-5700,
  about 500 m ahead of the lab spawn and before CUBE 08;
- keep the real 30 s Hermite horizon and the Cobra's real vehicle authority;
- preserve CUBE 08 as the later independent exact-static obstacle;
- split acceptance ordering into:
  1. active MovingPassageClear + real physical acceleration,
  2. same-tick sparse/canonical replication of that active epoch,
  3. physical moving-gap plane crossing,
  4. subsequent CUBE 08 exact-static avoidance/progress;
- expose the last moving-passage evaluator status and required forward/reverse/
  lateral/vertical peak accelerations plus sample/continuous clearance in the
  server diagnostic line.

The corrected fixture no longer asks the Cobra to cover ~3.7 km in 30 s and
then brake at ~16-17 m/s^2 with 2 m/s^2 reverse authority. At the authored
~500 m initial range, the expected Hermite endpoint demands are inside the
physical manoeuvre envelope, while the same trajectory ends before CUBE 08 so
exact-static proof can succeed independently.

Target-machine acceptance is still pending.


## 12A-6b3b corrected fixture target-machine gate — EXACT-STATIC PHYSICAL VIOLATION

Target-machine baseline:

```text
69d8098d795c25b24b778b5229644244ae1384cb
```

Passed before the live failure:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- canonical EliteGame / EliteServer build.

The authoritative live run then failed correctly on physical exact-static sweep:

```text
exact_static_violation=1
violation_entity=24
proving_obstacle_entity=24
planner_status=2
```

The ship entered the exact HitVolume of the legacy single CUBE 08 proving
obstacle after the moving-gap fixture was moved earlier in the route. This is
not accepted and 12A-6b3b remains open.

The next correction changes the static proving geometry, not vehicle capability
or collision acceptance: replace the single arbitrary block as the terminal
acceptance obstacle with a deterministic exact-HitVolume slit/tunnel gate. The
goal remains beyond it, so the planner must guide the real hull through authored
empty space rather than merely skirt one isolated cube.

The general contract remains vehicle-agnostic: runtime planning consumes the
current vehicle's physical capability and hull dimensions; no Cobra-specific
acceleration constants may be introduced into the navigation algorithm.


## 12A-6b3b slit-tunnel candidate

Candidate code/contract baseline before documentation commits:

```text
d05c97905f3df8e42980a88fbc216e03c7f8dbab
```

The single isolated CUBE 08 terminal obstacle is no longer the acceptance
geometry. The live static proving fixture is now a real exact-HitVolume tunnel:

```text
six GuidanceDockCube physical blocks
two rows x three blocks
tunnel depth: 900 m
horizontal slit height: 140 m
slit half-width: 540 m
slit center: {975, -1180, -4500} map/visual
straight authored route y: -1300
```

The slit is intentionally offset by 120 m from the original straight route.
The lower-middle wall block retains the `NAV STRESS CUBE 08` identity so the
existing direct-route exact-block witness still has a stable physical blocker.

NavigationSpace now publishes two coarse free-space regions connected only by
`NavigationRuntimeLabSlitPortalId` at the slit center. The route planner must
therefore publish that real portal as its static steering waypoint.

Fixture validation proves both facts independently:
- direct start->goal line is exact-static blocked by the representative wall;
- start->portal->tunnel-exit is exact-static traversable for the current ship's
  conservative physical envelope plus the existing 10 m static clearance.

Live physical acceptance no longer means merely "passed the obstacle plane".
The fixed-step exact sweep interpolates the actual ship crossing at the tunnel
center plane and requires the full conservative hull + static clearance to fit
inside the authored slit. The resulting crossing position and remaining margin
are retained in diagnostics.

The moving-gap authority gate remains first in the same run. Final ordered proof:

```text
live moving gap
 -> MovingPassageClear
 -> PilotSkillExecutor / real physics
 -> same-tick replication
 -> moving-gap plane crossed
 -> NavigationSpace slit portal selected
 -> exact-static tunnel crossing with positive margin
 -> continued route progress
 -> zero exact-static violation
```

Vehicle capability remains generic. The navigation algorithm reads the
descriptor/runtime capability of the current vehicle; no Cobra-specific
acceleration value is embedded in planner logic.


## 12A-6b3b slit candidate target-machine gate — FIXTURE VALIDATION FAILURE

Target-machine baseline:

```text
777716f52cdde3222dd875d93e91fe00df9859f1
```

Green before the live fail:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- EliteGame / EliteServer build.

Fail-fast evidence:

```text
slit_exact_open=0
slit_exact_obstacles_examined=22
[FAIL] authored exact-static slit tunnel does not admit the current ship envelope
```

Root cause is the fixture proof's region-boundary semantics. The slit portal
center lies exactly on the shared NavigationSpace region boundary. Ordinary
`querySegment` validates its endpoint through `queryPoint`, where geometric
clearance to the region boundary is zero, so it rejects the legal portal point
before exact OBB aperture geometry can be accepted.

The runtime planner already has the correct selected-portal exception:
`allowEndOnStartRegionBoundary`. The fixture proof must use the same contract
for approach and reverse-exit segments rather than treating the portal center
as an ordinary interior point.

Architecture refinement from user review: a portal/tunnel is not accepted merely
because the hull center intersects the opening. A traversable finite-depth
passage needs an entry/capture frame. Before crossing the entry plane, automatic
execution must align:
- velocity direction with the oriented portal/tunnel normal;
- vehicle forward axis with that normal when the passage requires pose
  alignment;
- lateral/vertical position and cross-track velocity inside the capture
  envelope.

For a straight tunnel, transit keeps that entry axis through the exit. This must
be a generic portal traversal contract, not a NavigationRuntimeLab/Cobra special
case.


## 12A-6b3b oriented tunnel-capture candidate

Candidate code/contract baseline before documentation commits:

```text
b73cc129ce32cae1147165bf79b29f20b83ef04c
```

The target-machine failure on `777716f...` was a fixture-validation failure,
not a failed flight: `slit_exact_open=0` came from treating a legal portal
boundary as an ordinary region-interior point.

The correction now separates topology from collision truth and implements the
entry behavior required for finite-depth passages:

```text
APPROACH REGION
    -> staging/capture point
    -> ENTRY PORTAL (oriented normal + capture limits)
    -> TUNNEL REGION
    -> EXIT PORTAL
    -> DEPARTURE REGION
```

Before the entry plane can be accepted, the live actor must satisfy:
- hull position inside the passage capture envelope;
- velocity vector within 5 degrees of the entry normal;
- lateral speed <= 1 m/s;
- hull forward axis within 5 degrees of the entry normal.

The planner actively commands angular alignment using the current vehicle's
angular capability. It stages/brakes before the entrance, then releases
longitudinal transit only after capture is ready.

The live physical sweep now uses exact HitVolumes only; NavigationSpace region
boundaries remain topology and cannot produce a fake physical collision.

New regressions pin:
- route-direction portal normals, including reverse traversal;
- exact-obstacle-only sweep across a virtual region boundary;
- portal capture holding a misaligned hull;
- release to PortalTransit only after velocity + hull-axis alignment.

Target-machine acceptance is pending. The last accepted Stage-12 baseline
remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`; the later
`777716f...` run is retained as failed evidence, not acceptance.


## 12A-6b3b target-machine gate on e432363 — MOVING PASSAGE REJECTED

Target-machine baseline:

```text
e432363717d6bff49aaef35d34fac61cfe802903
```

Green evidence:
- Stage-12 architecture contract PASS;
- navigation_space 1/1 PASS;
- navigation_runtime 3/3 PASS;
- navigation_trajectory 11/11 PASS;
- canonical EliteGame and EliteServer build PASS;
- static slit fixture is now physically valid: `slit_exact_open=1`;
- physical exact-static sweep stayed clean: `exact_static_violation=0`;
- both intended moving actors were published and their kinematics verified.

The live run did not progress because the current test required the ordinary
free-space encounter to become a precision MovingPassage between the two moving
actors. The evaluator correctly rejected that geometry:

```text
moving_gap_pair=1
moving_gap_kinematics=1
moving_passage_feasible=0
moving_eval_status=2   # GeometryBlocked
moving_sample_clearance_m=-110.435
moving_passage_authority=0
conflict_hold=1
progress_m=0.0343542
simulated_s=120
```

This is not a tunnel/collision failure. It exposes a routing-policy problem:
when free space exists around a pair of obstacles, normal transit should not be
forced to prove a narrow moving passage between them.

Architecture decision from user review:
- default local transit becomes bounded line-of-sight / visibility steering;
- always start from the direct A->B direction;
- look only through the current physical horizon, sized by braking/turning and
  safety margin;
- if that corridor is blocked, increase the deflection only until a hull-sized
  safe corridor is found;
- on every receding-horizon update, retry direct A->B first and immediately
  return to it when visibility is restored;
- dynamic actors use their predicted swept volume inside the same bounded
  horizon;
- MovingPassage remains a precision mode only when passage is topologically or
  semantically mandatory: authored portal/tunnel, docking mouth, ravine, narrow
  gate, etc.

Current task changes from "force live MovingPassage authority through the free
moving pair" to "prove bounded visibility steering around the free moving pair,
then use the already-authored oriented portal capture for the mandatory tunnel."


## 12A live bounded-visibility steering candidate

Candidate code/contract baseline before documentation commits:

```text
e2584b4b798baa661d14ab1fb8f68789f76c99b9
```

Implemented after the `e432363...` live GeometryBlocked result:
- LocalAvoidance still tests the direct accepted target first on every update;
- the physical horizon remains bounded by latency + braking distance +
  turn-distance allowance + safety margin;
- when direct visibility is blocked, angular search now expands
  `15 -> 30 -> 45 -> 60 -> 75 deg` and stops at the first corridor that passes
  both exact-static and dynamic-horizon checks;
- the selected target is pass-through only; there is no persistent alternate
  route, so the next update automatically returns to direct A->B as soon as the
  corridor clears;
- live free-space MovingPassage authority is disabled. The precision
  MovingPassage contracts/tests remain intact for explicit mandatory gaps;
- live diagnostics now retain `visibilityBypassSeen`,
  `visibilityBypassActive`, `visibilityDirectRecoveredSeen` and maximum
  selected deflection;
- same-tick sparse/canonical replication is captured while the visibility
  bypass is actively executing;
- the same authoritative run must then pass the moving pair, recover direct
  visibility, perform oriented tunnel capture/alignment, traverse the exact
  HitVolume tunnel, and keep `exact_static_violation=0`.

A new local regression proves that a fixture requiring more than the old
15/30-degree fan finds a wider safe direction and then immediately resumes
direct A->B when the blocker disappears.

Target-machine acceptance is pending.


## Control-law-specific maneuver contract — candidate

Candidate code/architecture baseline before mandatory state-doc commits:

```text
8e707f8376099ce522cc300b293a33cf1d8f484c
```

New authority:
`src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

Two local flight laws are now explicit in maneuver selection:

```text
Assisted / Elite-classic
    cheap passage proxy: oriented OBB ("brick")
    continuous truth: swept oriented hull
    velocity/forward coupling remains controller policy

Newtonian
    coarse arbitrary-rotation / flip free-volume proxy:
        conservative rotation sphere
    constrained passage truth:
        time-varying oriented hull / swept OBB
    strong braking:
        rotate tail toward velocity -> main-engine burn
```

The Newtonian sphere is not physical collision geometry and must never replace
exact oriented HitVolume truth. It exists only as a conservative test for free
rotation/flip space.

`ManeuverDecisionController` candidates now carry a control-law requirement:
`Any`, `AssistedOnly`, or `NewtonianOnly`. The selector filters
incompatible candidates before doctrine ranking. Candidate families now include
extended visibility recovery, backtrack, Newtonian flip-and-burn and reverse
escape.

The ordinary local visibility fan remains:

```text
direct -> 15 -> 30 -> 45 -> 60 -> 75 deg
```

75 degrees is an ordinary progress-preserving search boundary, not a vehicle
capability limit.

When all ordinary probes fail, `LocalAvoidancePlanner` publishes
`ordinarySearchExhausted=true`; `NavigationRuntimePlanner` publishes
`ordinaryVisibilitySearchExhausted=true`. This is a recovery-escalation
signal:

```text
>75 deg escape turn
backtrack / previous viable portal
safe stop/brake
Assisted brake-turn / controller-supported reverse
Newtonian coast-turn / drift / side-on pass / flip-and-burn / reverse escape
mandatory precision passage
contact-expected emergency
```

A provisional braking hold may protect the current control cycle while higher
selection occurs, but fan exhaustion is no longer architecturally equivalent to
"stay stopped forever".

The shared production library `EliteManeuverDecision` is linked by both
EliteGame and EliteServer. Architecture/runtime tests now pin control-law
filtering and ordinary-search escalation.

Target-machine validation is pending. Last target-machine accepted Stage-12
baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## Accepted-segment execution / event-driven replanning candidate

Candidate code/architecture baseline before mandatory state-doc commits:

~~~text
d7b298f9fbc4cacd34eef912a96b324b894b197e
~~~

New authority:
`src/game/navigation/TRAJECTORY_EXECUTION_REPLAN_MODEL.md`

New production policy:
`NavigationExecutionReplanPolicy.{h,cpp}`

Core invariant:

~~~text
execution tick != planning tick
monitoring tick != planning tick
~~~

Automatic navigation is now specified as:

~~~text
plan -> accept short trajectory/segment -> execute -> monitor
~~~

A stable automatic segment is not replanned merely because another fixed frame elapsed. Local replanning is event-driven by:
- accepted segment completion/expiry;
- tracking error outside the accepted envelope;
- newly invalidating dynamic hazard evidence;
- vehicle capability change/damage.

Goal intent or topology/route-branch invalidation escalates to a full-route rebuild.

Manual navigation shares the accepted route but not autopilot authority.
Manual guidance performs:
- configurable periodic local suffix refresh (initial policy default 0.25 s);
- immediate local refresh when the player exits the recommended corridor;
- immediate local refresh for hazard/capability invalidation.

Manual local deviation preserves the global RoutePlan while its topology branch remains valid. Full route solving is reserved for goal/branch/topology invalidation.

Important current limitation:
`NavigationRuntimeLab` still calls `NavigationRuntimePlanner::plan()` in its fixed-step path. This is now explicitly transitional. The next integration slice must insert an accepted-segment/follower seam and prove stable automatic flight with many execution ticks per planner invocation (`planCount << executionCount`).

Target-machine validation for this scheduler/policy is pending. The last target-machine accepted Stage-12 baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## AcceptedShortSegment + TrajectoryFollower integration candidate

Code candidate:

~~~text
0fc8d9b9d929cb19ad0338191433b635bb723f31
~~~

This slice replaces the live Stage-12 lab's unconditional fixed-tick
`NavigationRuntimePlanner::plan()` call with the first real accepted-execution
seam:

~~~text
planner epoch
    -> AcceptedShortSegment
    -> TrajectoryFollower every execution tick
    -> PilotSkillExecutor
    -> physics
    -> NavigationExecutionReplanPolicy
~~~

New production files:
- `src/game/navigation/AcceptedShortSegment.h`
- `src/game/navigation/TrajectoryFollower.{h,cpp}`

The accepted segment retains planner/world revisions, accepted/valid times,
local start/target/velocity, portal-axis capture data, control response data,
tracking envelope, emergency metadata and a vehicle-capability snapshot.

The follower deliberately has no NavigationMap/NavigationSpace dependency. It
tracks the already accepted local velocity/acceleration product and recomputes
fixed-step angular/linear control from current kinematics without running
obstacle or route search.

`GameSimulation::buildNavigationRuntimeLabIntent()` now asks
`NavigationExecutionReplanPolicy` before waking the planner. The current live
automatic triggers wired by this slice are:
- no accepted segment;
- segment completion;
- segment expiry;
- tracking-envelope escape;
- vehicle-capability change;
- goal revision change.

Stable execution keeps the accepted segment authoritative between those
events. Nominal lab segment validity is 0.75 s. Provisional
`ConflictHold/StaticHold/StaleHold` products are accepted only for 0.25 s, so
a safe hold cannot silently become a permanent navigation shutdown.

Diagnostics now distinguish:
- `planCount`;
- `executionCount`;
- `acceptedSegmentFollowCount`;
- `acceptedSegmentReplanCount`;
- accepted segment revision / last replan reason.

The isolated navigation-runtime target now also compiles
`TrajectoryFollower.cpp` and pins:
- follower execution without world search;
- tracking-envelope invalidation;
- stable `planCount << executionCount`.

Important limitations of this candidate:
- target-machine MinGW/runtime validation is still pending;
- the existing `DynamicHazardInvalidated` policy hook is not yet fed by a
  dedicated live accepted-segment hazard monitor in this slice;
- manual `GuidanceCorridor` publication from the same segment is still the
  next manual-mode integration step;
- the live slit/tunnel capture must be re-run after this cadence change.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Do not promote `0fc8d9b9d929cb19ad0338191433b635bb723f31` to accepted
baseline until the target-machine gate is green.


### Accepted-segment integration correction

Superseding code candidate:

~~~text
1e1b5ff2e72d743688833a403e94aefc0a832c87
~~~

This correction fixes two defects found during post-commit diff review of
`0fc8d9b9d929cb19ad0338191433b635bb723f31` before target-machine testing:

1. generated include edits contained literal `\n` text in
   `GameSimulation.h` and
   `NavigationExecutionReplanPolicyTests.cpp`; these are now real line breaks;
2. provisional hold segments no longer report immediate geometric completion.
   `AcceptedShortSegment::completionTriggersReplan` lets a safe hold remain
   accepted until its short validity expiry instead of waking the planner every
   fixed tick.

A regression now pins that a time-bounded hold stays in follower execution until
expiry.

Target-machine validation remains pending. The last actually accepted Stage-12
baseline is still:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Use `1e1b5ff2e72d743688833a403e94aefc0a832c87` as the code candidate for
the next MinGW/runtime gate.


### Target-machine gate failure: accepted-segment intent revision contract

Target-machine run against repository HEAD
`9a8b618f08f80b9945a49709640d1fc52e54be24` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_local: PASS
navigation_space: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: PASS (11/11)
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL (return 43)
~~~

Failure:

~~~text
[FAIL] navigation-runtime same-tick sparse replication proof incomplete
error_mps2=inf canonical_error_mps2=inf
~~~

Root cause identified before changing code:

The pre-follower runtime planner used `NavigationRuntimePlanner::Result::intent`
whose `Intent::revision` is the goal/mission revision
(`plannerGoal.revision == 1202001` in the Stage-12 lab).

The first `TrajectoryFollower` implementation incorrectly replaced that
semantic identity with `AcceptedShortSegment::revision`, which is an execution
segment epoch (1, 2, 3, ...).

The live self-test intentionally filters sparse execution publications with:

~~~text
replicatedExecution->intentRevision == 1202001
~~~

Therefore every otherwise eligible sparse publication from the new follower was
discarded before same-tick/canonical comparison. Both replication error values
remained at their initialized infinity values. This failure does not demonstrate
a sparse/canonical acceleration mismatch; it demonstrates that the follower
violated the existing intent-revision contract before the comparison could run.

Required correction:
- preserve `goalRevision` as `Bridge::Intent::revision`;
- retain `AcceptedShortSegment::revision` separately as execution-segment
  identity/diagnostics;
- pin this distinction in the isolated follower test;
- rerun the same target-machine gate.

No new Stage-12 baseline is accepted. Last actually target-machine accepted
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Intent revision / accepted target revision ownership fix

Code candidate:

~~~text
88bf3f05f405bc3c9b93a6ecf3619fcdcd0879cb
~~~

The target-machine replication failure exposed an overloaded revision field.
This code slice separates the two identities already represented by replication
output:

~~~text
intentRevision       = high-level maneuver/goal identity
activeTargetRevision = concrete accepted execution target/segment identity
~~~

Changes:
- `NavigationRuntimeControlBridge::Intent` now carries `targetRevision`
  separately from `revision`;
- `PilotSkillExecutor::Command` now carries the same optional target revision;
- zero `targetRevision` falls back to `revision`, preserving all legacy
  callers;
- reaction-delay ownership still follows high-level `revision`;
- the queued/applied target revision follows `targetRevision`;
- `TrajectoryFollower` publishes
  `intent.revision = AcceptedShortSegment::goalRevision` and
  `intent.targetRevision = AcceptedShortSegment::revision`.

This preserves the Stage-12 self-test requirement
`intentRevision == 1202001` while retaining explicit segment identity in
`activeTargetRevision`.

Target-machine validation of this correction is pending. Last actually accepted
Stage-12 baseline remains
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Revision-split regression coverage

Test candidate:

~~~text
25c8f2ad1df21de937c5db0ce24394b2e0b93fab
~~~

Regression coverage now pins the correction exposed by the failed target-machine
replication gate:

- runtime bridge preserves high-level `intentRevision` while publishing a
  distinct `activeTargetRevision`;
- `TrajectoryFollower` maps
  `goalRevision -> intent.revision` and
  `AcceptedShortSegment::revision -> intent.targetRevision`;
- `PilotSkillExecutor` may advance the concrete target revision inside the
  same intent without restarting reaction delay;
- the Stage-12 architecture contract now rejects re-merging these identities.

The next required action is a target-machine rerun. No acceptance is claimed
until the MinGW tests/build and `EliteServer --self-test-navigation` are green.

Last actually accepted Stage-12 baseline remains
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Target-machine gate failure after revision split

Target-machine run against
`91ac7f66e02f1a9aa76df82d09557e94e601310c` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: 10/11 PASS
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL after the earlier replication gate
~~~

Isolated regression failure:

~~~text
navigation_pilot_skill:
NAVIGATION PILOT SKILL TESTS: FAIL:
new target inside same intent must not restart reaction delay
~~~

This regression was constructed incorrectly: it changed the target at t=0.01
after `reset(t=0)` while the configured initial reaction delay was still
active. It therefore measured the initial command delay, not a delay restart
caused by targetRevision. The test must first establish an already-reacted
intent, then change only targetRevision and verify that no new reaction window
starts.

More importantly, the live self-test now advances beyond the previous
same-tick replication filter and fails later on physical exact geometry:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
~~~

This is a real Stage-12 execution failure. The accepted-segment cadence is now
allowing a command to remain authoritative long enough that the physical exact
sweep catches a static HitVolume crossing. The next code pass must identify
entity 28, capture the accepted segment/planner status/target at the violation,
and correct segment invalidation/follower semantics rather than weakening the
exact-static gate.

No new Stage-12 baseline is accepted. Last actually target-machine accepted
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Exact-static accepted-segment monitoring candidate

Code candidate:

~~~text
fe6ca95a17b9edda45de08c4ee624b3cc43a87c2
~~~

This pass addresses the target-machine failure where the live Stage-12 actor
physically crossed exact-static entity 28. Deterministic scene spawn order maps
entity 28 to `NAV STRESS CUBE 08`, the lower-middle block of the slit tunnel.

Root cause:
the first accepted-segment integration monitored expiry, completion, tracking
error, capability change and goal revision, but did not revalidate the
currently executing kinematics against authoritative exact-static HitVolumes
every fixed step. A still-unexpired segment could therefore remain
authoritative while inertia/tracking drift made its physical continuation
unsafe near the tunnel wall.

New monitor behavior:

~~~text
fixed tick
  -> follow accepted segment
  -> exact-static execution monitor
       * current position -> accepted target
       * short current-kinematics forecast
  -> if still clear: continue same segment, no planner call
  -> if blocked: StaticSafetyInvalidated -> immediate LocalHorizon replan
~~~

The monitor uses `NavigationSpace::querySegment` with
`exactObstaclesOnly=true`, the live ship envelope and the planner's static
additional-clearance policy. This is observation/validation work, not route or
avoidance search, so the core invariant remains:

~~~text
monitoring tick != planning tick
execution tick != planning tick
~~~

New policy reason:
`NavigationExecutionReplanPolicy::Reason::StaticSafetyInvalidated`.

New live diagnostics:
- `acceptedSegmentStaticSafetyInvalidationCount`;
- `acceptedSegmentLastStaticBlockingEntityId`.

The same pass corrects the newly added PilotSkillExecutor regression. Its
previous form changed targetRevision at t=0.01 while the initial reaction delay
from reset was still active, so it did not actually test whether a target
change restarted reaction delay. The corrected test first lets the existing
intent finish its initial reaction window, then changes only targetRevision.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Pilot regression timing correction

Superseding target-machine candidate:

~~~text
5665d4bc27edf138d744dacadc73329980df925b
~~~

Post-commit static review found one defect only in the new regression test:
`PilotSkillExecutor::step` requires `deltaSeconds` to equal elapsed time since
the previous step. The test attempted `step(0.50, 0.25)` immediately after
`reset(0.0)`, which would correctly return InvalidInput.

The regression now advances through two valid 0.25 s steps:
- t=0.25: initial reaction window is still active;
- t=0.50: the original intent becomes eligible;
- t=0.51: only targetRevision changes, proving that no new reaction window is
  started.

Production exact-static accepted-segment monitoring remains the code from
`fe6ca95a17b9edda45de08c4ee624b3cc43a87c2`.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


### Target-machine result: exact-static monitor candidate still fails live tunnel

Target-machine run against repository HEAD
`a29f3e01708663afb003dfe14ddbce0e4f330316` produced:

~~~text
architecture Stage-12 contract: PASS
navigation_runtime: PASS (5/5)
navigation_trajectory: PASS (11/11)
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL
~~~

The live failure is unchanged in physical terms:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
~~~

Entity 28 is the deterministic `NAV STRESS CUBE 08` lower-middle block of
the authored slit tunnel.

Therefore candidate
`5665d4bc27edf138d744dacadc73329980df925b` is NOT accepted as the new
Stage-12 baseline. The newly added exact-static accepted-segment monitor did not
prevent the collision in the authoritative runtime.

Important evidence from this run:
- the corrected target-revision/reaction regression now passes;
- all isolated runtime and trajectory tests pass;
- the failure is now exclusively in the live authoritative behavioral run;
- the current defect is specifically in execution/monitor/planner interaction
  near the slit tunnel, not compilation, replication, or the isolated policy
  contracts.

Next work must instrument the accepted segment immediately before entity-28
impact: segment revision, replan reason, exact-static monitor result, accepted
target, velocity, forecast endpoint and planner status. Do not weaken the
physical exact-static gate.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Accepted-segment exact-static impact instrumentation candidate

Code candidate:

~~~text
61d89d59a87c7034c948d85417c71c6ab4ae7f47
~~~

This pass is diagnostic-only. It does not alter navigation authority or
replanning behavior.

The authoritative live self-test still fails by physically sweeping the lab
Cobra through exact-static entity 28 (`NAV STRESS CUBE 08`, the lower-middle
slit-tunnel block). The previous exact-static monitor did not prevent that
impact, so the next required evidence is the execution state immediately before
the failing fixed step.

The runtime now preserves the last pre-physics accepted-segment safety probe:
- target blocked / forecast blocked flags;
- blocking entity;
- probe start;
- accepted target;
- short forecast endpoint;
- current map-space velocity;
- follower ideal acceleration;
- forecast duration and probe time.

At the first exact-static physical violation it additionally snapshots:
- accepted segment revision;
- last replan reason;
- planner status;
- accepted target + accepted target velocity;
- current velocity;
- last actually executed PilotSkill demand;
- the complete previous exact-static monitor witness.

`EliteServer --self-test-navigation` now prints that witness directly on
return-55 failure.

The purpose of this pass is to distinguish among:
1. the monitor never seeing entity 28;
2. monitor seeing it and replanning too late;
3. planner replacing the segment with another unsafe segment;
4. ideal follower forecast being optimistic relative to PilotSkill/physics
   inertia.

Target-machine validation/output is pending. No Stage-12 baseline changes.
Last actually target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Target-machine witness: ideal follower forecast diverges from executed PilotSkill command

Target-machine live self-test against the impact-instrumented candidate produced:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
segment_revision=127
planner_status=1
last_replan_reason=2
static_invalidations=1
previous_monitor_blocker=0
previous_target_blocked=0
previous_forecast_blocked=0
previous_probe_time_s=54.8
previous_forecast_s=0.73
violation_start=(1065.16,-1268.64,-4977.71)
violation_end=(1064.97,-1268.68,-4977.59)
current_velocity=(-9.77741,-2.20251,6.24856)
accepted_target=(811.472,-1361.83,-5385.02)
accepted_target_velocity=(-31.1628,-11.4446,-49.979)
last_executed_demand=(-3.88461,-43.6151,13.1693)
previous_forecast_end=(1053.75,-1272.09,-4984.39)
previous_velocity=(-9.77741,-2.20251,6.24856)
previous_ideal_accel=(-16.039,-6.93155,-42.1707)
~~~

Root cause is now concrete:

- accepted target and target velocity were already away from CUBE 08 in -Z;
- follower ideal acceleration also demanded strong -Z correction
  (`-42.1707 m/s^2`);
- the actually executed PilotSkill demand was still +Z
  (`+13.1693 m/s^2`);
- the exact-static monitor forecast used the follower's ideal acceleration, not
  the actual delayed/filtered PilotSkill command that authoritative physics was
  applying;
- therefore the monitor proved a hypothetical trajectory that the ship was not
  actually following and missed the imminent impact.

The prior monitor implementation is not accepted.

Required correction:
- execution safety prediction must start from current velocity and the latest
  actually executed PilotSkill acceleration;
- it must include a conservative braking/command-transition envelope before it
  may assume the new ideal follower demand;
- exact-static validation must cover that physically reachable swept envelope,
  not just one ideal endpoint chord;
- planner cadence remains event-driven; monitoring may remain per fixed tick.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Executed-demand exact-static safety candidate

Superseding code candidate:

~~~text
6fc035f23b079a33bde99bc564108a0494c9fca9
~~~

The target-machine impact witness proved that the first exact-static monitor
used the wrong acceleration source near the slit wall:

~~~text
follower ideal Z acceleration   = -42.1707 m/s^2
actually executed Z demand      = +13.1693 m/s^2
~~~

The planner/follower were already commanding motion away from entity 28 while
PilotSkillExecutor/physics were still carrying the previous command toward it.
The ideal-only monitor therefore proved a hypothetical safe path rather than
the authoritative near-term motion.

This candidate adds a second exact-static execution branch:
- start from current authoritative map position and velocity;
- use the latest actually executed PilotSkill linear demand;
- predict constant-current-command motion across the remaining accepted
  segment horizon;
- sample that parabolic path into 12 subsegments;
- exact-query every subsegment against physical HitVolumes;
- any blocked subsegment raises
  `NavigationExecutionReplanPolicy::StaticSafetyInvalidated` immediately.

The previous ideal follower forecast remains as a separate branch. Safety now
requires both:
1. the intended new command path is clear;
2. continued execution of the command that physics is actually receiving is
   also clear.

This preserves event-driven planning: the exact-static monitor may run every
fixed tick, but `Planner::plan` is still woken only by invalidating evidence.

Additional diagnostics now preserve and print:
- whether the executed-demand forecast was blocked;
- its last forecast endpoint;
- the executed acceleration used for the proof.

Target-machine validation is pending. No Stage-12 baseline promotion is claimed.
Last actually target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Root-cause correction: executed-demand monitor mixed WORLD and MAP frames

The latest target-machine run with sampled executed-demand monitoring still
failed on exact-static entity 28, but its telemetry exposed a more fundamental
frame error:

~~~text
last_replan_reason=6
static_invalidations=40
previous_monitor_blocker=28
previous_executed_forecast_blocked=1
~~~

So the monitor was now detecting danger and requesting
`StaticSafetyInvalidated` replans, but inspection of the production ownership
chain found that the acceleration fed into the executed-motion forecast was in
the wrong coordinate frame.

Production flow is:

~~~text
TrajectoryFollower intent      : NavigationMap working frame
Planner::mapIntentToWorld(...) : converts command to WORLD frame
PilotSkillExecutor             : executes that WORLD-frame command
ExecutionSnapshot              : carries the executed WORLD-frame vector
~~~

However `GameSimulation::updateNpcNavigationControl` copied
`ExecutionSnapshot::executedLinearAccelerationDemandMapMps2` directly into
`NavigationRuntimeLabObservation::lastExecutedLinearDemandMapMps2` and the
exact-static monitor then combined it with
`agent.velocityMapMetersPerSecond`.

Therefore the monitor mixed:

~~~text
position / velocity : MAP frame
executed acceleration: WORLD frame
~~~

The previous numerical comparison between follower ideal Z and executed Z is
not a valid same-axis comparison until the executed vector is transformed back
into the NavigationMap frame.

Required correction:
- retain WORLD executed demand for world-space application/diagnostics;
- explicitly transform authoritative executed WORLD acceleration back to the
  lab NavigationMap axes before storing
  `lastExecutedLinearDemandMapMps2`;
- exact-static motion forecast must consume only MAP-frame position, velocity
  and acceleration;
- pin this transform in the Stage-12 architecture contract/regression evidence.

The emergency-response hypothesis remains unproven and must not be acted on
until this frame error is removed.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### WORLD -> MAP executed-demand correction candidate

Code candidate:

~~~text
6659354c5b2c8e280cdae28cf9ea021b2ca4da5e
~~~

The sampled executed-demand safety monitor exposed a coordinate-frame ownership
bug rather than an established emergency-response failure.

In the live lab production path:

~~~text
TrajectoryFollower                   -> MAP-frame demand
NavigationRuntimePlanner::mapIntentToWorld
                                     -> WORLD-frame demand
PilotSkillExecutor / control bridge  -> WORLD-frame executed demand
DynamicMotionSystem                  -> applies WORLD-frame acceleration
~~~

The legacy ExecutionSnapshot member name still says
`executedLinearAccelerationDemandMapMps2`, but for this runtime path its
contents are already WORLD-space because the conversion happens before the
bridge.

The exact-static monitor was incorrectly combining that WORLD-space executed
acceleration with MAP-space position and velocity.

This candidate makes the boundary explicit in
`GameSimulation::updateNpcNavigationControl`:

~~~text
executedWorldVector
  -> dot(world, hub.normalAxis)     = map X
  -> dot(world, hub.radialAxis)     = map Y
  -> dot(world, -hub.progradeAxis)  = map Z
  -> executedMapVector
~~~

Only `executedMapVector` is stored in
`lastExecutedLinearDemandMapMps2` and consumed by the sampled exact-static
forecast. Existing world-space lateral/applied diagnostics continue using
`executedWorldVector`.

If the hub frame is invalid the stored map-space command is explicitly cleared
instead of reusing stale data.

The Stage-12 architecture gate now pins the complete inverse-basis transform so
the safety monitor cannot silently regress to mixed coordinate frames.

Target-machine validation is pending. The emergency-response hypothesis remains
unproven until this candidate is exercised.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Target-machine result after WORLD -> MAP correction

Target-machine run against
`60da8fc21a458b481894f3edebb266c02ed8d2be` produced:

~~~text
architecture contract: FALSE NEGATIVE
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL on exact-static entity 28
~~~

Architecture-check failure:

~~~text
[FAIL] accepted automatic segment must be monitored against exact-static geometry without restoring per-frame planning
~~~

Root cause of that check is textual/stale only: the architecture script still
requires the retired helper name `exactExecutionBlocked`, while production
code now intentionally uses `exactExecutionSegmentBlocked`. The build and
live runtime both compile/use the new helper. The gate must be updated rather
than reverting production code.

Live witness:

~~~text
segment_revision=129
planner_status=AdjustedClear
last_replan_reason=StaticSafetyInvalidated
static_invalidations=3
previous_monitor_blocker=28
previous_executed_forecast_blocked=1

velocity=(-9.77789,-2.20347,+6.24257)
ideal_accel=(-16.0268,-6.93006,-42.1738)
executed_accel=(-16.7456,-7.03306,-41.9582)
~~~

The WORLD -> MAP correction is validated by the witness: ideal follower and
actually executed acceleration now agree closely in the same NavigationMap
frame. Coordinate-frame mismatch is no longer the cause of the entity-28
impact.

The remaining failure is dynamic viability. The monitor detects the obstacle
and wakes the local planner, but `AdjustedClear` continues to optimize route
progress while the ship still has finite momentum toward the wall. Replanning
alone cannot instantaneously remove velocity. Static-safety invalidation must
therefore be able to select a short active recovery maneuver (brake/escape)
before returning to ordinary progress.

No Stage-12 baseline promotion. Last target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Directional stopping reserve + emergency recovery candidate

Superseding target-machine candidate:

~~~text
f25a9c2378c45950155db00caf48e71b5a07e3ac
~~~

The last live witness proved that WORLD -> MAP correction is working: follower
ideal and actually executed acceleration now agree closely in the same local
NavigationMap frame. The remaining entity-28 failure is therefore dynamic
viability, not coordinate conversion.

The accepted-segment monitor previously detected exact-static risk and requested
`StaticSafetyInvalidated`, but planner output remained an ordinary
`AdjustedClear` progress target. That cannot instantaneously remove existing
momentum.

This candidate adds a directional local-frame stopping reserve:

~~~text
current local velocity
  -> conservative control-response coast distance
     (decision period + command latency + one response-frequency interval)
  -> guaranteed braking distance at manoeuvre-thruster authority
  -> exact-static swept query along that stopping direction
~~~

If this stopping reserve intersects exact-static geometry, AUTO selects a short
active recovery product rather than only another progress replan:

~~~text
AcceptedShortSegment
  linearMode = FixedAcceleration
  acceleration = opposite current local velocity
  magnitude = guaranteed manoeuvre braking authority
  emergency = true
  hazardUrgency01 = 1
  validity = 0.25 s
~~~

The recovery segment is still normal navigation execution. It does not disable
the navigation module and does not discard the global route. While that recovery
segment is active, the same static warning does not wake the planner every fixed
tick; expiry returns control to the local planner, which either continues
recovery if the stopping reserve is still blocked or resumes progress.

The stopping-reserve proof is independent of the ordinary target/ideal/executed
forecast checks. This was corrected after static review so recovery still
activates when an ordinary forecast has already found the blocker.

Architecture-gate fixes in the same candidate:
- stale `exactExecutionBlocked` marker replaced with
  `exactExecutionSegmentBlocked`;
- recovery diagnostics are checked in `LAB_H`, not `GameSimulation.h`;
- the single LOCAL INERTIAL / TACTICAL translational-frame contract is pinned.

If live failure remains, the server witness now includes:
- emergency recovery count / active state;
- stopping-reserve blocked flag;
- response reserve seconds;
- stopping reserve distance.

Target-machine validation is pending. Last actually accepted Stage-12 baseline
remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### Coordinate/spatial architecture audit

A repository-wide spatial-ownership audit was completed while the current
Stage-12 target-machine candidate remains under validation.

Audit document:

~~~text
src/game/navigation/COORDINATE_ARCHITECTURE_AUDIT.md
~~~

The audit finds that recurring MAP/WORLD defects are architectural debt rather
than unrelated local mistakes. Important confirmed issues include:
- tactical KinematicFrame basis and visual/model hub basis both entering
  navigation paths;
- DynamicMotionState mixing authoritative local and derived world quantities;
- propulsion acceleration bouncing Local -> WORLD -> Local;
- global gravity being sampled/stored but not consumed by HubTactical local
  fixed-step integration;
- HubNavigationFrame duplicating KinematicFrame transforms;
- duplicate spatial truth in ShipTransform + ShipReferenceFrameSnapshot;
- WorldPosition changing meaning between system-local and galactic-absolute
  based on side-channel systemId;
- transitional pendingReferenceVelocityMatch speed heuristic.

Recommended direction is two physical simulation domains with a shared frame
kernel: server-authoritative GlobalDynamics, deterministic shared
LocalSimulation, and a small shared SpatialCore owning strong types,
KinematicFrame transforms/rebasing and the only Global<->Local API.

This is an AUDIT/PROPOSAL, not yet an accepted migration contract. No current
navigation behavior or target-machine acceptance status is changed by this
documentation pass.


## 2026-09-19 spatial-foundation decision / Navigation freeze

The coordinate audit is now preserved in:

~~~text
src/game/navigation/COORDINATE_ARCHITECTURE_AUDIT.md
src/game/navigation/SPATIAL_ARCHITECTURE_DECISION.md
~~~

Project direction is now fixed as follows:

- complete and record the already-running target-machine gate for current
  recovery candidate `f25a9c2378c45950155db00caf48e71b5a07e3ac`;
- do not add another Navigation v2 algorithm/recovery patch after that gate;
- pause Stage-12 implementation at the current boundary;
- make Spatial Phase A the next implementation task;
- resume Navigation v2 only after the canonical Global/System <-> Local API,
  strong coordinate types and LocalDomainFrame authority are green.

Last actually target-machine accepted Stage-12 baseline is unchanged:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Current candidate remains unaccepted until the target-machine output is
reported.

The selected local-simulation model is a moving free-fall interaction domain:
GlobalDynamics advances the domain carrier under large-scale gravity; short
range LocalSimulation may cancel/ignore the common-mode field and integrate only
local forces. Differential/tidal gravity is an explicit optional residual and
may be omitted only under a bounded error criterion.

A local domain may follow an anchor ship, but interacting bodies must share one
LocalDomainId; the domain basis is not the ship body frame. Preferred basis is
non-rotating/inertial over the local horizon.

Star-system coordinates use one shallow System frame per SystemId. The Sun is
not a universal master origin, and planet/moon/hub relationships do not create
new public coordinate domains.

This changes project sequencing/architecture only. It does not promote or
reject the pending Stage-12 candidate and does not alter the last verified
baseline.


### Target-machine recovery gate — emergency brake still enters exact-static entity 28

Latest target-machine run of the Stage-12 recovery code path completed full
MinGW client/server build but the live navigation self-test still failed:

~~~text
[FAIL] moving-passage continuation crossed exact static geometry
violation_entity=28
segment_revision=136
planner_status=1
last_replan_reason=2
static_invalidations=191
emergency_recoveries=15
emergency_recovery_active=1
stopping_reserve_blocked=1
stopping_reserve_s=0.643333
stopping_reserve_m=27.7918
previous_monitor_blocker=28
previous_target_blocked=1
previous_forecast_s=0
~~~

Physical state at first violation:

~~~text
current_velocity=(-6.26585,-1.23874,+6.80803)
accepted_target=(1049.98,-1271.4,-4957.4)
accepted_target_velocity=(0,0,0)
last_executed_demand=(+1.35067,+0.272356,-1.46091)
previous_ideal_accel=(0,0,0)
previous_executed_accel=(0,0,0)
~~~

The emergency segment is physically braking opposite the current velocity:
the executed demand direction is consistent with active deceleration. Therefore
the remaining defect is not "emergency command failed to reach physics".

The decisive evidence is:

~~~text
stopping_reserve_blocked=1
previous_target_blocked=1
~~~

The recovery begins only after the state has already reached a configuration
whose complete conservative stopping envelope intersects exact-static geometry.
A replan/emergency command at that instant cannot make the collision avoidable.

Root cause / required Navigation-v2 correction:

- stopping viability must be an ACCEPTANCE invariant of every executable short
  segment, not only a late invalidation response;
- a segment may be accepted only if the swept trajectory plus the physically
  reachable stopping/reaction envelope at its future execution states remains
  exact-static safe;
- when that invariant first fails, the planner must select an earlier escape or
  braking maneuver while free space still exists;
- do not weaken the exact-static collision gate and do not hide the failure by
  increasing cube clearance ad hoc.

The last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

The current recovery candidate remains unaccepted.


### 2026-09-19 task sequencing correction — finish Navigation v2 behind a sealed spatial API

The previous spatial-foundation note proposed freezing Navigation v2 before
Spatial Phase A. Project sequencing is now intentionally changed after review
of the ACTUAL repository coordinate types and live failure evidence.

Current task:

~~~text
FINISH NAVIGATION V2 FIRST
    -> seal its coordinate boundary
    -> remove ambiguous MAP/WORLD control semantics
    -> move stopping viability into segment/candidate acceptance
    -> pass the Stage-12 live exact-static tunnel gate

THEN
    -> resume the wider repository spatial/global refactor
~~~

This is not a rollback of the coordinate audit. The audit remains valid and is
now used to define a strict Navigation-v2 containment boundary.

Verified existing coordinate facts that Navigation v2 must respect:
- runtime WorldPosition is system-local while systemId >= 0; the same type is
  used as galactic-absolute by PlayerSpatialDomainResolver outside a system;
- KinematicFrame is the existing canonical moving-frame P/V/A transform and
  already owns frame velocity, acceleration, omega/alpha and rebase math;
- HubNavigationFrame duplicates part of KinematicFrame and is not allowed to
  become an additional Navigation-v2 coordinate API;
- authored hub visual axes differ from tactical KinematicFrame axes;
- NavigationMap::DynamicActorInput currently claims system velocity but live
  Stage-12 supplies a pre-relative velocity because WorkingFrame lacks frame
  velocity/omega;
- NavigationRuntimePlanner::mapIntentToWorld currently returns the SAME
  Bridge::Intent type after changing the physical meaning of its vectors;
- ShipControlState / NavigationExecutionSnapshot still use *Map* field names
  for vectors that authoritative physics interprets in system/world axes.

Therefore the immediate Navigation-v2 cleanup target is:

~~~text
existing system/runtime state
        |
        | ONE explicit typed NavigationFrameBoundary
        v
NavLocal-only NavigationMap / NavigationSpace / planner / follower / monitor
        |
        | ONE explicit typed control egress
        v
System-space PilotSkill / ShipControlState / physics
~~~

No raw visual/model coordinates or semantically ambiguous "Map" control vector
may cross that boundary.

The wider question of galactic/system/global authority remains deferred until
Navigation v2 is closed and accepted; no new global coordinate model is assumed
by this task.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 target-machine late-braking root cause — stopping reserve invalidation was overwritten

The latest live witness raised the key timing question: Navigation must begin
avoidance/braking while the maneuver is still physically reachable, not only
after collision geometry is immediately ahead.

Repository inspection confirms the physical horizon itself is already
speed-dependent:

~~~text
latencyDistance = v * resultAge + 0.5 * |a| * resultAge^2
brakingDistance = v^2 / (2 * maxBrakingAcceleration)
horizonDistance = max(
    minimumHorizon,
    latencyDistance + brakingDistance + turnDistance + safetyMargin
)
~~~

The late emergency activation is caused by a concrete execution-monitor bug,
not by absence of a speed-dependent distance horizon.

Current Stage-12 code does:

~~~cpp
if (staticSafetyStoppingReserveBlocked)
{
    staticSafetyInvalidated = true;
    staticSafetyRecoveryRequired = true;
}

staticSafetyTargetBlocked = exactExecutionSegmentBlocked(...);

// BUG: destroys the earlier stopping-reserve invalidation whenever the direct
// current->target chord is still clear.
staticSafetyInvalidated = staticSafetyTargetBlocked;
~~~

Therefore Navigation can detect that the complete reaction+braking reserve is
already unsafe, then discard that evidence and continue executing the accepted
segment. Replanning/emergency begins later when the direct target chord itself
finally becomes blocked, at which point the target-machine witness shows:

~~~text
stopping_reserve_blocked=1
emergency_recovery_active=1
violation_entity=28
~~~

Required correction:

~~~cpp
staticSafetyInvalidated =
    staticSafetyInvalidated || staticSafetyTargetBlocked;
~~~

More generally, the Navigation-v2 acceptance invariant is:

- detection horizon is physical and speed/capability dependent;
- a candidate executable segment is valid only while a safe reaction/braking
  or maneuver reserve remains available;
- stopping-reserve invalidation has equal authority to direct target/forecast
  blockage and may never be overwritten by a later independent safety check;
- planner execution remains event-driven: monitor every fixed tick, replan only
  when the accepted segment loses that invariant.

The fixed 3 s dynamic-conflict look-ahead is a separate policy dimension from
the physical distance horizon. It must eventually be bounded from below by the
time represented by the physical maneuver reserve for high-speed moving
hazards, but it is not the cause of this CUBE 08 exact-static failure.

No Stage-12 baseline promotion. Last actually target-machine accepted baseline:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

### 2026-09-19 Navigation-v2 sealed spatial API + timely physical horizon candidate

Current unaccepted candidate code HEAD before the next broadphase-alignment slice:

~~~text
cfecf1014a6f109acef65d46707f15e7acfa4242
~~~

The target-machine failure at entity 28 established that emergency braking was
already physically commanded correctly but began after the safe stopping
envelope had been lost. Repository tracing found the immediate code defect:

~~~cpp
if (staticSafetyStoppingReserveBlocked)
    staticSafetyInvalidated = true;

// old bug: erased the earlier stopping-reserve result
staticSafetyInvalidated = staticSafetyTargetBlocked;
~~~

The monitor is now monotonic:

~~~cpp
staticSafetyInvalidated =
    staticSafetyInvalidated || staticSafetyTargetBlocked;
~~~

A later independent safety check can add an invalidation reason but cannot erase
an already proven unsafe stopping reserve.

Navigation timing is now based on one shared physical maneuver horizon rather
than unrelated planner/execution formulae. PhysicalManeuverHorizon derives:

~~~text
response time =
    snapshot age
  + pilot reaction delay
  + decision period
  + command latency
  + command/filter response reserve

response distance = v*t + 0.5*a*t^2
speed at brake    = v + a*t
braking distance  = v_brake^2 / (2*a_brake)

distance horizon =
    max(minimum,
        response distance
      + braking distance
      + maneuver/turn distance
      + safety margin)

dynamic time horizon =
    max(policy minimum,
        response time + braking time)
~~~

Therefore the horizon is deliberately NOT merely linear in speed: reaction
distance is approximately linear in speed, while braking distance is quadratic
in speed. Dynamic look-ahead also grows with the time physically required to
respond and brake.

The live pilot profile's reactionDelaySeconds is now included in the reserve;
the previous live stopping monitor omitted it.

Navigation-v2 coordinate ownership has also been sealed around the ACTUAL
repository coordinate contracts:

~~~text
system/runtime state
        |
        | NavigationFrameBoundary (KinematicFrame-backed)
        v
NavLocal-only NavigationMap / NavigationSpace / planner / follower / monitor
        |
        | NavigationLocalControlIntent
        | explicit NavigationFrameBoundary conversion
        v
NavigationSystemControlIntent
        |
        v
PilotSkillExecutor -> ShipControlState -> authoritative physics
~~~

Key API invariants now encoded in C++ types:

- NavigationMap accepts NavLocal actor position/velocity/acceleration/angular
  velocity only; it no longer owns a WorkingFrame or system->map conversion.
- NavigationLocalControlIntent and NavigationSystemControlIntent are distinct
  incompatible types.
- NavigationRuntimePlanner and TrajectoryFollower cannot emit a system-space
  execution command.
- NavigationRuntimeControlBridge, ShipControlState, replicated execution truth
  and authoritative physics use explicit System-space field names.
- the only local/system conversion admitted by Navigation v2 is
  NavigationFrameBoundary, backed by the existing KinematicFrame.
- the existing project basis was verified rather than replaced: hub tactical
  axes are X=prograde, Y=radial, Z=normal with
  normal=cross(prograde,radial); the Stage-12 navigation/visual permutation
  X=normal, Y=radial, Z=-prograde is likewise right-handed.

Network serialization retains the same byte ordering and scalar layout; only the
C++ execution field semantics/names changed from misleading Map to System.

New deterministic regression:
PhysicalManeuverHorizonTests.cpp proves that doubling speed from 10 to 20 m/s
with the same braking authority grows the required distance from 45 m to 125 m
and the dynamic time horizon from 5.5 s to 10.5 s in its fixture.

One remaining architecture alignment is intentionally NOT hidden:
NavigationMap broadphase still uses its configured fixed prediction sweep
(default/live lab 3 s) while LocalHorizonPlanner can now require a longer
physical dynamic look-ahead. Before Navigation-v2 acceptance, broadphase
candidate publication must consume the same effective physical horizon so a
fast incoming actor cannot be omitted before local conflict evaluation.

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

### 2026-09-19 physical dynamic broadphase horizon aligned

Current unaccepted Navigation-v2 candidate code HEAD:

~~~text
57ae48cc4cc7d376279582e1da722961e5c9fb5a
~~~

The previously recorded final timing mismatch is now closed:
NavigationMap no longer forces every dynamic broadphase query to use only its
configured fallback prediction horizon.

Both CorridorQuery and SphereQuery now expose an optional lookAheadSeconds.
When supplied, all of the following are evaluated over that exact requested
time horizon:

- broadphase AABB expansion;
- per-actor conservative swept radius;
- candidate predicted endpoint;
- candidate conservative swept product;
- QueryResult/Candidate diagnostics identifying the horizon used.

The sparse index remains indexed by current NavLocal actor position. To avoid a
full actor scan merely to size the query AABB, NavigationMap retains bounded
publication maxima for indexed actor radius, speed and acceleration and uses
them to compute a conservative maximum swept-radius upper bound for the query
horizon. Per-actor acceptance then uses each actor's own velocity/acceleration
travel bound.

The authoritative Stage-12 dynamic query now explicitly supplies:

~~~cpp
dynamicQuery.lookAheadSeconds = physicalHorizon.lookAheadSeconds;
~~~

Therefore dynamic candidate discovery and LocalHorizonPlanner conflict
evaluation consume the same speed/capability-dependent physical time horizon.
The Config prediction horizon remains only a fallback for callers that do not
provide a physical query horizon.

A new NavigationMap regression proves the distinction: with a 1 s configured
fallback an incoming actor remains outside the local query, while the same
query with 5 s physical look-ahead includes that actor and predicts its crossing
at the correct endpoint.

Combined timing invariant for Navigation v2 is now:

~~~text
physical response/braking need
        -> distance horizon for static/current geometry
        -> time horizon for dynamic prediction
        -> same time horizon for NavigationMap candidate broadphase
        -> accepted short segment may execute only while stopping reserve remains safe
~~~

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 Navigation-v2 purity/isolation audit and safety-math extraction

Current unaccepted candidate code HEAD:

~~~text
c549d3afb740df6cb6a56494c22a2c63c0ad409d
~~~

Navigation-v2 now has an explicit three-level purity contract in:

~~~text
src/game/navigation/NAVIGATION_PURITY_CONTRACT.md
~~~

The architectural rule is:

~~~text
calculation = value-in -> value-out
~~~

unless the component explicitly owns an immutable-at-query-time published
snapshot/index or is inherently sequential authoritative execution.

#### Strict-pure calculation core

The following components are intended to be deterministic value-in/value-out
calculations with no wall-clock/random/I/O/runtime-state dependency:

- PhysicalManeuverHorizon;
- NavigationExecutionSafetyProbeBuilder;
- LocalHorizonPlanner;
- BoundedGapCandidateBuilder;
- MovingGapPredictor;
- MovingPassageTrajectoryEvaluator;
- TrajectoryFollower;
- NavigationExecutionReplanPolicy;
- ManeuverDecisionController.

NavigationFrameBoundary is an immutable value-object transform: its result is
pure for the explicitly captured KinematicFrame and it performs no frame lookup.

#### Snapshot-pure query services

NavigationMap and NavigationSpace intentionally own published/indexed state.
Publication mutates their snapshot:

~~~text
NavigationMap::replaceDynamicWorld(...)
NavigationSpace::replaceStaticWorld(...)
~~~

Their query methods are deterministic/read-only over the current published
snapshot and return products by value. LocalAvoidancePlanner and
NavigationRuntimePlanner are therefore classified as snapshot-pure composition:
they do not mutate runtime state but consume a const NavigationSpace snapshot.

#### Intentionally stateful seam

State is legitimate only in:

- PilotSkillExecutor/runtime bridge: reaction delay, command queue/filter/slew;
- authoritative physics/ShipControlState;
- GameSimulation orchestration, accepted-segment/revision ownership and
  diagnostics;
- replication/network publication/hydration state.

The audit found one real isolation leak: execution-safety kinematics were still
embedded directly inside GameSimulation together with exact-static queries and
diagnostics.

That math has now been extracted into:

~~~text
src/game/navigation/NavigationExecutionSafetyProbeBuilder.h
~~~

It is a stateless pure builder which receives explicit NavLocal kinematics and
returns:

- stopping-reserve segment;
- ideal constant-acceleration forecast segment;
- 12-sample executed-acceleration forecast polyline.

It has no NavigationSpace, NavigationMap, HitVolume, GameSimulation, diagnostics,
clock, random source or I/O dependency.

GameSimulation now only:

1. reads authoritative state;
2. calls the pure probe builder;
3. submits returned segments to authoritative exact-static NavigationSpace
   queries;
4. records invalidation/recovery/diagnostic state.

The existing physical-horizon runtime test now also proves deterministic
same-input/same-output execution-safety probe construction and bounded forecast
geometry.

The Stage-12 architecture contract now checks:

- the pure safety-probe builder exists and remains free of stateful
  dependencies/I/O/time/random sources;
- the principal pure calculation components do not acquire ambient time/random
  or file I/O;
- GameSimulation delegates probe construction to the pure builder;
- the written purity categories remain part of the Navigation-v2 architecture.

No target-machine validation has been run for this candidate yet.
No Stage-12 baseline promotion.

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Current next step: target-machine architecture/runtime/build gates, then the
authoritative headless Navigation self-test. If those pass, evaluate live
behavior before promoting the Stage-12 baseline.


### 2026-09-19 purity gate comment/dependency distinction

Current unaccepted Navigation-v2 code candidate before this documentation sync:

~~~text
e21c81a9acf360b01863729a7c98fed5990273a9
~~~

The new purity architecture gate was corrected so it rejects actual stateful
dependencies/includes in NavigationExecutionSafetyProbeBuilder rather than
matching architecture names that appear only in explanatory comments.

The purity/isolation behavior and code contract are unchanged:

- execution-safety kinematics remain strict value-in/value-out;
- GameSimulation remains the authoritative stateful shell;
- NavigationMap/NavigationSpace remain snapshot owners with deterministic
  read-only query phases;
- PilotSkillExecutor remains intentionally sequential/stateful.

No target-machine validation yet. Last actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 API boundary audit — remaining static-state capability leak

Audit of the current Navigation-v2 purity candidate found one remaining runtime
boundary weakness after the strict-pure math extraction.

The dynamic side is already sealed correctly:

~~~text
NavigationMap state owner
    -> querySphere/queryCorridor API
    -> NavigationMap::QueryResult value DTO
    -> planner
~~~

The static side is not yet equivalently sealed. Both production planners still
receive the state owner itself:

~~~text
NavigationRuntimePlanner::plan(..., const NavigationSpace&, ...)
LocalAvoidancePlanner::evaluate(..., const NavigationSpace&)
~~~

They use only public const query methods, so there is no direct field access,
but the calculation boundary still carries a capability to the stateful
NavigationSpace object. Under the strengthened isolation rule this is considered
a boundary leak.

New rule for Navigation v2:

~~~text
NO state-owner object crosses a calculation boundary.
State stays behind its owner.
Across the boundary pass only:
- immutable/value DTOs; or
- a deliberately narrow read-only query API capability with no publication,
  mutation, storage access or owner escape hatch.
~~~

Current implementation task is therefore to introduce a narrow static-navigation
read API, bind it at the GameSimulation/orchestration edge, and make
NavigationRuntimePlanner/LocalAvoidancePlanner depend on that API rather than on
NavigationSpace itself. Architecture gates must reject a future reintroduction
of `const NavigationSpace&` into those calculation seams.

Broader non-Stage-12 surfaces also deserve the same treatment: the legacy/client
workspace exposes mutable sub-state references, and NavigationFrameBoundary
currently exposes its captured frame by const reference. They are not part of
the active live planner path, but they are recorded for follow-up boundary
hardening rather than being treated as acceptable precedent.

No target-machine validation has been run for this new boundary slice. The last
actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 static-state read API boundary — implementation candidate

Candidate code/contract baseline before this state-document sync:

~~~text
dac32c59ed5c6c9031d3783ae707f603e7cb7e4e
~~~

The Stage-12 calculation boundary has now been tightened to the stronger rule
recorded in the preceding audit: **state-owner objects do not cross calculation
seams**.

Implemented:

- added `NavigationStaticQueryApi`, a narrow read-only capability bound to an
  owned `NavigationSpace`;
- the capability exposes only point/segment/corridor/costed-corridor queries and
  value results;
- it exposes no publication, patch, invalidation, stats/storage view or owner
  accessor;
- `NavigationRuntimePlanner::plan` now receives
  `const NavigationStaticQueryApi&`, not `const NavigationSpace&`;
- `LocalAvoidancePlanner::evaluate` now receives the same narrow API;
- planner/local-avoidance public and implementation surfaces no longer name
  `NavigationSpace::...` DTOs directly; they use aliases exported by the read
  API;
- `GameSimulation` remains the stateful orchestration owner and binds the
  short-lived read capability at the call edge;
- runtime/local behavioral fixtures now pass the explicit read capability;
- architecture gates now fail if either calculation surface regains a direct
  `NavigationSpace&` or `NavigationSpace::...` dependency, or if the read API
  grows owner mutation/escape methods;
- `NAVIGATION_PURITY_CONTRACT.md` and the local-layer contract now state the
  owner/API distinction explicitly.

The dynamic side was already compliant and remains unchanged:

~~~text
NavigationMap owner -> query API -> NavigationMap::QueryResult value -> planner
~~~

Authoritative exact-static safety checks performed directly by `GameSimulation`
remain legitimate because GameSimulation is the orchestration/state owner; they
are not calculation-layer backdoors.

Static inspection of the modified production calculation surfaces shows no
remaining direct `NavigationSpace&`, `NavigationSpace::...` or `staticSpace`
seam in NavigationRuntimePlanner or LocalAvoidancePlanner.

Target-machine compile/runtime validation is still pending. Do not promote the
accepted Stage-12 baseline yet. The last actually accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Required target-machine gate:

~~~bash
python tests/architecture_contracts/check_navigation_local_avoidance.py
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_space/run_mingw64.sh
bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
~~~

Broader follow-up remains recorded separately: legacy/client workspace mutable
sub-state references and the unused NavigationFrameBoundary frame-reference
escape hatch should be hardened after this active Stage-12 API gate is green,
rather than mixing unrelated legacy churn into the same acceptance slice.


### 2026-09-19 legacy route isolation + typed-boundary cleanup

Candidate code/contract baseline before this documentation sync:

~~~text
9ee0fdea3b366cb89633c3abddade270a308c9a9
~~~

Repository audit confirmed that the pre-Stage-12 client docking route pipeline
still exists in `SpaceState`:

~~~text
DockingPathPlanner
  -> GeometricPathPlanner
  -> TrajectoryGenerator
  -> GuidanceTunnel
~~~

It is not part of authoritative `GameSimulation`/Navigation-v2 control, but it
was still runtime-enabled through the old RoutePlanning/LocalGuidance module
switches.

This legacy path is now isolated and disabled in two independent ways:

- `NavigationModuleState` defaults both `RoutePlanning` and `LocalGuidance` to
  OFF;
- `SpaceState::updateDockingGuidance` additionally contains a hard
  `LegacyClientRoutePipelineEnabled = false` gate, so toggling those old module
  switches cannot re-activate the route pipeline accidentally.

Architecture protection now requires the legacy route/tunnel stack to stay out
of `GameSimulation` and `NavigationRuntimePlanner`. The old implementation is
retained only as reference/regression/presentation code for now; no Stage-12
authoritative command may originate from it.

The target-machine log also exposed migration leftovers from the typed
NavLocal/System split. Fixed in this candidate:

- stale `...DemandMap...` follower test names -> NavLocal names;
- stale `...DemandSystem...` control-test names -> current System intent names;
- NPC test kinematics `relativeWorldVelocity/forwardMap/rightMap/upMap` ->
  explicit System-space names;
- replicated HUD execution truth now remains explicitly System-space instead of
  relabelling server acceleration as map-space;
- the last untyped `pointToMap(...)` call in `GameSimulation` now crosses
  `NavigationFrameBoundary` explicitly.

The Stage-12 architecture gate now rejects those stale identifiers and rejects
re-introduction of the removed `pointToMap` helper.

Observed target-machine evidence before these fixes:

- navigation_local: PASS 2/2;
- navigation_space: PASS 1/1;
- navigation_runtime: compile failed on stale renamed identifiers;
- full build: compile failed on the same HUD rename leftovers plus the final
  `pointToMap` call;
- the subsequent navigation self-test result is not acceptance evidence because
  the build had already failed, so that command executed a previously built
  EliteServer binary.

No target-machine validation has been run for this fixed candidate yet. Last
actually accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Next gate is to rerun architecture + runtime + full build. Only after a fresh
successful build should `EliteServer --self-test-navigation` be interpreted as
current behavior evidence.


### 2026-09-19 second legacy route path — repair drones hard-off

Candidate code/contract baseline before this documentation sync:

~~~text
5dbc8ba11bf0e2317fc8e9cc3b4f35b74042a86d
~~~

A second live pre-Navigation-v2 route path was found in
`ObjectRepairJobRuntime`:

~~~text
repair job
  -> GeometricPathPlanner::plan
  -> SmallCraftNavigation waypoint follower
  -> TacticalCollisionMonitor diagnostics
~~~

This is separate from the already-disabled client docking route/tunnel path.
Because `SmallCraftNavigation` treats an empty waypoint list as completed,
simply returning an empty legacy path would be unsafe: the repair state machine
could advance without physical travel.

The repair-drone legacy route pipeline is therefore hard-disabled fail-closed:

- `LegacyRepairDroneRoutePipelineEnabled = false`;
- `startJob(...)` rejects new legacy-navigation repair jobs;
- `update(...)` freezes any pre-existing active job, clears its legacy waypoint
  path and returns no completion event;
- the old implementation remains in place only as migration/reference code;
- architecture tests pin both the OFF gate and the fail-closed update behavior.

This intentionally means automatic repair-drone travel is unavailable until the
repair task is connected to Navigation v2. It is preferable to silently running
a second route planner or falsely completing repair phases.

`SmallCraftNavigation` and `TacticalCollisionMonitor` are therefore not being
mistaken for the new Navigation-v2 runtime: their only confirmed live ownership
was the now-disabled repair-drone legacy path, and they remain forbidden from
`GameSimulation`/`NavigationRuntimePlanner` by the architecture gate.

No target-machine validation yet for this candidate. Last accepted Stage-12
baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 dormant LocalGuidance/Ruckig runtime exclusion

Candidate code/contract baseline before this documentation sync:

~~~text
5a144f7552848abe8b61c2978e3c02b35dc3f755
~~~

The older `LocalGuidancePlanner` / `RuckigRoutePlanner` stack remains in the
repository for regression/reference builds, but no live owner is allowed to
reactivate it. Architecture checks now forbid LocalGuidance/Ruckig dependencies
from `SpaceState`, `GameServer`, `ServerRuntime`, `NpcAiSystem`, and the repair
runtime, in addition to the existing `GameSimulation` / `NavigationRuntimePlanner`
exclusion.

This leaves the old code compiled/reference-only, with both confirmed live route
paths separately hard-disabled.


### 2026-09-19 target-machine pass: runtime green, wrapper/gate cleanup

Candidate code/contract baseline before this documentation sync:

~~~text
92b860fc07377e0fbdb64e78ab833d62bbb05586
~~~

Latest target-machine evidence at checkout `f02d8e67a974714af4fa24b84c57a8110f08d3b4`:

- `navigation_runtime`: PASS 6/6;
- `navigation_local` and `navigation_space` had already passed in the prior run;
- architecture gate 1 failed only because it required the old spelling
  `glm::dvec3(0.0, 0.0, 0.0)` for the static Hub cylinder while the scene now
  uses the equivalent canonical `glm::dvec3(0.0)`;
- architecture gate 2 failed because it still required manual
  normal/radial/prograde dot-product conversion for executed PilotSkill demand,
  while production now correctly crosses through
  `NavigationFrameBoundary::toNavigationVector(SystemVector{...})`;
- full client build failed because `SpaceState` still contained an unused
  diagnostic trace function typed against the removed legacy
  `LocalGuidancePlanner` include.

Corrections in this candidate:

- the dead LocalGuidance docking trace block was removed from `SpaceState`
  instead of reintroducing the legacy planner dependency;
- the static-cylinder architecture gate accepts both equivalent zero-vector
  spellings while still pinning the fixture as non-rotating;
- the executed-demand architecture gate now requires the typed
  `NavigationFrameBoundary` System -> NavigationLocal conversion instead of the
  superseded manual basis projection.

No Navigation-v2 planner/control algorithm changed in this pass. The fresh
runtime 6/6 result is retained as positive evidence, but full target-machine
acceptance still waits for architecture gates + complete build + a newly built
`EliteServer --self-test-navigation`.

Last fully accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


### 2026-09-19 exact-static gate follow-up

Candidate code/contract baseline before this documentation sync:

~~~text
67065a0cd0f888273da0390891fc718569851e3c
~~~

Target-machine gate failure:

~~~text
[FAIL] accepted segment exact-static monitor must prove sampled continuation of the actually executed PilotSkill acceleration
~~~

Root cause: stale architecture-test ownership assumption, not a missing execution-safety mechanism. Production already builds the continuation with
`NavigationExecutionSafetyProbeBuilder::buildSampledConstantAccelerationForecast`, uses
`NavigationExecutionSafetyProbeBuilder::kExecutedForecastSamples = 12`, and checks every adjacent sampled chord through
`exactExecutionSegmentBlocked` while raising
`staticSafetyExecutedForecastBlocked` on collision. The gate still searched for the removed local implementation detail
`ExecutedSafetySamples = 12` inside `GameSimulation.cpp`.

Correction: the Stage-12 architecture gate now verifies the sample-count owner in
`NavigationExecutionSafetyProbeBuilder.h` plus the sampled builder call, sampled-loop traversal, exact-static segment check, and executed-forecast blocking state in `GameSimulation.cpp`.

No Navigation-v2 planner, follower, control law or safety algorithm changed in this pass. Full target-machine acceptance still requires the corrected architecture gates, canonical build, and freshly built server self-test.


### 2026-09-19 full post-failure Stage-12 gate trace

Candidate code/contract baseline before this documentation sync:

~~~text
a8a40a00985c9664e310c7b90c2ac6d4cec2e23f
~~~

Target-machine evidence at checkout `9d2f9120d8b66b7d8b553d5eb92b555552bdd4bf`:

- `check_navigation_foundation_lock.py`: PASS;
- Stage-12 runtime-planner gate advanced through all earlier checks and failed at
  `accepted segment target revision must remain distinct from high-level intent revision`.

Root cause of the reported failure: stale architecture-test ownership. Revision separation is intact in production:

- `AcceptedShortSegment::goalRevision` is the high-level maneuver/goal revision;
- `AcceptedShortSegment::revision` is the concrete accepted short-segment revision;
- `TrajectoryFollower` publishes them as `intent.revision` and `intent.targetRevision` respectively;
- `NavigationFrameBoundary` preserves both fields across NavLocal -> System conversion;
- `NavigationRuntimeControlBridge::toPilotCommand` forwards both revisions to `PilotSkillExecutor`;
- `PilotSkillExecutor` exposes the active concrete target revision through `activeTargetRevision`.

The obsolete gate incorrectly required a duplicate `targetRevision` field to be declared in
`NavigationRuntimeControlBridge.h`, although the bridge now aliases the typed
`NavigationSystemControlIntent` DTO and should not duplicate that state.

A full audit of every architecture condition after the failing check found one additional stale marker before rerunning the target machine: the live moving-gap kinematic proof now correctly owns
`expectedMovingGapVelocityMapMps` in NavLocal, while the gate still searched for the superseded
`expectedMovingGapVelocityWorldMps` name. That marker was corrected in the same candidate.

All other remaining post-failure contract markers were checked against current source: authoritative self-test evidence, slit/tunnel topology and capture, exact-static physical sweep, bounded visibility recovery, ManeuverDecision doctrines, automatic/manual replan policy, Assisted/Newtonian control-law contract, and client/server CMake ownership. No additional stale marker was found in that remaining portion of the gate.

No Navigation-v2 planner/control/safety behavior changed in this pass. Full target-machine acceptance still requires the refreshed Stage-12 gate, canonical build, and freshly built server self-test.


### 2026-09-19 live self-test reached rotating-infrastructure precision gate

Candidate code/contract baseline before this documentation sync:

~~~text
a611aaec5d01368dcd6c13a1b40a30f3197a7f6b
~~~

Target-machine evidence at checkout `8c32e5a72084646933e959d8308037bd6fbe9f3b`:

- foundation architecture gate: PASS;
- Stage-12 runtime-planner architecture gate: PASS;
- navigation runtime tests: PASS 6/6;
- canonical client build: PASS;
- canonical headless server build: PASS;
- freshly built `EliteServer --self-test-navigation` entered the real live proving run and failed only at rotating-infrastructure angular-velocity verification.

Observed witness:

~~~text
expected=(0,0,0.0349066)
observed=(1.33529e-10,6.92304e-10,0.0349066)
error=7.11278e-10 rad/s
~~~

Root cause: diagnostic precision contract was stricter than the authoritative source representation. `StaticObject::angularVelocity` is currently a `glm::vec3` float field. The hub/local angular velocity is computed in double, quantized once into that float field, then promoted back to double and crossed through `NavigationFrameBoundary::SystemAngularVelocity -> NavAngularVelocity`. The previous `1e-12 rad/s` acceptance threshold therefore demanded precision the authoritative source does not contain; the measured error is consistent with one float quantization at ~0.035 rad/s and does not indicate a frame-transform defect.

Correction: `NavigationRuntimeLabAngularVelocityToleranceRadPerSecond` is now `1e-8`, with the source-precision rationale pinned next to the constant. This remains far below any physically meaningful angular-velocity error for the lab while admitting the actual authoritative storage precision.

No planner, follower, collision, replan or control-law behavior changed in this pass. Next target-machine step is a canonical rebuild followed by the live navigation self-test so execution can proceed to the next physical acceptance gate.


### 2026-09-19 live visibility deadlock — dynamic sphere broadphase replaced by exact OBB narrow-phase

Candidate code/contract baseline before this documentation sync:

~~~text
cf224145d758d09d35d70c63324c97d02f619e60
~~~

Fresh target-machine live self-test evidence from checkout `93fa488b8a5b226134309b3486e96e00c93f9afb`:

- both architecture gates: PASS;
- navigation runtime tests: PASS 6/6;
- canonical client/server build: PASS;
- rotating infrastructure proof: PASS after source-precision correction;
- live flight remained collision-free for 120 s but stalled after ~351 m of progress;
- `visibilityBypassSeen=1`, later `ConflictHold` occurred, `movingGapPlanePassed=0`, `visibilityDirectRecoveredSeen=0`;
- moving-gap kinematic verification reported ~0.001967 m/s residual.

Root cause of the flight deadlock: `NavigationMap` published each moving 360 x 360 x 900 m guidance box only as a conservative enclosing sphere. Its radius is about 517 m before agent/safety inflation. At roughly the observed 350 m route progress the ship enters that conservative sphere while still outside the real HitVolume OBB. `LocalHorizonPlanner` then treated the broadphase sphere as final collision truth at t=0, so every visibility probe became dynamically conflicting and the actor fell into repeated `ConflictHold` instead of continuing around the real box.

Architecture correction in this candidate:

- `NavigationMap::DynamicActorInput` and query `Candidate` now carry optional value-owned exact NavLocal `NavigationObstacle` geometry in addition to the swept-sphere broadphase radius;
- authoritative dynamic infrastructure publishes its current HitVolume-derived OBBs through `NavigationHitVolumeAdapter`, explicitly transformed System -> NavLocal at the existing typed boundary;
- `LocalHorizonPlanner` keeps the swept sphere for candidate collection but, for translation-only dynamic shapes with negligible angular/acceleration terms, re-tests both current kinematics and the bounded requested corridor against the real moving OBBs in the obstacle translating frame;
- rotating/accelerating dynamic geometry remains conservative sphere fallback until a dedicated continuous swept-OBB solver owns those motion classes;
- regression `testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute` pins the failure mode: sphere overlaps the route while exact OBB + ship envelope remains clear;
- NavigationMap contract now pins exact geometry as value-owned NavLocal state rather than an owner reference.

The ~0.001967 m/s moving-gap velocity residual has the same representation cause as the earlier angular-velocity residual: `StaticObject::linearVelocity` is currently `glm::vec3` while hub world velocity is orbital-scale. The live diagnostic tolerance is now a named `NavigationRuntimeLabLinearVelocityToleranceMps = 1e-2`, tight enough for the lab but consistent with the authoritative source precision.

No static exact-HitVolume authority was weakened. The actual authoritative ship sweep remains checked every fixed step against NavigationSpace exact static geometry. This change only prevents a dynamic broadphase sphere from overriding more precise dynamic HitVolume truth when that truth is available.

Next target-machine gate: architecture contract + navigation map/local/runtime tests + canonical build + fresh live self-test. The expected behavioral change is that the ship no longer stalls inside the moving box's enclosing sphere and can proceed through visibility bypass, direct recovery, portal capture and tunnel transit.


### 2026-09-19 dynamic OBB gate ownership correction

Candidate code/contract baseline before this documentation sync:

~~~text
12a759f725a903b047d198e9daf7aaf6939640b9
~~~

Target-machine architecture failure:

~~~text
[FAIL] dynamic conservative spheres must narrow against exact translation-only OBB geometry instead of sealing real free space
~~~

Root cause: the architecture gate searched for the new dynamic narrow-phase implementation in `LocalAvoidancePlanner.cpp`, but the implementation deliberately belongs to `LocalHorizonPlanner.cpp`, where dynamic candidate collision truth is evaluated. Production ownership is therefore correct; the contract pointed at the wrong owner file.

Correction: the gate now loads `LocalHorizonPlanner.cpp` explicitly and verifies `exactTranslationNarrowPhaseAvailable`, `exactTranslationConflict`, `segmentIntersectsNavigationObstacle`, exact candidate geometry consumption, and the regression `testDynamicSphereBroadphaseDoesNotSealClearExactObbRoute` against the correct owner.

The remainder of the new slice was rechecked against source: NavigationMap exact-geometry value ownership, live HitVolume OBB publication, and named moving-infrastructure velocity tolerance markers are all present. No navigation behavior changed in this follow-up; the dynamic exact-OBB candidate remains the implementation baseline awaiting target-machine compilation/runtime validation.


### 2026-09-19 exact-OBB broadphase fix exposed a non-blocking moving fixture

Candidate code/contract baseline before this documentation sync:

~~~text
18076a9851647fcf9eb1b237105d0de4bdfd3440
~~~

Fresh target-machine live evidence after the dynamic exact-OBB narrow-phase candidate:

- canonical build: PASS;
- moving-gap kinematics: PASS (`moving_gap_kinematics=1`);
- no dynamic `ConflictHold` and no exact-static violation;
- progress increased from ~351 m to ~1903 m inside the same 120 s run;
- slit portal selection/capture/alignment/entry-plane crossing all became visible;
- however `visibility_bypass=0` and `moving_gap_passed=0`, so the self-test still failed its ordered visibility-bypass evidence gate.

This is not a regression in avoidance. The exact geometry revealed that the authored moving pair did not actually block the route. The original centres were Y=-790 and Y=-1890 with 360 m-high GuidanceDockCube HitVolumes, leaving about 740 m of real vertical free space. The lab route is Y=-1300, almost through the middle of that gap. The old sphere-only broadphase falsely made this look blocked; once the real OBB narrow-phase became authoritative, the correct result was direct flight with no visibility bypass.

The proving fixture is corrected rather than weakening the self-test. The moving pair now has:

~~~text
aperture center Y = -900 m
boundary centre separation = 500 m
GuidanceDockCube height = 360 m
physical aperture = 140 m
route Y = -1300 m
~~~

The direct route therefore intersects the lower real OBB, while a bounded upward visibility deflection can enter the actual 140 m aperture. This preserves the intended test: NavigationMap sphere performs broadphase candidate collection, exact dynamic HitVolume OBBs decide collision truth, and bounded visibility steering must find real free space rather than react to an enclosing-sphere artefact.

The Stage-12 architecture gate now pins the explicit offset-aperture constants/derivation so this fixture cannot silently drift back into a geometry that requires no avoidance.

No planner/control-law behavior changed in this follow-up. Next target-machine run should prove that the corrected real geometry now produces `visibility_bypass=1` without restoring the previous false `ConflictHold` deadlock.


### 2026-09-19 corrected moving aperture: bypass+replication proven, forward progress still incomplete

Target-machine checkout actually exercised:

~~~text
46f6a37da6775a1d044391f773476df1bb07bc6a
~~~

Latest live result:

~~~text
[FAIL] visibility-bypass replication succeeded but the ordered live flight did not complete moving-pair bypass, direct recovery and exact-static tunnel passage inside the 120 s bound
moving_gap_passed=0
slit_portal=0
slit_entry_capture=0
slit_entry_crossed_aligned=0
passed_obstacle_plane=0
exact_static_violation=0
simulated_s=120
slit_entry_cross_track_m=148.3
~~~

Interpretation:

- the corrected offset moving aperture now does produce the required visibility bypass strongly enough that the self-test reaches and completes the same-tick replication proof;
- the dynamic exact-OBB broadphase/narrow-phase slice is therefore active in the real authoritative chain rather than only in isolated tests;
- no exact-static physical collision occurred;
- however the actor does not complete the moving-pair bypass/plane crossing and does not recover to the direct route inside the 120 s acceptance window;
- the static slit/tunnel phase is not reached in this run (slit_portal=0), so the current failure is upstream of tunnel capture;
- slit_entry_cross_track_m=148.3 is diagnostic state only here; no slit waypoint/capture was active.

This is now a behavior/execution problem after a valid visibility bypass, not a stale architecture gate and not the old false-sphere deadlock. The next task is to trace the accepted adjusted segment after bypass: selected target, segment expiry/completion, replanning reason, executed demand, actual ship progress relative to the moving-pair plane, and the condition that should switch back to the direct nominal target. Do not weaken the ordered self-test or reintroduce sphere collision authority to make it pass.

The last fully accepted Stage-12 target-machine baseline remains the previously recorded accepted baseline; checkout 46f6a37da6775a1d044391f773476df1bb07bc6a is a tested candidate, not accepted, because the live ordered-flight gate still fails.


### 2026-09-19 end-to-end chain audit: failure localized to LocalVisibility -> accepted execution semantics

Repository state analyzed: `794b02a803b91e47bc991c8faa932a02c3184d0c`. Latest target-machine behavior evidence remains checkout `46f6a37da6775a1d044391f773476df1bb07bc6a`.

Current failure is NOT inability to find a route and NOT inability to move. The live run already proves a real visibility bypass strongly enough to pass same-tick replication, while exact-static collision remains false. Static/global topology has also previously carried the same actor to slit portal capture when the moving pair did not physically block the route.

End-to-end source audit localized the integration defect to the ordinary local-visibility maneuver handoff:

1. Scene/publication is coherent: the lab starts at visual/NavLocal (975,-1300,-6200); the corrected moving lower OBB is centered at (975,-1150,-5700), 360 x 360 x 900 m, and the moving aperture is real. Dynamic OBB truth reaches LocalHorizon through NavigationMap value DTOs.
2. Global NavigationSpace corridor is coherent: the first oriented slit entry portal is at z=-4950 with a 500 m approach, so the coarse approach waypoint is approximately (975,-1180,-5450). From the start the nominal coarse vector is (0,+120,+750), length about 759.5 m.
3. LocalHorizon/LocalAvoidance correctly detect the real dynamic block and can produce an AdjustedClear visibility ray. However this ordinary fan proves geometry only. LocalHorizon AgentState has no directional linear/angular capability and the ordinary fan never time-parameterizes the ray against the craft's anisotropic propulsion.
4. NavigationRuntimePlanner AgentState DOES contain real linear/angular capability and Newtonian/Assisted mode, but those fields are currently consumed only by the precision MovingPassageTrajectoryEvaluator. The live lab explicitly disables MovingPassage steering authority for this ordinary moving-pair encounter, so the selected visibility ray bypasses the capability-aware trajectory layer.
5. A second concrete handoff defect changes maneuver semantics when the planner result is packed into AcceptedShortSegment. During AdjustedClear, NavigationRuntimePlanner itself does not request portal alignment; its angular intent only damps angular velocity. GameSimulation nevertheless sets AcceptedShortSegment.alignForward = lastPlan.portalTraversalActive and desiredForward = portalNormal. Therefore the downstream follower forces the hull to face the later slit portal even while the local planner is commanding a temporary lateral bypass.
6. The latest failure numerically confirms that premature alignment is active upstream of the portal: slit_portal=0 while slit_entry_fwd_angle_rad=0.000679006, i.e. the hull is already almost exactly aligned to the +Z portal normal before reaching the slit phase.
7. Cobra propulsion is highly anisotropic: main forward authority is maxLinearGs 7.5 (~73.55 m/s^2), while physical manoeuvre/RCS authority is only 2 m/s^2. DynamicMotionSystem correctly decomposes the requested System acceleration into forward-only main-engine thrust plus a 2 m/s^2-clamped remainder. Thus a steep visibility ray cannot be executed as planned while the accepted segment simultaneously pins the nose to +Z.
8. Example reconstruction from the actual fixture (illustrative, not the exact latest unlogged selected azimuth): a steep ~75 degree downward visibility ray can request roughly desired velocity (0,-54.8,+24.5) m/s and ideal acceleration from rest about (0,-41.1,+18.4) m/s^2. With the hull pinned near +Z, physics can provide the +Z main-engine component but only about 2 m/s^2 laterally. The geometrically proven ray and the physically executed trajectory therefore diverge materially.
9. TrajectoryFollower, NavigationFrameBoundary, NavigationRuntimeControlBridge, PilotSkillExecutor and DynamicMotionSystem are individually coherent: they preserve revisions/vectors, cross NavLocal/System explicitly, model reaction/latency/filtering, and enforce real propulsion authority. The wrong assumption has already entered the accepted execution product before these layers.
10. ManeuverDecisionController and CONTROL_LAW_MANEUVER_MODEL already document the required architecture: Navigation should generate physically truthful control-law-compatible maneuver candidates (Newtonian coast/drift/RCS trim/turn-then-burn/etc.), then the decision owner selects among them. The current live lab path does not invoke ManeuverDecisionController for ordinary visibility and still converts a geometric visibility ray directly into an arbitrary acceleration request.

Primary defect boundary: `LocalAvoidance geometric AdjustedClear -> NavigationRuntimePlanner desired velocity -> GameSimulation AcceptedShortSegment`. Two corrections are required by contract, not test weakening: (a) do not inherit later portal forward-alignment while executing an AdjustedClear bypass unless that specific maneuver requires it; (b) ordinary visibility candidates must become capability/time-aware or escalate to a control-law-compatible trajectory primitive before acceptance. Moving the fixture farther away could mask the issue but would not repair this contract.

The exact selected deflection/replan sequence in the latest target-machine run is not printed by the second-phase failure, so the next implementation/debug pass should add bounded event diagnostics (only on plan/replan/bypass transitions) for selected target/deflection, accepted alignment, ideal/executed/applied acceleration, replan reason and route/moving-plane progress. This will target-machine-confirm the predicted divergence without producing per-frame log spam.


### 2026-09-19 behavior-character and vehicle-feasibility contract tightened

Architecture baseline for this decision: `e2d4238968a1b3f360eb342ced2ca0d4d782b5e7`.

Three current defects are now explicit architecture blockers:

1. A geometric path/ray must never become an executable route or AcceptedShortSegment unless it is feasible for the actual vehicle from the current state. Global/topological routing may stay coarse, but every edge/portal must be vehicle-feasible at its abstraction level; local accepted segments must be time-parameterized and dynamically feasible from current P/V/A/attitude/angular state under real propulsion and damage-degraded capability.
2. For a main-engine-dominant Newtonian craft, substantial delta-v should normally come from rotating the hull to a burn vector and using the main engine, with coast/rotate/brake/flip-and-burn as required. The small manoeuvre/RCS system is primarily trim/precision/parking/docking/capture authority, not a hidden omnidirectional main engine. A craft whose real propulsion profile differs may legitimately use omnidirectional thrusters as primary translation.
3. Behavior character is not one scalar. It has two independent inputs: (a) situation/doctrine, such as ordinary/rational, extreme attack/escape, or precision ingress/retrieval; and (b) pilot model/transient pilot state. Doctrine changes candidate generation/ranking preferences and accepted risk bands. Pilot skill changes reaction/latency, anticipation, control precision, overshoot/damping, hull/clearance judgement uncertainty and execution envelope. Neither may modify world geometry or vehicle capability.

New canonical document: `src/game/navigation/NAVIGATION_BEHAVIOR_CHARACTER_MODEL.md`.

`CONTROL_LAW_MANEUVER_MODEL.md` now explicitly requires main-engine-oriented Newtonian course changes and forbids relying on the propulsion allocator to rescue an impossible arbitrary acceleration request. `MANEUVER_DECISION_TREE.md` now states `geometric path != executable route` and separates doctrine from pilot ownership.

Current live failure remains localized at the ordinary visibility handoff. The next implementation must not merely patch alignment; it must establish the missing sequence `free-space candidate -> control-law-compatible maneuver generation -> capability/pilot-aware continuous proof -> decision -> accepted segment`. The existing premature future-portal alignment bug is still a concrete defect inside that seam and must be removed as part of the correction.


### 2026-09-19 pipeline audit activated; first P9 handoff defect corrected

Code/contract candidate before documentation commits: `db79542ad0547f34dfadcb933bd1135067c145c7`.

Canonical audit: `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`. Navigation is now reviewed left-to-right as INPUT / RESPONSIBILITY / OUTPUT / HANDOFF / PERFORMANCE contracts; isolated green tests are not sufficient if the handoff changes semantics or omits required truth.

Scaling contract: avoid dense N^2 work. Current NavigationMap already uses a spatial hash and bounded local queries. Target architecture is scene-wide sparse broadphase -> unordered potentially-interacting pairs -> batch/SIMD kinematic filtering -> per-agent influence lists -> exact narrow phase only for survivors. Per-agent duplicate discovery is an optimization follow-up, not the current live failure.

Current audit localization: P5 LocalAvoidance produces geometric free-space candidates but ordinary AdjustedClear is not yet vehicle-feasible; P6 physical maneuver generation is missing for ordinary visibility; P7 continuous capability/geometry proof exists in precision components but is not applied there; P8 ManeuverDecisionController is bypassed by the ordinary live chain. P9 had a concrete semantic handoff defect and is corrected in this candidate.

P9 correction: NavigationRuntimePlanner now publishes `selectedManeuverRequiresForwardAlignment` plus `selectedManeuverForwardMap`. Only the selected current nominal portal capture/transit maneuver sets them. GameSimulation copies these fields into AcceptedShortSegment and no longer derives alignment from future route context (`portalTraversalActive`). A regression and architecture gate pin that AdjustedClear with a future portal must not inherit portal-forward alignment.

One-shot first-bypass diagnostics now capture P5->P9->P13 evidence: selected deflection/target, agent P/V, accepted attitude requirement, follower ideal acceleration, PilotSkill executed acceleration, and physically applied main/RCS/total acceleration. The next target-machine run will therefore expose the exact divergence without per-frame logging.

No acceptance promotion. Last actually exercised live checkout remains `46f6a37da6775a1d044391f773476df1bb07bc6a`; last fully accepted baseline remains the recorded accepted baseline. Candidate `db79542ad0547f34dfadcb933bd1135067c145c7` still needs target-machine verification.

Next repair stage is P6/P7: free-space rays become candidates only; for main-engine-dominant Newtonian craft they must be converted into real rotate/main-burn/coast/trim/brake or flip-and-burn primitives and continuously proven against capability/geometry/pilot uncertainty before ACCEPT.


### 2026-09-19 command ownership and accepted maneuver API corrected

Architecture/code candidate before documentation commits: `3aa0639e72d094ae95ecea041c8926f0dfecf59a`.

Portal semantics were corrected conceptually: portal existence must not imply mandatory centerline capture or hull alignment. The correct question is which passage maneuver is safe/acceptable for the current opening geometry/motion, vehicle capability, pilot execution uncertainty and Situation/Doctrine. A wide, slow, high-margin portal in Ordinary/Rational behavior may be crossed as FreeTransit without stopping or forced alignment; tight/fast/precision cases may require PrecisionCapture/Transit; Extreme behavior may accept smaller margin/higher load/contact according to explicit policy.

New canonical document: `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`.

Ownership is now fixed as:
- objective/mission owner selects semantic goal/terminal contract;
- global route selects topology/corridor/portal opportunities, not control;
- maneuver planner compiles the next physically valid bounded maneuver;
- accepted product must contain the same time-parameterized reference state and feed-forward control that were proved: P(t), V(t), A_ff(t), q/body-basis(t), omega(t), alpha_ff(t);
- follower samples that program and adds only bounded tracking feedback; it does not invent a new trajectory/target velocity;
- PilotSkill models reaction/latency/precision around the ideal program;
- propulsion allocator maps vehicle-level acceleration/attitude demand to real main-engine/RCS/torque actuators;
- physics remains final authority.

This means the current `AcceptedShortSegment` + `TrajectoryFollower` API is transitional. Today it carries target position/velocity or one fixed acceleration and the follower re-derives acceleration. That can diverge from the trajectory that was actually proved. `NavigationRuntimePlanner.h` and `AcceptedShortSegment.h` are now explicitly marked transitional toward `AcceptedManeuverProgram`.

Newtonian turn semantics were also tightened: turn does not imply stop-turn-go. A main-engine-dominant craft should preserve useful inertial velocity, rotate the hull ahead of the required future delta-v, begin main-engine burn while V remains non-zero, bend the velocity vector continuously, then coast/trim and rotate early for the next burn or flip-and-burn if braking is needed. Angular authority and pilot reaction/latency determine lead-rotation time.

`NAVIGATION_PIPELINE_AUDIT.md`, `CONTROL_LAW_MANEUVER_MODEL.md`, and `TRAJECTORY_EXECUTION_REPLAN_MODEL.md` were updated to this ownership/API contract.

No live acceptance promotion. The next implementation slice should introduce the bounded AcceptedManeuverProgram representation and migrate the runtime-lab execution seam so proof and execution consume the same program before building the full ordinary Newtonian maneuver generator.


## 2026-09-19 planner/follower two-world architecture analysis

Architecture analysis is now canonical in:

`src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`

Decision:
- expose two top-level runtime worlds: Planner and Autopilot/Follower;
- Planner consumes authoritative world truth directly; obstacle rays are not perception;
- retain segment/sweep/ray intersection only as geometry/proof primitives;
- replace the LocalAvoidance angular ray-fan as the target ordinary search strategy with a route-aligned configuration-space corridor solver;
- keep NavigationSpace/global topology above that local solver because one A->B longitudinal frame cannot represent arbitrary backtracking/topology changes;
- Planner must compile/prove and publish the exact bounded `AcceptedManeuverProgram` that execution consumes;
- Follower tracks/recoveries only inside the accepted envelope and does not become a hidden normal planner;
- bounded imminent-hazard reflex is permitted inside the autopilot world, but material deviation invalidates the accepted program and immediately requests local replan;
- shared dynamic broadphase/pair isolation should become scene-wide/batched, while planner work remains event-driven for only agents needing a new program.

Current repository comparison:
- NavigationMap authoritative snapshot + spatial hash already match the world-truth model;
- NavigationExecutionReplanPolicy already matches plan -> accept -> execute -> monitor;
- TrajectoryFollower is already obstacle-search-free but still consumes transitional `AcceptedShortSegment`;
- ordinary `AdjustedClear` is still only geometrically clear and the visibility fan remains the primary local search method;
- scene-wide sparse pair/influence computation is still a target, not the current per-agent query implementation.

Implementation order remains conservative:
1. introduce `AcceptedManeuverProgram`;
2. migrate `TrajectoryFollower` to sample the same proved program + bounded feedback;
3. then prototype `RouteAlignedCorridorPlanner` beside LocalAvoidance and A/B test it;
4. add physical Newtonian/Assisted maneuver compilation;
5. retire the ray-fan search only after target-machine evidence.

This iteration changes architecture documentation only. It does **not** promote a new target-machine accepted Stage-12 baseline. Last fully accepted target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`; last actually exercised target checkout remains `46f6a37da6775a1d044391f773476df1bb07bc6a`.


## 2026-09-19 canonical B0-B14 architecture and B8/B9 migration slice

Canonical Navigation v2 architecture is now fixed in:

- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
- `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`

Top-level ownership is two worlds:

```text
Planner World:
    B0 World Snapshot
    B1 Shared Influence Builder
    B2 Navigation Objective
    B3 Topology Route
    B4 Route-Aligned Local Corridor
    B5 Physical Maneuver Compiler
    B6 Continuous Maneuver Prover
    B7 Maneuver Decision
    B8 Maneuver Acceptance / Program Store

Autopilot / Follower World:
    B9 Maneuver Program Sampler
    B10 Bounded Tracking Controller
    B11 Safety Monitor / Bounded Reflex
    B12 PilotSkill
    B13 Propulsion / Physics

Scheduling service:
    B14 Navigation Work Scheduler
```

Each canonical block now has an explicit owner, question, invocation cadence, input, output, forbidden responsibilities and scaling contract.

The current repository was audited against those blocks. The important preservation/migration result is:

- KEEP: NavigationSpace static topology/exact geometry foundation;
- KEEP: NavigationMap spatial hash/broadphase foundation;
- KEEP: PilotSkill and propulsion/physics authority;
- KEEP: NavigationExecutionReplanPolicy rule that another frame alone does not wake planning;
- MIGRATE: per-agent dynamic discovery toward shared scene-wide sparse InfluenceFrame;
- REPLACE AS TARGET SEARCH: LocalAvoidance angular ray-fan with a route-aligned configuration-space corridor planner, while retaining segment/sweep/ray intersection as proof primitives;
- ADD: ordinary physical maneuver compiler and generalized continuous proof;
- USE: existing ManeuverDecisionController only after candidates are physically proved;
- REPLACE BOUNDARY: AcceptedShortSegment with AcceptedManeuverProgram;
- SPLIT: program sampling from bounded feedback tracking;
- ADD: explicit bounded safety-reflex API;
- ADD: dirty-agent planning scheduler with urgent/normal/background queues.

Scaling canon is explicit: shared B0/B1 work for the scene; cheap fixed-step execution for active controlled actors; expensive B3..B8 work only for dirty agents. No dense N x N and no plan-every-frame. B14 will budget queued planner jobs, reject stale revisions, prioritize urgent invalidations and provide starvation prevention.

### Iteration 1 implementation candidate — B8/B9

Code/contract baseline before documentation commits:

```text
516f63eb7a3b2bbf09bad553aff975d52d7c3e8c
```

New production API:
- `AcceptedManeuverProgram.h` — fixed-capacity 16-sample value-owned program carrying P/V/A_ff, body basis, omega/alpha_ff, validity/revisions, terminal tolerances, tracking reserve, capability and proof witnesses;
- `ManeuverProgramSampler.h/.cpp` — pure time sampler with no world query, no obstacle search, no feedback controller and no replanning.

New isolated runtime test:
- `ManeuverProgramSamplerTests.cpp`;
- wired into `tests/navigation_runtime` as `maneuver_program_sampler`;
- sampler implementation also linked into the production `EliteNavigationWorldRuntime` target.

The live GameSimulation chain intentionally still uses AcceptedShortSegment. This slice introduces and tests the clean B8/B9 API before live migration.

Target-machine gate is pending:

```bash
bash tests/navigation_runtime/run_mingw64.sh
bash build_mingw64.sh
```

No Stage-12 acceptance promotion is claimed from this documentation/code pass. Last fully accepted target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.


## 2026-09-19 B8/B9 accepted; B10 bounded tracking candidate

Target-machine verified checkout:

```text
701881ddae861cd5593e425de91600e048bd417c
```

Evidence supplied by user:
- `navigation_runtime`: 7/7 PASS;
- `maneuver_program_sampler`: PASS;
- canonical `EliteGame`: BUILD PASS;
- canonical `EliteServer`: BUILD PASS.

This accepts the first clean execution-boundary slice B8/B9:
`AcceptedManeuverProgram` + `ManeuverProgramSampler`.

Current B10 candidate baseline before documentation commits:

```text
66d89b93e3edf0817bb8d88b78e405180887cecc
```

B10 is now an explicit `ManeuverTrackingController` block. It consumes one
B9 reference sample plus actual vehicle kinematics and may add only bounded
feedback inside the reserve carried by `AcceptedManeuverProgram`.

The new Navigation-v2 TrajectoryFollower path is:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> ManeuverTrackingController
 -> NavigationLocalControlIntent
```

The old `AcceptedShortSegment` overload remains live for compatibility.

Architecture contracts now forbid B8/B9/B10 from depending on
NavigationMap, NavigationSpace, GameSimulation, NavigationRuntimePlanner,
planner/world query entry points or unbounded `std::vector` hot-path storage.

New regression `maneuver_tracking_controller` pins:
- zero tracking error => exact `A_ff/alpha_ff`;
- tracking feedback is clamped to the accepted reserve;
- B9 -> B10 composition inside TrajectoryFollower;
- terminal completion uses accepted tolerances;
- execution before acceptance time fails closed.

`tests/navigation_runtime/run_mingw64.sh` now prints configure/build/test/total
phase timings. No persistent log is currently required. If later diagnosis
creates one, the invoking script/command must print the exact path at completion.

B10 target-machine gate is pending; do not promote the candidate until that
evidence is supplied.
