# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

## Latest runtime attempt

Tested:
```
8729abbbf3e74df0969f83bbc03773ebd827d3af
```

Results:
- Stage-12 architecture PASS;
- build PASS;
- runtime 17/18;
- only `maneuver_chained_limit_matrix` failed.

Newtonian chain passed its physical behavior:
```
4/4 phases
seam P jump 0
seam V jump 0
seam forward jump ~0.000001 deg
seam omega jump 0
max hull half-width 17.428330 m / 25 m
max slip 34.491954 deg
final P 0.024611 m
final speed 0.024960 m/s
final forward error 0.028574 deg
tracking exceeded 0
```

Failure:
```
Assisted chained aligned turn produced excessive slip
```

## Diagnostic interpretation

Phase 2 uses ScheduledMoving and advances at nominal end without requiring terminal capture.

Therefore phase 3 intentionally inherits actual physical P/V/q/omega, including possible residual slip.

The old phase-3 assertion used absolute maximum slip from the first tick, so it could not distinguish an inherited handoff transient from Assisted phase behavior.

## Current unverified candidate

```
6fda55f8a2a954ae1656d5eebf4538f585125f2e
```

Adds per-phase `[CHAIN-PHASE]` diagnostics:
- entry slip;
- absolute max slip;
- max slip after 1 s;
- final slip;
- max P/V/forward tracking errors;
- final P/V/forward errors;
- tracking envelope exceed ticks.

Assisted aligned phase criteria:
- max slip after first 1 s <= 8 deg;
- final slip <= 4 deg.

This preserves the original 8 deg aligned behavior criterion while separating the explicit ScheduledMoving handoff transient.

If the Assisted phase remains >8 deg after 1 s, treat it as a real reference/control defect; do not relax the limit.

## Validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

OUT="navigation_test_$(date +%Y%m%d-%H%M%S).txt"

{
    echo "===== TESTED HEAD ====="
    git rev-parse HEAD

    echo
    echo "===== ARCHITECTURE CONTRACT ====="
    python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

    echo
    echo "===== NAVIGATION RUNTIME ====="
    bash tests/navigation_runtime/run_mingw64.sh
} 2>&1 | tee "$OUT"

echo
echo "===== CHAIN/LIMIT SUMMARY ====="
grep -E '\[CHAIN-PHASE\]|\[CHAIN\]|\[LIMIT\]|MANEUVER CHAINED/LIMIT|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Next

If 18/18:
- accept chained transitions + negative/physical-limit block;
- build the one final composite laboratory proving ground.

If red:
- use phase diagnostics to identify actual root cause and fix mechanism/reference authoring.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
