# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Evidence boundary

Accepted baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Result: architecture PASS, runtime build PASS, 17/19 tests PASS.

## Task

Diagnose and correct the first target-machine failures of the hard-replaced B4
two-segment projected visible-horizon solver **without weakening safety
clearance and without restoring the removed fan/branch mechanism**.

## Proven fixture problem

`testAdjustedVisibilityDoesNotInheritFuturePortalAlignment()` cannot satisfy
its own current dynamic proof: required separation is 2.75 m while blocker
distance to both start and merge is 2.5 m. Repair the geometry, not the safety
rule.

## Composite problem

The first composite replan has a 30 m merge point while the hazard is 48 m
ahead. The merge is only 18 m from the hazard, but required clearance is
26.775995 m (about 18.51 m after the hazard's 4 s lateral motion).

Thus a complete return to the current fixed merge point is impossible under
the current two-segment contract.

## Immediate work order

1. Make first-failure diagnostics print projection/static/dynamic rejection
   counts.
2. Fix only the stale focused test geometry.
3. Add a regression for `merge target lies inside dynamic exclusion envelope`.
4. Determine whether production B4 must:
   - sample a later merge station;
   - extend the physical horizon when a demonstrated bypass requires it; or
   - keep an accepted off-route continuation across receding-horizon updates
     and reacquire later.
5. Do not simply increase offsets, reduce radii/padding, or force green.
6. Re-run the 19-test target gate.

## Exit criterion

A fresh target-machine green gate plus evidence that the solver can handle a
real recoverable obstacle whose safe reacquisition lies beyond the first
bounded nominal point.
