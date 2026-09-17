# LocalHorizonPlanner candidate-scaling benchmark

**Stage:** `NAV-V2-LOCAL-1`  
**Purpose:** measure the isolated local conflict / receding-horizon reference through the public `LocalHorizonPlanner` boundary.

This benchmark intentionally does **not** measure full-world `NavigationMap` broadphase again. `NAV-V2-MAP-2` already established the dynamic-map reduction boundary. Here the input is the compact `NavigationMap::QueryResult` that the local planner is actually contracted to consume.

Pinned candidate-count scales:

```text
0 / 16 / 64 / 256 / 1024 compact candidates
```

`1024` is a stress scale for the downstream local layer, not a claim that a normal NavigationMap query should routinely return that many candidates.

Measured scenarios:

- `clear_*`: all compact candidates are dynamically clear of the bounded intended segment;
- `conflict_*`: one deterministic conflicting actor is embedded in the same candidate count while the planner still evaluates the complete compact set;
- `stale_1024`: completed dynamic result exceeds `maxResultAgeSeconds`; the planner must fail closed before candidate evaluation, demonstrating the stale path is independent of candidate count.

Measured output:

- median and p95 microseconds per `LocalHorizonPlanner::evaluate()` call;
- p95 nanoseconds per supplied candidate for non-empty scenarios;
- supplied candidate count;
- actual `candidatesExamined` and `conflictsFound`;
- final status (`Clear`, `ConflictHold`, `StaleHold`).

The harness batches repeated calls per timing sample so sub-millisecond measurements are not dominated by clock-call overhead. Scenario construction and vector allocation happen outside the timed region.

The benchmark links only through `EliteNavigationLocal`; it does not inspect planner internals or legacy collision/navigation code.

Build and run from MSYS2 MinGW64:

```bash
bash benchmarks/navigation_local/run_mingw64.sh
```

Optional:

```bash
bash benchmarks/navigation_local/run_mingw64.sh \
  --warmup 5 \
  --iterations 30 \
  --output navigation_local_benchmark.csv
```

The CSV path is printed as an absolute path.

## Decision use

This first run is a measurement gate, not a predeclared micro-optimization target. The existing Navigation v2 frame budget remains the context: typical main-thread work should stay below roughly `0.5 ms` and normal peaks below roughly `1.0 ms`, but the candidate benchmark exists to measure the actual downstream slope before choosing an adjusted-target / lateral-avoidance algorithm.

Do not optimize `LocalHorizonPlanner` or invent a more complex avoidance method until target-machine results show where the cost actually sits.
