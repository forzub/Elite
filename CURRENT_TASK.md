# Elite — CURRENT TASK

**Updated:** 2026-09-18 Europe/Kyiv  
**Stage:** 12A-6b3b — live moving-passage physics/replication authority gate  
**Last target-machine verified baseline:** `daaf038021cdf8b9561db60fdd35e7cefce0b2df`  
**Candidate implementation baseline before documentation commits:** `616f5b868795439fb42308d2c2d13ffc87218cba`

## What changed

The real shared `NavigationRuntimePlanner` can now consume its already-bounded
`NavigationMap::QueryResult` after a nominal dynamic conflict and run:

```text
primary conflict
    + compact local neighbors
    -> BoundedGapCandidateBuilder (<= 8)
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
```

This is **observe/prove only** in 12A-6b1. It records whether a continuous moving
passage is feasible but does not replace the accepted LocalAvoidance steering
command yet.

The moving-passage result now exposes the first linear-acceleration sample from
the exact Hermite trajectory that was continuously verified. That will be the
control sample used when authority is enabled; no second trajectory may be
re-derived after acceptance.

## Why authority is intentionally still disabled

A moving passage can be dynamically valid and still intersect stationary
HitVolume geometry.

Before a feasible moving passage may steer the ship, the same accepted moving
trajectory must receive exact-static proof. Until that composition exists:

```text
moving precision says feasible
    -> diagnostic only

existing LocalAvoidance / exact-static result
    -> remains steering authority
```

This prevents Stage 12A-6b from regressing the already accepted 12A-4/12A-5
static collision contract.

## Candidate fixtures

The runtime-planner test now pins both sides:

```text
open translating gap
    -> candidate pair found
    -> moving prediction open
    -> continuous ship passage evaluated
    -> feasible

closing gap
    -> candidate pair found
    -> prediction detects closure inside horizon
    -> moving passage is never accepted
```

## Latest target-machine result

On `6badb5f74ea65900a38a48854d66f16f34127a89` the new runtime suite passed:

```text
navigation_runtime_control    PASS
navigation_runtime_planner    PASS
navigation_replication_truth  PASS
3/3 PASS
```

The architecture checker failed only because it expected the literal CMake
substring `PUBLIC EliteNavigationGeometry`; the new trajectory dependency had
changed whitespace/layout. Production/test linking itself succeeded.

Fixes now on `main`:
- restore the existing CMake contract-friendly layout;
- normalize whitespace in the architecture checker before matching the link
  relationship, so formatting changes do not create false failures.

### Corrected architecture rerun

Target machine now reports:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
```

The already green runtime 3/3 result is retained. Run only the remaining gate:

```bash
bash tests/navigation_trajectory/run_mingw64.sh
bash tests/navigation_map/run_mingw64.sh
bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

## Target-machine gate

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_trajectory/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_map/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

Expected new runtime-planner output includes:

```text
NAVIGATION RUNTIME PLANNER TESTS: PASS
 - bounded runtime candidates -> moving-gap/passage precision probe
 - closing moving gap fails closed before passage evaluation
```

The existing Stage-12 live self-test must remain green unchanged because 12A-6b1
has no live steering authority yet.

## Next step after a green gate

Compose the **same** verified moving Hermite trajectory with exact-static
NavigationSpace/HitVolume proof. Only after that gate is green may a feasible
moving passage become an authoritative `NavigationRuntimePlanner` maneuver and
flow through map->world control, PilotSkillExecutor, physics and replication.

## Documentation invariant

