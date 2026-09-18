# Elite — CURRENT STATE

**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Last target-machine verified baseline:** `daaf038021cdf8b9561db60fdd35e7cefce0b2df`

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
- 12A-6b3a verified moving-passage steering authority seam — ACCEPTED
- 12A-6b3b live moving-passage physics/replication authority gate — CANDIDATE / target-machine pending

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


## 12A-6b3a acceptance

Target-machine gate completed from:

```text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
```

Accepted evidence:

```text
Stage-12 architecture contract   PASS
navigation_runtime               3/3 PASS
navigation_trajectory            11/11 PASS
EliteGame                        BUILD PASS
EliteServer                      BUILD PASS
rebuilt server self-test         PASS
exact_static_violation           0
replication_error_mps2           0
canonical_replication_error_mps2 0
```

This accepts the deterministic steering-authority seam:
a moving passage may take planner authority only under explicit opt-in after
both dynamic feasibility and same-trajectory exact-static safety are proven,
and the exact first Hermite acceleration sample crosses map->world and
PilotSkillExecutor without re-planning.

## Active 12A-6b3b boundary

Next step is live production evidence, not another planner algorithm:
enable the accepted moving-passage authority for a deterministic live moving-gap
fixture and prove `MovingPassageClear` reaches authoritative physics and
same-tick sparse/canonical replication without static violations.


## Deferred strategic navigation task

Galactic gravity/fuel/jump routing is now explicitly tracked in:

`src/game/navigation/GALACTIC_ROUTE_PLANNING.md`

It is a separate future layer above the current System / Local / Precision stack.
The strategic planner must account for jump-drive range/capability, fuel and
reserve policy, gravity-aware long-leg trajectory validation, timing/ephemerides,
hazards and replanning.

Manual and automatic execution must consume the same accepted route solution:
manual mode presents guidance; autopilot executes it. No second planner is
allowed for presentation.

This deferred task does not change the active Stage-12 priority. Current work
remains 12A-6b3b live moving-passage physics/replication authority.


## 12A-6b3b candidate now on main

Candidate code/contract baseline before documentation commits:

```text
d5e1f990aed38ded7a517d719e935b6b6e763d4d
```

The previous live lab had only one dynamic candidate, so it could never form a
two-boundary moving aperture. The 12A-6b3b candidate adds two real hub-attached
physical boundaries:

```text
NAV MOVING GAP UPPER
NAV MOVING GAP LOWER
```

They translate deterministically in the live Hub Motion Lab, remain excluded
from persistent exact-static NavigationSpace ownership, and are published into
NavigationMap as the dynamic pair used by the real planner.

The live NavigationRuntimeLab now supplies the Cobra's real logical hull and
physical body-axis authority to moving-passage evaluation instead of unit-test
capabilities.

The production policy explicitly enables both moving precision and steering
authority for this diagnostic actor. Acceptance now requires live evidence of:

```text
both moving boundaries in the ship's bounded candidate set
    -> verified map-frame moving kinematics
    -> moving precision attempted
    -> expected boundary pair feasible
    -> same Hermite trajectory exact-static safe
    -> MovingPassageClear authority
    -> non-zero PilotSkillExecutor execution
    -> non-zero authoritative physical acceleration
    -> same-tick sparse/canonical execution replication
    -> physical ship passes the moving aperture plane
    -> zero exact-static physical violation
```

CUBE 08 remains the independent stationary exact-HitVolume proving obstacle;
the new moving pair does not replace or weaken that accepted gate.


## 12A-6b3b first live gate — FIXTURE FAILURE

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


## 12A target evidence + maneuver-decision ownership

Latest target-machine run:

```text
f8012a3c903902c7fc3050e8f82ee2f4b674c76d
```

Evidence:
- Stage-12 architecture contract PASS;
- navigation_space 1/1 PASS;
- navigation_runtime 3/3 PASS;
- navigation_trajectory 11/11 PASS;
- canonical EliteGame / EliteServer build PASS;
- bounded-visibility live bypass executed and replicated;
- `moving_gap_passed=1`;
- `exact_static_violation=0`;
- terminal tunnel capture did not complete inside 120 s:
  `slit_portal=0`, `slit_entry_capture=0`,
  forward error `0.169452 rad`, cross-track `96.8553 m`.

The only isolated unit failure was the new widened-visibility fixture: its
geometry accidentally admitted a safe <=30-degree direction while the assertion
required >30 degrees. Fixture corrected at
`fab1a0b1b87f7210988780c30d2b0e7add9cd064`; rerun pending.

Architecture has now been refined above Navigation. Candidate code/contract
baseline before these documentation commits:

```text
7ee5dd320163fae40a1178535730efca494eb96c
```

New authority:
`src/game/MANEUVER_DECISION_TREE.md`

New production selector:
`src/game/ship/controller/ManeuverDecisionController.{h,cpp}`

Decision ownership:
- Navigation/trajectory generate safe and contact-expected maneuver products;
- damage/structural annotates semantic loss (radar/fairing vs drive/reactor/
  cockpit, mission payload, detachable/expendable parts);
- threat/combat annotates projected silhouette/exposure;
- ManeuverDecisionController selects doctrine-dependent action;
- PilotSkillExecutor executes the selected intent;
- physics/contact/damage remain final truth.

Pinned doctrines:
`Rational`, `PrecisionRetrieval`, `Extreme`, `CombatEscape`.

Critical invariant:

```text
no collision-free proof != no navigation command
no global route != automatic hold
```

`StaticHold` / `ConflictHold` remain valid safe-planner outcomes, but they
must not become the permanent final command before the maneuver decision layer
has considered stop/backtrack/local progress/precision/emergency alternatives.

Last target-machine accepted Stage-12 baseline remains
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.
None of the current selector/capture changes are target-machine accepted yet.


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
