# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest target-machine attempt

Exact tested checkout:

```
0ad327ad63d3e63f8c204b2e59a6f224f80c8fee
```

Results:
- architecture contract PASS;
- runtime 17/19;
- failures:
  - `navigation_runtime_planner` strengthened cross-ring continuity regression;
  - `navigation_composite_proving_ground`.

## Key evidence

The strengthened regression failed on:
```
accepted branch continuity must prefer a larger same-side ring over a smaller opposite-side ring
```

The final composite still showed:
```
continuity=(0.357512,-0.349550,+0.866025)
selected target=(172.399109,59.234718,-2.521893)
selected_deflection_deg=60
```

Thus the explicit hint was present, but branch classification/ranking was still wrong.

## Root cause

We classified "same branch" using the full direction dot product.

That is incorrect for progress-preserving visibility rays:
- all candidates share a large forward component;
- opposite bypass sides may both have positive full-direction dot;
- therefore +Z and -Z can both be mislabeled as the same branch.

Avoidance branch identity must be based on the **transverse component relative to the current nominal forward**.

## Current unverified production fix

Production commits:

```
b8bcaae6c5af366e8cabcefb86fbe104c120b010
caa8da847b0f48ea2d72d2fa8c042c1d599c73b8
88f63b0d62803d73e7f3694d1c4f30bcbc1925d0
9422dddfab64f70f94773ebcacf2167be1daacbe
e9655a4b9f004fe790adbbc287bd55a3f1c269ea
```

New semantics:
1. project accepted continuity direction into the plane perpendicular to current nominal forward;
2. project each candidate direction into the same plane;
3. branch alignment = dot(normalized lateral candidate, normalized accepted lateral);
4. positive transverse alignment = same branch;
5. same branch outranks opposite branch;
6. within the same branch class, choose the **smallest safe deflection ring**;
7. inside that ring, maximize transverse branch alignment;
8. full direction continuity and azimuth index are deterministic lower-level tie-breaks.

If the accepted continuity has no meaningful transverse component, the planner falls back to full-direction continuity rather than inventing a branch.

## New diagnostics

Local/runtime result now exposes:
- continuity hint used;
- continuity lateral valid;
- number of safe same-branch candidates;
- selected branch alignment.

Composite `[COMPOSITE-RESUME]` prints:
- selected deflection;
- lateral-valid flag;
- same-branch safe count;
- selected branch alignment;
- accepted continuity vector.

## Focused regression correction

Commit:
```
0e344f3a3b2a5e887cbb474ce9ce650b68851716
```

The fixture now uses four azimuth samples so the preferred first-ring -Z branch is a single deterministic candidate and can be blocked exactly.

The regression requires:
- explicit continuity hint used;
- meaningful transverse continuity;
- at least one safe same-branch candidate exists;
- selected branch alignment > 0.5;
- selected deflection is larger than the blocked primary ring;
- selected target remains on -Z.

This now tests the intended cross-ring branch rule rather than ambiguous diagonal first-ring candidates.

## Current gate

Expected suite remains **19 tests**.

If focused regression passes and composite reports:
- same_branch_safe > 0;
- selected_branch_alignment > 0;
then branch preservation is functioning.

If composite still switches to the opposite side while same_branch_safe > 0, the ranking remains wrong.

If same_branch_safe == 0, switching branch is physically justified and the next missing behavior is a recovery/braking maneuver before the branch switch rather than a no-stop continuation.

No physical, tracking, clearance, hull or terminal criterion has been weakened.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