After every state-affecting result or scope change, update together:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`.

Use a verified baseline hash for accepted evidence; do not store a self-invalidating
"current HEAD" field.


## 12A-6b1 gate result — ACCEPTED

Target-machine evidence on `a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d`:

```text
architecture                    PASS
navigation_runtime              3/3 PASS
navigation_trajectory           11/11 PASS
navigation_map                  1/1 PASS
client/server build             PASS
server --self-test-navigation   PASS
exact_static_violation          0
replication errors              0
```

## Current implementation task: 12A-6b2

Add a bounded exact-static verifier for the exact moving Hermite trajectory
already accepted by `MovingPassageTrajectoryEvaluator`.

The verifier must conservatively cover the curve between discrete trajectory
samples and use NavigationSpace exact obstacle geometry. A dynamically feasible
moving passage is only marked statically safe when every bounded curve interval
passes that proof.

Do **not** grant steering authority in this slice. First prove and expose:

```text
moving passage feasible
    + exact-static same-trajectory safe
    -> eligible-for-authority diagnostic
```

After a green target-machine gate, the next slice may route the already verified
first acceleration sample into authoritative planner intent.


## 12A-6b2 candidate implementation

Candidate baseline before documentation commits:

```text
61f62e9d096542ae52680cf47d6da1c03b0bb8c0
```

Implemented proof chain:

```text
accepted MovingPassage Hermite curve
    -> 33 exact center witnesses
    -> 32 A*dt^2/8 continuous deviation bounds
    -> hull containment radius + curve deviation
    -> NavigationSpace::querySegment for each interval
    -> exact-static safe / exact blocker identity
```

This is deliberately conservative for the ship envelope but still uses the
authored HitVolume shape as static obstacle truth. It cannot miss a collision
between samples.

### Target-machine gate to run now

```bash
cd /d/__elite/work

git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_trajectory/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_map/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

Expected runtime-planner output now also includes:

```text
 - exact-static blocker rejects the same accepted moving Hermite trajectory
```

Expected live self-test behavior remains unchanged because 12A-6b2 still has no
steering authority.


## 12A-6b2 first gate result — compile failure corrected

First run on `8c30ebc1c7a724c06113b5b1e4704a7a172b6d93`:
- architecture contract: PASS;
- navigation_map: 1/1 PASS;
- navigation_trajectory: COMPILE FAIL;
- navigation_runtime: COMPILE FAIL;
- full build: COMPILE FAIL;
- final server self-test output was from a stale pre-existing binary and is not
  evidence for this candidate.

Root cause and correction:
- missing `TrajectoryWitness trajectory {}` member in evaluator `Result`;
- explicit `NavigationSpace::Vec3d` construction required by the MinGW build;
- architecture contract tightened so the missing result member cannot pass
  structural validation again.

Corrective code/contract baseline:

```text
52a5fa9e239d8ea00923cbb65a293cca5863567b
```

### Rerun now

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_trajectory/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

`navigation_map` need not be rerun for this correction: it already passed and
none of the corrective files touch NavigationMap. If the compile/runtime gate is
green, 12A-6b2 can be accepted.


## 12A-6b2 gate result — ACCEPTED

Corrected target-machine rerun on `25dc4369b95b4872d9a70a2d236db5e482cf45e2` passed architecture,
trajectory 11/11, runtime 3/3, canonical client/server builds and the rebuilt
headless navigation self-test. The self-test retained zero exact-static
violations and zero sparse/canonical replication error.

## Current implementation task: 12A-6b3a

Promote the already verified first moving-passage acceleration sample to planner
authority only when both proofs are green:

```text
movingPassageFeasible && movingPassageStaticSafe
```

Required contracts:
- use the exact `movingPassageInitialAccelerationMapMps2` sample already
  emitted by the accepted Hermite evaluator;
- do not reconstruct a second path or desired velocity;
- expose a distinct authoritative planner status/diagnostic;
- preserve map->world conversion and PilotSkillExecutor ownership;
- statically blocked or closing passages must never receive moving-passage
  authority;
- default behavior remains unchanged when moving precision is disabled.

This first authority slice is deterministic planner/control evidence. It does
not yet claim live moving-gap execution through authoritative physics/replication.


## 12A-6b3a candidate implementation

Candidate baseline before documentation commits:

```text
5e96b59ab99a22d3d1fa94cf16577b5ff71c84bb
```

Implemented authority rule:

