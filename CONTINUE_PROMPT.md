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
8011cc3ed19fc027fba256ee4aecca7c93a4ce0f
```

Architecture PASS. Runtime 17/19.

Failures:
- focused cross-ring continuity regression;
- final composite.

## Decisive composite diagnostics

The failing persistent-hazard replans report:

```
continuity_lateral_valid=1
same_branch_safe=0
```

and later:
```
selected_branch_alignment=-0.976922
```

This means the accepted branch is no longer safely available.

Do not continue trying to force that branch.

## New production signal

Commits:
```
4a91a78bea1e159a329ab346586a4d290ea5d420
9949b701bc8ba181a08b96e8077a375aada725be
7a5b4ae0520a10edbd89fd5f1e81f2427f4768be
e407857d7764300193075cbee941be82312338f8
```

`NavigationRuntimePlanner::Result::avoidanceBranchSwitchRequired` is true only when:
- accepted continuity exists;
- transverse branch is meaningful;
- no safe same-branch candidate exists;
- some safe adjusted target exists on another branch.

This is an escalation signal, not steering authority.

## Focused regression

Commit:
```
c8e0c7cd6b6a7deb6c7f618c4d86ea80d4c62400
```

The exact-static blocker now sits directly on the actual 15-degree / 600 m preferred -Z probe endpoint:
```
(579.555496, 0, -155.291427)
```

A larger-ring -Z continuation remains free.

The test must prove:
- same-branch safe count >0;
- branch switch required = false;
- selected target stays on -Z;
- selected deflection > primary ring.

## Final composite recovery candidate

Commit:
```
0ecf1b71b9620022a49ea71c996f5e81c02e5243
```

When branch switch is required:
1. do not execute the opposite adjusted target directly;
2. fit a conservative Brake program from actual live P/V;
3. hold current body attitude;
4. dense proof:
   - peak total acceleration <=1.35 m/s2;
   - planned dynamic clearance >=1.5 m;
   - planned static clearance >=1.5 m;
   - no velocity reversal;
5. execute with StateCapture through B9/B10 -> PilotSkill -> real physics;
6. require zero tracking violations and >0.5 m actual clearances;
7. require final speed <=0.60 m/s;
8. clear obsolete accepted branch continuity;
9. republish hazard at current time;
10. call planner again from recovered state.

New diagnostic:
```
[COMPOSITE-RECOVERY]
```

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE-RECOVERY\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Interpretation

If the cross-ring regression is green but branch-switch recovery triggers in composite:
- that is expected when same_branch_safe == 0.

If recovery cannot be physically proved:
- fail closed and inspect the current state; do not execute the new branch.

If recovery succeeds:
- old branch is retired and next plan is fresh from the recovered state.

If 19/19:
- accept exact target checkout;
- close synthetic maneuver behavior laboratory;
- move immediately to real NAV STRESS/game visualization and live behavior review.

Do not weaken physical/safety criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
