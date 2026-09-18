# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Stage:** 12A-3 — live CUBE 08 behavior proof

## Accepted baseline

12A-2 is accepted on:

```text
7b4db95788d80c95afb3b57c109c671cb7a41366
```

with architecture PASS, `navigation_runtime 3/3`, EliteGame PASS and
EliteServer PASS.

## Candidate under test

Current code baseline:

```text
99833c243992f5af2ce7bacc75efb3eb65ef3a84
```

New live mode:

```text
EliteServer --self-test-navigation
```

The self-test requires all of the following before PASS:

- CUBE 08 enters the bounded NavigationMap candidate set;
- CUBE 08 is retained as the nominal conflict that rejected the straight route;
- LocalAvoidance produces an adjusted safe target;
- PilotSkillExecutor executes non-zero lateral acceleration;
- authoritative motion departs from the original straight line;
- the ship passes the CUBE 08 center plane;
- minimum conservative sphere clearance remains positive;
- the ship continues at least 3500 m toward the final goal;
- a sparse packet that actually publishes the lab row matches the authoritative
  published execution at the exact same `serverTick`;
- retained canonical hydration matches that same execution.

The test is bounded to 120 s of simulated time and stops early when all evidence
is complete.

## RUN NOW

Because `LocalAvoidancePlanner` changed to preserve nominal conflict identity,
rerun its isolated suite as well:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

If another EliteServer process is running, stop it first: the executable uses
the normal single-instance guard.

## Expected

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS

navigation_local:
    2/2 PASS

navigation_runtime:
    3/3 PASS

EliteGame build PASS
EliteServer build PASS

[NAV-SELFTEST] ... obstacle_candidate=1 obstacle_conflict=1 adjusted=1 ...
               lateral_exec=1 ... min_conservative_clearance_m=>0 ...
               progress_m=>3500 ... replication_error_mps2=0 canonical_replication_error_mps2=0

[PASS] navigation-runtime CUBE 08 caused authoritative avoidance
       with positive conservative clearance and replicated execution
```

Do not tune thresholds merely to obtain PASS. If the live self-test fails, use
the printed metrics to identify whether the defect is candidate publication,
avoidance geometry, pilot execution, physical authority, or replication.


### Correction after first live run

The first live run reached the replication check and reported:

```text
error_mps2=0.00497292
```

This was a test-epoch bug, not grounds for changing numerical tolerance.
Per-fixed-step diagnostic state was being compared with a cadence-limited
retained client snapshot. The corrected gate waits for a sparse packet
containing the lab row and compares it against a copied authoritative
`GameServer::snapshot()` with the exact same `serverTick`.

Run the same gate again after pulling current `main`.
