# LocalHorizonPlanner benchmark run log

## 2026-09-17 — candidate-count scaling harness prepared

Behavior/ownership baseline immediately before this harness:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

Target-machine behavior gate therefore accepts the initial `NAV-V2-LOCAL-1` boundary semantics:

- compact `NavigationMap::QueryResult` input only;
- bounded physical receding horizon;
- deterministic closest-approach + conservative swept conflict reference;
- fail-closed `ConflictHold` and `StaleHold`;
- no unverified lateral bypass yet.

The next measurement harness is now `benchmarks/navigation_local/`.

Pinned scales:

```text
clear:     0 / 16 / 64 / 256 / 1024 candidates
conflict:     16 / 64 / 256 / 1024 candidates
stale:                           1024 candidates
```

Status: **pending target-machine benchmark run**. No performance claim is made until the MinGW64 output is captured here.
