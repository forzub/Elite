# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

## Latest tested checkout

```
350f7d593e22b8b89cb3ae4dbfbfbb7fbb53ea03
```

Architecture PASS, build PASS, runtime 17/18.

Only failure:
```
maneuver_chained_limit_matrix
Assisted aligned phase retained excessive slip after handoff transient
```

Measured Assisted phase 3:
- entry slip 1.579 deg;
- max/final slip 25.789 deg;
- final forward error 25.965 deg;
- 171 tracking-envelope exceeded ticks.

This proves the problem is generated inside phase 3, not inherited from phase 2.

## Root cause

The test's velocity-aligned attitude builder rebuilt right/up from a world-up seed and switched seed axes at `abs(dot(forward,Y)) > 0.92`.

That creates a discontinuous roll frame while forward remains smooth.

B10 tracks all three axes, so the artificial roll discontinuity becomes a real angular command.

## Current fix candidate

```
b8eb4641b013692c773d087d6cad96756c672b3c
```

Velocity-aligned references now use a parallel-transport/Bishop frame:
- new forward = trajectory tangent;
- previous right projected into new normal plane;
- up reconstructed from transported right and new forward;
- no world-up threshold switch.

Terminal forward is pinned while transported roll is preserved.

Exact terminal roll, if required by docking/placement, must be authored explicitly as a separate attitude-capture profile.

## Target commands

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

## Acceptance

Assisted phase 3 remains strict:
- max slip after 1 s <=8 deg;
- final slip <=4 deg.

If 18/18:
- accept chained transitions + physical-limit block;
- move to one final composite laboratory proving ground.

If red:
- inspect continuous-frame quaternion/angular feed-forward next;
- do not weaken thresholds.

## Iteration rule

After every state/evidence change, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
