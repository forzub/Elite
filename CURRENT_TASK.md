# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested checkout

```
f626fb0373499928e0ae89585c3bd992e5436c92
```

Architecture PASS, runtime 18/19.

Newtonian final composite fully completes.

Assisted fails only after repeated persistent-hazard replanning.

## Diagnosis

Production adjusted targets for Assisted change avoidance side:

```
initial  Z=+17.815749
iter 0   Z=+15.519026
iter 1   Z=-14.477971
iter 2   Z=+13.773784
```

The final failure is:
```
composite could not author bounded continuation around persistent hazard
```

The no-stop author cannot physically reverse side again from the current live state.

Root cause is production local-avoidance azimuth selection:
- first safe azimuth wins;
- local transverse basis is regenerated after every segment;
- equivalent safe side ordering changes with vehicle pose;
- repeated replans can ping-pong.

## Current production candidate

```
86640b05145938ec0880a3a26539957aaa72f085
ef2e6ec85823c229d6cadb6aa8dbe5b65e209193
6064a22f565fd7cc82568b0babf2721891c8d925
```

New rule:
- smallest safe deflection ring still wins;
- inside that ring, choose the safe candidate most aligned with current velocity;
- azimuth index is deterministic tie-break.

New focused regression:
- symmetric +/-Z safe bypass;
- vehicle already moving slightly toward -Z;
- result must preserve -Z side.

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
echo "===== FINAL COMPOSITE SUMMARY ====="
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Interpretation

If the focused planner regression fails:
- fix the continuity selection itself.

If planner regression passes but composite still ping-pongs:
- inspect whether current velocity is an insufficient continuity signal and promote an explicit accepted-corridor side/steering hint into the planner contract.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior lab;
- move primary evaluation to NAV STRESS/game.

## Iteration rule

After every state/evidence change, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