```text
allowSteeringAuthority
    && !StaleHold
    && movingPassageFeasible
    && movingPassageStaticSafe
    -> Status::MovingPassageClear
    -> intent.linear = movingPassageInitialAccelerationMapMps2
```

No desired-velocity or second trajectory solve occurs after proof.
Existing planner status ordinals 0..5 are preserved; `MovingPassageClear` is
appended as value 6.

### Target-machine gate to run now

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

The live self-test is expected to remain behaviorally unchanged in this slice
because production moving-passage authority has not yet been enabled in the
live lab. The key new deterministic runtime output is:

```text
 - doubly-proven moving passage -> exact sample -> map/world -> PilotSkillExecutor authority
```

If this gate is green, accept 12A-6b3a and activate the live moving-gap
authority/physics/replication fixture.


## 12A-6b3a gate result — ACCEPTED

Target-machine run on `daaf038021cdf8b9561db60fdd35e7cefce0b2df` passed:
- Stage-12 architecture contract;
- navigation_runtime 3/3;
- navigation_trajectory 11/11;
- canonical client/server build;
- rebuilt server navigation self-test;
- zero exact-static violations;
- zero sparse/canonical replication error.

## Current implementation task: 12A-6b3b

Build one deterministic **live moving-gap authority fixture** using the accepted
planner path and explicitly enable `allowSteeringAuthority` for that fixture.

Required live evidence:
- moving precision sees the intended dynamic boundary pair;
- moving passage becomes dynamically feasible and exact-static safe;
- planner publishes `Status::MovingPassageClear`;
- the exact proved acceleration sample is transformed to world control;
- PilotSkillExecutor emits the executed demand;
- authoritative ship physics responds;
- same-tick sparse and canonical replication match the executed demand;
- exact-static physical sweep remains violation-free.

Do not add a second presentation/debug planner. Manual guidance remains a
consumer of the same accepted navigation product.


## Deferred task recorded — galactic route planning

Future strategic work is specified in:

`src/game/navigation/GALACTIC_ROUTE_PLANNING.md`

Key requirements are frozen there:
- hierarchical Galaxy -> System -> Local -> Precision planning;
- gravity-aware long physical legs;
- fuel / delta-v / reserve coupling;
- drive-specific jump range, charge and cooldown constraints;
- one accepted route product for both ManualGuidance and Autopilot;
- replanning after fuel/capability/hazard/arrival-state changes.

This is **not** the current implementation target.

### Current next step remains 12A-6b3b

Create the deterministic live moving-gap fixture and prove that accepted
`MovingPassageClear` authority reaches real physics and same-tick replication
without exact-static violation.


## 12A-6b3b candidate implementation

Candidate code/contract baseline before documentation commits:

```text
d5e1f990aed38ded7a517d719e935b6b6e763d4d
```

The live lab now contains a real two-boundary translating aperture. The planner
receives real Cobra hull dimensions and real forward/RCS/angular capability,
then explicitly enables the already accepted moving-passage authority seam.

The server self-test is now two-phase:
1. capture `MovingPassageClear` while its command is actively executed and
   produces physical acceleration;
2. require sparse/canonical same-tick replication from that active authority
   epoch, then continue the authoritative run until the ship physically crosses
   the moving-gap plane.

### Target-machine gate to run now

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

Expected final self-test diagnostics now include:

```text
moving_gap_pair=1
moving_gap_kinematics=1
moving_precision=1
moving_passage_feasible=1
moving_passage_static_safe=1
moving_passage_authority=1
moving_passage_executed=1
moving_passage_applied=1
moving_gap_passed=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

Any missing flag is a failed 12A-6b3b gate and must be diagnosed before manual
guidance work begins.


## 12A-6b3b first live gate result — CORRECT FIXTURE

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

Current task: relocate only the moving-gap fixture so the live test exercises moving-passage authority before the independent static obstacle.


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

### Rerun

```bash
git pull --ff-only
git rev-parse HEAD
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh
bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

