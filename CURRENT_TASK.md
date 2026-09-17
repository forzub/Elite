# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — local-horizon reference candidate pending target-machine gate

## Closed prior gate

`NAV-V2-SPACE-1` is CLOSED / ACCEPTED. Final 10k turn-aware p95 is `12.0072 ms` open / `11.9065 ms` hub against the pinned `<=40 ms` gate. Do not continue static turn-search optimization without new runtime evidence.

## Candidate now on `main`

New backend-neutral block:

```text
src/world/navigation/local/
    LocalHorizonPlanner.h
    LocalHorizonPlanner.cpp
    CMakeLists.txt
    README.md
```

Tests/contracts:

```text
tests/navigation_local/
    NavigationLocalContractTests.cpp
    CMakeLists.txt
    run_mingw64.sh

tests/architecture_contracts/check_navigation_local_boundary.py
```

The block consumes an upstream nominal target plus `NavigationMap::QueryResult` compact candidates. It owns no actor table, spatial index, GPU state, global route or second NavigationWorld snapshot.

## Reference semantics

Physical horizon:

```text
latencyDistance = |v|*resultAge + 0.5*|a|*resultAge^2
brakingDistance = |v|^2 / (2*maxBrakingAcceleration)

horizonDistance = max(
    minimumHorizon,
    latencyDistance + brakingDistance + turnDistance + safetyMargin
)
```

If the nominal target is farther away, the result is a bounded `PassThrough` target. If it lies inside the horizon, the result remains `Terminal` and preserves supplied terminal P/V/A.

Dynamic conflict reference uses only compact candidates:

- age candidate P/V using published acceleration;
- bounded relative-motion closest approach;
- accelerated separation at closest time;
- conservative swept-sphere intersection against the bounded intended segment;
- self-entity filtering.

Results:

```text
Clear
    -> PassThrough or Terminal safe progress target

ConflictHold
    -> no safe progress target demonstrated; fail closed

StaleHold
    -> completed dynamic result too old; fail closed before candidate work
```

No lateral bypass is invented yet. A stronger avoidance algorithm will be added only after this ownership/safety boundary is behavior-accepted and measured.

## Pinned fixtures

```text
clear_far_candidate
    -> bounded PassThrough

terminal_inside_horizon
    -> Terminal with terminal P/V/A preserved

crossing_actor
    -> ConflictHold

head_on_actor
    -> ConflictHold with bounded closest-approach time

stale_snapshot
    -> StaleHold before candidate loop

self_candidate
    -> ignored

invalid_candidate
    -> contract rejection
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
```

Send the complete output.

## Decision after behavior gate

If architecture + behavioral tests PASS:

1. accept the `NAV-V2-LOCAL-1` ownership/safety reference;
2. add a small candidate-count scaling benchmark (not a full-world benchmark — NavigationMap reduction is already measured);
3. measure clear/conflict/stale local evaluation cost on the target machine;
4. then choose the first adjusted-target avoidance algorithm from measured game constraints.

Do not wire live `EliteGame` / `EliteServer` and do not add pursuit-specific intercept logic before this boundary passes.
