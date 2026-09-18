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
