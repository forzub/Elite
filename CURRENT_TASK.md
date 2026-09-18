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
