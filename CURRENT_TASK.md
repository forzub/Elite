# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest actually tested checkout

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

Architecture PASS; runtime behavior did not execute because of the historical
missing-`<iomanip>` diagnostic compile failure.

## Current unverified replacement baseline before state-doc sync

```
f8cd91d01006d9cba8327ae51efb6705e6df83e0
```

## Task

Validate the **hard-replaced B4 visible-horizon local solver** on the target
machine.

Do not repair, preserve, or re-enable the removed angular-fan / branch mechanism
to make tests pass.

The production path under test is:

```text
accepted trajectory
 -> visible physical horizon
 -> predicted unexpected dynamic occupancy
 -> projection onto normal plane
 -> metric lateral/vertical offset search
 -> exact-static proof
 -> time-coupled dynamic proof
 -> bypass target + nominal merge target
 -> physical maneuver compilation/proof
 -> execution
 -> nominal trajectory reacquisition
```

## Required behavior

Focused regressions must establish:
- clear nominal route performs no offset search;
- crossing moving obstacle produces a projected bypass;
- head-on obstacle with genuine lateral room does not require a mandatory stop;
- exact static geometry constrains/rejects offsets;
- disappearing conflict returns directly to the original trajectory;
- no fitting offset fails closed through `localBypassExhausted`;
- stale dynamic truth fails closed;
- runtime planner publishes metric bypass offset + merge target;
- composite persistent-hazard replans use fresh world truth without branch
  continuity/recovery state.

## Forbidden response to failures

Do **not**:
- restore 15/30/45/60/75-degree fan search;
- restore azimuth branch identity;
- restore accepted-branch continuity hints;
- restore Brake-before-branch-switch as ordinary avoidance;
- weaken static/dynamic clearance simply to obtain green tests;
- make a second parallel local planner.

If a target-machine failure occurs, diagnose the replacement mechanism itself
or its fixture/API integration.

## Next validation

Run the architecture + navigation runtime target gate from `CURRENT_STATE.md`
or the freshly recreated `CONTINUE_PROMPT.md`.

If green:
- record exact tested HEAD and exact metrics;
- promote only the evidence actually proved;
- synchronize all required MD files;
- recreate `CONTINUE_PROMPT.md` from scratch;
- then decide whether the synthetic behavior lab is sufficiently closed to move
  into actual NAV STRESS/game visual evaluation.

## Iteration rule

After every state/evidence change, synchronize all project state MDs and
recreate `CONTINUE_PROMPT.md` from scratch.
