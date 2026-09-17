# Navigation trajectory continuous benchmark — run log

## 2026-09-17 — prepared; target-machine timing pending

Behavior precondition is accepted on target machine at:

```text
forzub/Elite@574a2e98fd7a75ebf562bbfa476fba367fb888d1
```

Accepted gate:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS
navigation_trajectory_passage               PASS
navigation_trajectory_gap                   PASS
navigation_trajectory_reachability          PASS
navigation_trajectory_emergency_passage     PASS
navigation_trajectory_continuous_passage    PASS
100% tests passed, 0 failed
Total Test time: 0.24 sec
```

The dedicated performance harness is now prepared at:

```text
benchmarks/navigation_trajectory_continuous/
```

Primary performance decision signal:

```text
full_precision_batch8 p95
```

Interpretation contract:

```text
<0.5 ms  -> freeze static continuous verifier
<1.0 ms  -> acceptable normal peak; inspect work scheduling before math changes
>=1.0 ms -> optimize/budget before live integration
```

No target-machine timing result is recorded yet.
