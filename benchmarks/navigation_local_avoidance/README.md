# Navigation local avoidance benchmark

This harness measures the **multiplied cost of `LocalAvoidancePlanner` target probing** after the base `LocalHorizonPlanner` compact-candidate loop has already been accepted separately.

It deliberately does not replace `benchmarks/navigation_local/`. The older benchmark measures one `LocalHorizonPlanner::evaluate()` pass over an already reduced compact `NavigationMap::QueryResult`; this benchmark measures what happens when local avoidance adds static point queries and, in the worst case, repeats the dynamic horizon evaluation for several lateral targets.

## Scenarios

The benchmark pins four behavior classes:

```text
nominal_clear_64
    nominal target is already clear
    0 lateral probes
    1 horizon evaluation
    0 static point queries

early_adjust_64
    nominal corridor is blocked by a conservative swept envelope
    first lateral target is statically + dynamically accepted
    1 lateral probe
    2 horizon evaluations total
    2 static point queries total

all_static_rejected_64
    nominal corridor is blocked
    all 16 lateral targets leave the envelope-safe interior of the same region
    16 probes rejected statically
    1 horizon evaluation total
    17 static point queries total

all_dynamic_rejected_{16,64,256,1024}
    current-kinematics head-on conflict
    all 16 lateral targets are statically valid
    all 16 dynamic rechecks remain ConflictHold
    17 horizon evaluations total
    17 static point queries total
```

`1024` candidates is an intentionally excessive stress case. It is present to expose the ceiling of the bounded 16-probe fan, not as an expected normal local-neighbor count.

## What is timed

Scenario construction, static-space publication and dynamic candidate construction are **outside the timed region**. The timed operation is only:

```cpp
LocalAvoidancePlanner::evaluate(query, dynamicCandidates, staticSpace)
```

Each timing sample repeats the call enough times to avoid timer-resolution noise; the repetition count is scaled down for expensive scenarios.

Reported fields:

```text
median_us
p95_us
probes
static_rejected
dynamic_rejected
horizon_evaluations
candidate_visits_estimate
static_point_queries
status
```

`candidate_visits_estimate` is deterministic accounting for this reference implementation:

```text
horizon_evaluations * compact_candidate_count
```

The accepted `LocalHorizonPlanner` scans all compact candidates in each evaluation in order to select the primary conflict deterministically; it does not early-exit on the first conflict.

## Run

From MSYS2 MinGW64:

```bash
cd /d/__elite/work
python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh
```

Optional measurement controls:

```bash
bash benchmarks/navigation_local_avoidance/run_mingw64.sh \
    --warmup 5 \
    --iterations 30 \
    --output navigation_local_avoidance_benchmark.csv
```

## Measurement gate

This is a target-machine measurement gate, not a portable timing assertion.

Interpret the result against the current Navigation v2 CPU budgets:

```text
main-thread navigation CPU   <0.5 ms typical
                             <1.0 ms normal peak
```

Decision rule:

- `nominal_clear` must remain essentially the cost of one accepted horizon pass;
- `early_adjust` should be comfortably below the normal local budget;
- `all_static_rejected` tells us whether the sixteen `NavigationSpace::queryPoint()` probes are themselves material;
- `all_dynamic_rejected` is the deliberate worst case and determines whether the current 16-probe ordering/budget can remain unchanged;
- do not optimize the fan before target-machine evidence shows a real need.

The benchmark does **not** claim trajectory feasibility for head-on avoidance. Current-kinematics head-on remains fail-closed by design until the later trajectory-aware vehicle/control layer is implemented.
