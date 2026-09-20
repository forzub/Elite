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
69f8ca4dbcb44df5340b94f45640bcb7d6e6ed1a
```

Architecture PASS. Runtime 17/19.

Failures:
1. focused continuity regression;
2. final composite.

The focused regression itself had a static-space fixture bug: generic region depth was only +/-10 m in Z, so the alleged symmetric +/-Z alternatives were not actually legal.

More importantly, the composite showed the velocity-only tie-break does not reliably preserve an accepted avoidance branch.

## Architecture conclusion

The direction of the currently accepted bounded local segment is execution state.

It must be supplied explicitly to the stateless planner on the next REPLAN.

Do not infer it only from instantaneous velocity.

## Current unverified production API

Commits:
```
71b80c4529e1bc776e2a2dbf209059a2ccff44f9
4bde26ee2fbb531116f960d089d488067f1bfd6d
76022a5199422dd80ef4caff611539b53c39e731
40d7b9852bc6f265ae02ecc0bf9d7b4e002d5b96
```

`NavigationRuntimePlanner::AgentState` now carries:
```
bool localAvoidanceContinuityValid
glm::dvec3 localAvoidanceContinuityDirectionMap
```

`LocalAvoidancePlanner::Query` carries:
```
bool preferredDirectionValid
Vec3d preferredDirectionMap
```

Inside the minimum safe deflection ring:
- explicit accepted direction is the continuity reference;
- current velocity is fallback only;
- nominal forward is fallback when both are unavailable.

The planner remains stateless.

## Updated regression

Commit:
```
bd31307fbda3d512a579f521efc1664199b1de46
```

Fixture:
- wide 3D static region;
- symmetric safe +/-Z branches;
- current velocity intentionally does not point toward -Z;
- accepted local continuity explicitly points toward -Z;
- next AdjustedClear must preserve -Z.

## Updated final composite

Commit:
```
09bc81e03cab6c251b50678585161cc73e814ddb
```

On every accepted dynamic bypass:
- store normalized selected-target direction;
- execute the accepted short physical program;
- on next replan supply the stored direction as continuity;
- update it only after a new AdjustedClear is accepted.

`[COMPOSITE-RESUME]` now prints the continuity vector.

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

## Interpretation

If focused regression fails:
- explicit continuity is not reaching/scoring correctly.

If focused regression passes but composite still oscillates:
- a vector hint is insufficient; next step is an explicit accepted local branch/plane identity rather than another heuristic.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior lab;
- immediately move to real NAV STRESS/game accepted-corridor + trajectory visualization.

Do not weaken physical/safety thresholds.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
