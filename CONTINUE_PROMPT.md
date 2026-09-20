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
51e6c41bb94b65e8cc269fb035164a4eb0aa23fd
```

Architecture PASS. Runtime 18/19.

The focused accepted-segment continuity regression is now PASS.

The final composite still failed because continuity ranking was constrained to the first safe deflection ring.

Logged example:
```
continuity=(0.357512,-0.349550,+0.866025)
position=(181.656981,57.946838,25.984828)
selected target=(172.399109,59.234718,-2.521893)
```

The target is almost opposite the accepted +Z branch.

## Root cause

The production search still did:
```
for ring from small to large:
    rank azimuths by continuity
    if any safe candidate:
        return
```

Therefore a smaller-angle opposite-side candidate could beat a slightly larger-angle same-branch candidate.

## Current unverified production fix

```
e19c1804806ce5f3554f20c7b7d3d5ac19b4911e
```

When explicit continuity exists:
- evaluate all safe candidates across all ordinary rings;
- safe candidates with positive alignment to accepted direction form the preferred branch class;
- preferred branch class beats opposite branch class;
- then maximize continuity dot;
- then prefer smaller deflection;
- then smaller azimuth index.

If no safe same-branch candidate exists anywhere, choose the least-opposed safe fallback.

Without explicit continuity, legacy smallest-ring priority remains.

Public semantic comment:
```
c3dcf98b16bd6e45f0dbc949ec926f086aa623a0
```

## Strengthened regression

```
06a917f058926a29f41a936dd994be3f8073e7cf
```

Fixture:
- accepted branch = -Z;
- first-ring -Z is blocked by exact static geometry;
- first-ring +Z remains safe;
- larger-ring -Z is safe.

Required:
- AdjustedClear;
- selected target remains -Z;
- selected deflection must exceed the primary ring.

This pins cross-ring branch preservation, not merely same-ring azimuth ranking.

## Composite ownership/diagnostics

```
e62328d4af99b6e452e2d07e53cd20e25a103dcf
a2da6453daaa8e00cc1f4661321b9294513057ff
```

`[COMPOSITE-RESUME]` now prints selected deflection.

Continuity is updated only after a physically valid local program successfully executes, using executed program start->terminal displacement.

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

## If green

Record exact target checkout and composite metrics, close synthetic maneuver behavior laboratory, and move immediately into real NAV STRESS/game visualization.

## If red

Use continuity + selected deflection diagnostics. If branch still flips despite a safe same-branch candidate, the vector hint is insufficient and should be promoted to explicit branch/plane identity.

Do not weaken physical, tracking, hull, clearance or terminal criteria.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
