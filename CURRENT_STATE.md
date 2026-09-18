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