If the live evaluator still rejects, the final NAV-SELFTEST line now reports
`moving_eval_status`, `moving_req_*_mps2`, and moving clearance values.


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

Current task: author a static exact-HitVolume slit/tunnel proving fixture and make successful hull passage part of the live gate.


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

### Target-machine gate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

Expected new live diagnostics include:
`slit_exact_open=1 slit_portal=1 slit_passed=1 slit_margin_m=>=0`.



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

Current implementation task:
1. correct exact slit fixture proof with selected-portal boundary semantics;
2. add generic oriented finite-depth portal traversal metadata;
3. make runtime planner stage at the entry/capture point, align velocity and
   hull forward axis with the portal normal, then transit the tunnel;
4. require live evidence of aligned entry and collision-free exit.



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

### Current target-machine gate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

Acceptance requires the final live run to show, in addition to the previous
moving-passage/replication evidence:

```text
slit_exact_open=1
slit_portal=1
slit_entry_capture=1
slit_entry_vel_aligned=1
slit_entry_fwd_aligned=1
slit_entry_crossed_aligned=1
slit_entry_vel_angle_rad <= 0.0872665
slit_entry_fwd_angle_rad <= 0.0872665
slit_entry_lateral_mps <= 1
slit_passed=1
slit_margin_m >= 0
exact_static_violation=0
```



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

Implementation target:
1. extend LocalAvoidance from the fixed 15/30-degree fan to a bounded increasing
   deflection search, still selecting the smallest safe deviation;
2. preserve direct A->B as the first query every update, so recovery to the
   direct line is automatic and stateless;
3. keep the horizon physical: braking distance + turn allowance + safety margin,
   never route-wide precision;
4. disable free-space MovingPassage steering authority in the live lab fixture;
5. retain MovingPassage unit/precision contracts for explicit mandatory gaps;
6. require the same authoritative run to bypass the moving pair, recover toward
   the route, capture the tunnel entrance, align velocity + hull axis, traverse
   the exact-HitVolume tunnel, and keep `exact_static_violation=0`.



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

### Target-machine gate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

Expected live evidence now centers on:

```text
moving_gap_pair=1
moving_gap_kinematics=1
visibility_bypass=1
visibility_max_deflection_rad > 0
moving_gap_passed=1
visibility_direct_recovered=1
slit_exact_open=1
slit_portal=1
slit_entry_capture=1
slit_entry_vel_aligned=1
slit_entry_fwd_aligned=1
slit_entry_crossed_aligned=1
slit_passed=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```



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

### Current target-machine gate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
```

Next implementation slices after this gate:
1. build bounded recovery candidates when ordinaryVisibilitySearchExhausted;
2. keep candidate generation control-law-specific;
3. route those candidates through ManeuverDecisionController;
4. connect EmergencyPassageMitigator / EmergencyContactSeverityScorer for
   contact-expected recovery;
5. continue the separate live portal-capture correction.


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

### Active next slice

The accepted-segment/follower code slice now exists. The immediate task is the
target-machine gate for candidate
`1e1b5ff2e72d743688833a403e94aefc0a832c87`.

~~~bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

bash build_mingw64.sh
./build/headless_server/EliteServer.exe --self-test-navigation
~~~

Acceptance for this pass:
1. compile/tests/self-test are green;
2. stable live automatic execution shows `planCount << executionCount`;
3. `acceptedSegmentFollowCount` grows between planner epochs;
4. no new exact-static violation is introduced.

If green, the next pass goes back into the live game/slit fixture immediately:
first re-run tunnel capture with the new execution cadence, then wire the
dedicated dynamic-hazard invalidation monitor and manual GuidanceCorridor
publication without restoring per-frame planning.


## AcceptedShortSegment + TrajectoryFollower integration candidate

Code candidate:

~~~text
1e1b5ff2e72d743688833a403e94aefc0a832c87
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

Do not promote `1e1b5ff2e72d743688833a403e94aefc0a832c87` to accepted
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
