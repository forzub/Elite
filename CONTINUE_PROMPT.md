# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

## Latest target result

Tested:
```
350f7d593e22b8b89cb3ae4dbfbfbb7fbb53ea03
```

Results:
- architecture PASS;
- build PASS;
- runtime 17/18;
- only chained-limit Assisted phase 3 failed.

Newtonian chain:
- 4/4;
- no state resets at seams;
- max slip 34.49 deg;
- hull 17.43 m inside 25 m;
- final P 0.0246 m;
- final speed 0.0250 m/s;
- final attitude 0.0286 deg;
- zero tracking violations.

Assisted phase 3:
- entry slip 1.579 deg;
- max slip after 1 s 25.789 deg;
- final slip 25.789 deg;
- final forward error 25.965 deg;
- tracking exceeded 171 ticks.

This proves a phase-3 reference/control defect, not inherited transient.

## Root cause

The test reference builder reconstructed full body basis from velocity tangent plus a fixed world-up seed.

At `abs(dot(forward,Y)) > 0.92` it switched the seed from Y to X.

The forward tangent remained continuous, but right/up jumped in roll.

B10 tracks full SO(3) attitude:
- forward;
- right;
- up.

Therefore the basis discontinuity became a real angular command.

## Current unverified fix

```
b8eb4641b013692c773d087d6cad96756c672b3c
```

Aligned moving reference now uses a parallel-transport/Bishop frame:
- carry previous right;
- project it into the new tangent-normal plane;
- rebuild up continuously;
- preserve roll continuity.

At terminal sample:
- requested terminal forward is pinned;
- transported roll is preserved.

Exact terminal roll/up orientation for docking/attachment/placement must be authored explicitly as a separate attitude-capture profile.

No tracking gains or acceptance tolerances changed.

## Acceptance criteria

Assisted aligned phase:
- max slip after 1 s <=8 deg;
- final slip <=4 deg.

Whole chain:
- 4/4 phases;
- zero tracking-envelope violations;
- no P/V/attitude/omega seam reset;
- full hull <=25 m;
- strict final capture.

Negative/limit contracts must then all execute:
- insufficient turn horizon rejection;
- insufficient braking room rejection;
- rigid-hull corridor rejection;
- law-incompatible candidate rejection;
- dynamic-hazard invalidation.

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
- accept chained transitions + negative/limit block;
- build one final composite laboratory proving ground.

If still red:
- inspect angular feed-forward derivation from transported quaternion samples;
- do not loosen criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
