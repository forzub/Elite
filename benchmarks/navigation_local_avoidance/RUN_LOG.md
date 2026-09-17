# Navigation local avoidance benchmark — run log

## 2026-09-17 — target-machine performance ACCEPTED

Target-machine run used public commit:

```text
e0817d157ba5d8c9c329576236310507bda13364
```

Benchmark architecture contract passed and the Release/MinGW64 harness built successfully.

Measured results:

```text
scenario                     median_us    p95_us    probes  horizon_evals  candidate_visits
nominal_clear_64                1.7676     1.8333      0          1               64
early_adjust_64                 3.7656     4.2795      1          2              128
all_static_rejected_64          3.9628     4.2655     16          1               64
all_dynamic_rejected_16        10.7188    12.9500     16         17              272
all_dynamic_rejected_64        32.7183    35.8900     16         17             1088
all_dynamic_rejected_256      120.3708   128.9708     16         17             4352
all_dynamic_rejected_1024     474.1000   519.9286     16         17            17408
```

Status/output contract remained correct in every scenario:

```text
nominal_clear_64           -> NominalClear
early_adjust_64            -> AdjustedClear
all_static_rejected_64     -> ConflictHold
all_dynamic_rejected_*     -> ConflictHold
```

Design budgets:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Interpretation:

- normal clear / early-adjust / static-reject paths remain only about `0.002-0.004 ms p95`;
- 64-candidate repeated dynamic rejection remains about `0.036 ms p95`;
- 256-candidate repeated dynamic rejection remains about `0.129 ms p95`;
- the deliberately pathological 1024-candidate x 17-pass stress case is about `0.520 ms p95`, slightly above the `0.5 ms typical` guideline but still inside the `1.0 ms normal peak` budget;
- `1024` compact local conflicts are not an expected normal per-agent input.

Decision: **keep the deterministic 16-probe fan unchanged.** The multiplied local-avoidance path is performance-accepted. Do not optimize the already-accepted one-pass horizon loop or fan ordering without new runtime evidence.

`NAV-V2-LOCAL-1` performance gate is closed by this measurement.

---

## 2026-09-17 — behavior precondition

Behavior was accepted earlier on target-machine commit:

```text
bdec064d152050b4bc199b2657f14b3f577dcba3
```

Observed gate:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```
