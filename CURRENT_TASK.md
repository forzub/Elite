# Elite — CURRENT TASK

**Updated:** 2026-09-18 Europe/Kyiv  
**Stage:** 12A-6b2 — exact-static proof of the accepted moving Hermite trajectory  
**Last target-machine verified baseline:** `a587dcd96bdf0b05edbf4fcfe9a32f5f7be1058d`  
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
