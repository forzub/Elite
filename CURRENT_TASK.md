# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — behavior accepted; compact-candidate performance measurement pending

## Closed behavior gate

Fresh target-machine result on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

The initial local ownership/safety boundary is therefore **behavior-accepted**.

Accepted reference semantics:

```text
NavigationSpace / upstream route intent
        +
compact NavigationMap::QueryResult
        +
agent P/V/A + result age
        |
        v
bounded LocalHorizonPlanner
        |
        +-> Clear / PassThrough
        +-> Clear / Terminal
        +-> ConflictHold
        +-> StaleHold
```

No second NavigationWorld, full actor-table scan, renderer/game dependency or unverified lateral bypass is allowed inside this block.

## Candidate now on `main`

Dedicated downstream scaling harness:

```text
benchmarks/navigation_local/
    CMakeLists.txt
    main.cpp
    README.md
    RUN_LOG.md
    run_mingw64.sh

tests/architecture_contracts/check_navigation_local_benchmark.py
```

It measures only `LocalHorizonPlanner::evaluate()` over already reduced compact candidates. It deliberately does not rerun full-world NavigationMap broadphase.

Pinned candidate counts:

```text
clear:     0 / 16 / 64 / 256 / 1024
conflict:     16 / 64 / 256 / 1024
stale:                           1024
```

`1024` is a stress scale for the local consumer, not an expected normal candidate count.

Measured per scenario:

```text
median_us
p95_us
p95_ns_per_candidate
candidates_examined
conflicts_found
status
```

The harness batches calls so microsecond-scale measurements are not dominated by clock overhead. Scenario/vector construction occurs outside the timed region.

Expected semantic checks inside the benchmark:

```text
clear_*       -> Clear, conflictsFound == 0
conflict_*    -> ConflictHold, conflictsFound >= 1
stale_1024    -> StaleHold, candidatesExamined == 0
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_local_boundary.py
bash tests/navigation_local/run_mingw64.sh

python tests/architecture_contracts/check_navigation_local_benchmark.py
bash benchmarks/navigation_local/run_mingw64.sh
```

Send the complete output.

## Decision after benchmark

This first local benchmark is a measurement gate, not a fabricated optimization target.

Interpretation order:

1. verify clear/conflict cost slope against supplied compact-candidate count;
2. verify `stale_1024` remains O(1) and examines zero candidates;
3. compare measured p95 with the Navigation v2 frame-budget context (`<0.5 ms` typical main-thread work, `<1.0 ms` normal peak);
4. only then choose the first adjusted-target / lateral-avoidance algorithm and its candidate budget.

Do not optimize the reference loop, wire live `EliteGame` / `EliteServer`, or add pursuit-specific intercept logic before this measurement is recorded.
