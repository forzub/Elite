# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested checkout

```
8011cc3ed19fc027fba256ee4aecca7c93a4ce0f
```

Architecture PASS; runtime 17/19.

## What the latest log proved

At the failing persistent-hazard replans:

```
continuity_lateral_valid=1
same_branch_safe=0
```

and later:

```
selected_branch_alignment=-0.976922
```

So the accepted branch is genuinely unavailable.

The planner is allowed to select another safe branch, but execution must not try to jump to it as a no-stop continuation.

## Current production signal

```
avoidanceBranchSwitchRequired
```

is emitted when:
- accepted transverse continuity exists;
- no safe same-branch candidate exists;
- a safe adjusted target exists elsewhere.

Commits:
```
4a91a78bea1e159a329ab346586a4d290ea5d420
9949b701bc8ba181a08b96e8077a375aada725be
7a5b4ae0520a10edbd89fd5f1e81f2427f4768be
e407857d7764300193075cbee941be82312338f8
```

## Focused regression correction

```
c8e0c7cd6b6a7deb6c7f618c4d86ea80d4c62400
```

The blocker is now centered on the actual 15-degree -Z probe endpoint:
```
(579.555496, 0, -155.291427)
```

A safe larger-ring same-side candidate must suppress branch-switch escalation.

## Composite recovery candidate

```
0ecf1b71b9620022a49ea71c996f5e81c02e5243
```

On `avoidanceBranchSwitchRequired`:
- reject direct opposite-side continuation;
- build conservative physical Brake recovery;
- peak full acceleration <=1.35 m/s2;
- planned static/dynamic clearance >=1.5 m;
- execute through real follower/PilotSkill/physics;
- actual clearances >0.5 m;
- zero tracking-envelope violations;
- stop to <=0.60 m/s;
- clear old continuity;
- replan from actual recovered state.

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
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE-RESUME\]|\[COMPOSITE-CONTINUATION\]|\[COMPOSITE-RECOVERY\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|NAVIGATION RUNTIME PLANNER TESTS|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

## Interpretation

If focused regression fails:
- inspect exact-static blocker geometry / branch diagnostics.

If recovery cannot be authored:
- inspect whether stopping itself is unsafe in the current state; do not force a branch switch.

If recovery executes but replan still oscillates:
- inspect post-stop branch selection with continuity intentionally cleared.

If 19/19:
- accept final composite;
- close synthetic behavior lab;
- move to real NAV STRESS/game.

## Iteration rule

After every state/evidence change, synchronize all MDs and recreate `CONTINUE_PROMPT.md` from scratch.
