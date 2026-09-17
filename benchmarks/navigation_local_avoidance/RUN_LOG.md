# Navigation local avoidance benchmark — run log

## 2026-09-17 — prepared, target-machine measurement pending

Behavior precondition is accepted on target-machine commit:

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

The dedicated avoidance-cost harness was added only after that behavior gate passed.

Pending measurement scenarios:

```text
nominal_clear_64
early_adjust_64
all_static_rejected_64
all_dynamic_rejected_16
all_dynamic_rejected_64
all_dynamic_rejected_256
all_dynamic_rejected_1024
```

No performance acceptance is recorded until fresh target-machine output is captured.
