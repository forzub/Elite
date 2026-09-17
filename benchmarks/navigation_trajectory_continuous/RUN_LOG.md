# Navigation trajectory continuous benchmark — run log

## 2026-09-17 — target-machine performance ACCEPTED

Behavior precondition was already accepted on:

```text
forzub/Elite@574a2e98fd7a75ebf562bbfa476fba367fb888d1
```

Performance run used:

```text
forzub/Elite@6f85436252d36e4b586ab496efe3aea45fb25e79
MSYS2 MinGW64 / g++ 15.2.0 / Release / Ninja
```

Architecture contract:

```text
NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK CONTRACT: PASS
```

Measured results:

```text
scenario                  median_batch_us  p95_batch_us  median_per_query_us  p95_per_query_us
straight_newton                    3.9410        4.0458               3.9410            4.0458
rolled_newton                      6.3381        6.5235               6.3381            6.5235
lateral_newton                     3.9747        4.1593               3.9747            4.1593
elite_aligned                      4.0228        4.1820               4.0228            4.1820
geometry_blocked_roll              6.3501        6.5133               6.3501            6.5133
full_precision_batch8             36.7902       41.3527               4.5988            5.1691
```

Primary decision signal:

```text
full_precision_batch8 p95 = 41.3527 us = 0.04135 ms
```

Acceptance contract was:

```text
<0.5 ms  -> freeze static continuous verifier
<1.0 ms  -> acceptable normal peak; inspect scheduling before math changes
>=1.0 ms -> optimize/budget before live integration
```

Result: **performance accepted with large margin**. The complete eight-candidate precision batch consumes about 8.3% of the `0.5 ms typical` CPU budget and about 4.1% of the `1.0 ms normal peak` budget.

Decision: freeze the accepted static `ContinuousPassageTrajectoryEvaluator` math. Do not micro-optimize it without contrary live-runtime evidence.

Next trajectory work is emergency-contact severity ranking by predicted relative normal contact speed / impact-energy proxy, followed by time-varying gaps and moving/rotating docking.
