# Navigation trajectory bounded-gap benchmark — run log

## 2026-09-17 — target-machine performance ACCEPTED

Target-machine run used public commit:

```text
e0817d157ba5d8c9c329576236310507bda13364
```

Benchmark architecture contract passed and the Release/MinGW64 harness built successfully.

Measured results:

```text
scenario       neighbors  candidates  median_us   p95_us
reject_16          16         0          0.2263    0.2459
reject_64          64         0          0.7064    0.7550
reject_256        256         0          2.6036    4.6499
reject_1024      1024         0         10.1598   11.6730

top8_16            16         8          0.7502    0.7883
top8_64            64         8          1.7464    1.8260
top8_256          256         8          6.0664    6.3609
top8_1024        1024         8         22.3055   24.0699
```

The `1024` cases are deliberate stress ceilings, not expected normal precision-fallback inputs.

Existing local-navigation CPU budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Even continuous deterministic top-8 maintenance over 1024 already-reduced neighbors remains only about `0.0241 ms p95`. The bounded gap candidate builder is therefore **performance-accepted** and is not an optimization target without contrary runtime evidence.

The same target-machine trajectory run also built and passed all then-registered C++ tests:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time: 0.12 sec
```

One architecture checker failed only because it searched for an obsolete exact Markdown sentence (`small candidate budget (initial target <= 4-8)`). The implementation/test passed; the checker has since been repaired to assert stable markers (`hard candidate cap = 8`, one primary conflict, no all-pairs scan).
