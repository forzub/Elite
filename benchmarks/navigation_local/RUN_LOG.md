# LocalHorizonPlanner benchmark run log

## 2026-09-17 — target-machine candidate scaling ACCEPTED

Target machine / toolchain:

```text
Windows 10
MSYS2 MinGW64
g++ 15.2.0
HEAD 4b94048b15e6e2cd32754b6b8d48daedcb18625f
```

Pre-benchmark gates:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
NAVIGATION LOCAL BENCHMARK CONTRACT: PASS
```

Measured `LocalHorizonPlanner::evaluate()` cost over already reduced compact candidates:

```text
scenario       candidates   median_us   p95_us    p95_ns/candidate   examined   conflicts   status
clear_0                 0      0.0387    0.0394          0.0000             0           0   Clear
clear_16               16      0.4614    0.4814         30.0873            16           0   Clear
clear_64               64      1.7401    1.8327         28.6362            64           0   Clear
clear_256             256      6.7801    7.9820         31.1798           256           0   Clear
clear_1024           1024     28.5359   36.9641         36.0977          1024           0   Clear

conflict_16            16      0.4740    0.5073         31.7062            16           1   ConflictHold
conflict_64            64      1.7743    1.9333         30.2078            64           1   ConflictHold
conflict_256          256      6.9344    7.5441         29.4693           256           1   ConflictHold
conflict_1024        1024     27.6719   31.8656         31.1188          1024           1   ConflictHold

stale_1024           1024      0.0359    0.0359          0.0351             0           0   StaleHold
```

Raw CSV was emitted as `navigation_local_benchmark.csv` on the target machine.

### Interpretation

The reference loop scales linearly at roughly `29-36 ns` per examined candidate on the measured machine. Even the intentionally excessive `1024`-candidate stress case stays below `0.037 ms p95`, far below the Navigation v2 main-thread context budget (`<0.5 ms` typical, `<1.0 ms` normal peak).

`stale_1024` is effectively O(1) and exits before candidate work, exactly as required.

**Decision:** candidate-count performance is ACCEPTED. Do not optimize this loop further without new runtime evidence. The next work is behavior-first adjusted-target / lateral avoidance; its own multiplied probe cost must be measured separately after behavior acceptance.

## 2026-09-17 — candidate-count scaling harness prepared

Behavior/ownership baseline before the measurement:

- compact `NavigationMap::QueryResult` input only;
- bounded physical receding horizon;
- deterministic closest-approach + conservative swept conflict reference;
- fail-closed `ConflictHold` and `StaleHold`;
- no unverified lateral bypass.

Pinned scales were `0/16/64/256/1024` clear, `16/64/256/1024` conflict and `1024` stale.
