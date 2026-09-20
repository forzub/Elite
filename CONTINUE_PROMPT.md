# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest target result

Tested:
```
f626fb0373499928e0ae89585c3bd992e5436c92
```

Architecture PASS. Runtime 18/19.

Newtonian final composite fully passes its complete scenario:
- 7 completed phases;
- 4 dynamic-bypass segments;
- one hazard invalidation;
- minimum dynamic clearance 3.175166 m;
- zero tracking-envelope violations;
- final P 0.022675 m;
- final speed 0.075007 m/s;
- final forward 3.521282 deg.

Assisted safely executes:
- initial authority-bounded replacement;
- continuation 0;
- continuation 1.

Then production adjusted targets oscillate in Z:
```
+17.815749
+15.519026
-14.477971
+13.773784
```

At the final reversal the test-side no-stop physical author cannot produce a valid continuation.

## Root cause

Production `LocalAvoidancePlanner` previously returned the first safe azimuth in the first safe deflection ring.

The transverse basis is regenerated at each replan, so azimuth index ordering does not preserve a physical left/right/up/down bypass side.

Repeated replans may therefore switch sides for no safety reason.

## Current unverified production fix

```
86640b05145938ec0880a3a26539957aaa72f085
ef2e6ec85823c229d6cadb6aa8dbe5b65e209193
```

New selection semantics:
- preserve existing smallest-deflection-ring priority;
- evaluate every safe azimuth in that ring;
- maximize dot(candidateDirection, currentVelocityDirection);
- deterministic azimuth index breaks ties;
- when current velocity is effectively zero, nominal route direction is fallback.

This supplies local side continuity without persistent hidden state.

## Focused regression

```
6064a22f565fd7cc82568b0babf2721891c8d925
```

A symmetric dynamic obstacle has safe +/-Z alternatives while agent velocity contains -Z.

The runtime planner must:
- return AdjustedClear;
- keep the selected target on -Z;
- strongly align selected direction with current velocity.

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
echo "===== FINAL COMPOSITE SUMMARY ====="
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Next interpretation

If the new focused regression fails, repair continuity scoring.

If it passes but composite still alternates sides, current velocity alone is insufficient and the next architectural move is an explicit accepted local-corridor/avoidance-side continuity hint.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior testing;
- move to real NAV STRESS/game visualization and behavior evaluation.

Do not weaken physical or clearance criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
