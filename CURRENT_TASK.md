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

Current unverified code baseline before documentation sync:
```
abfd7a6a26168f177968f0dd4299f3c712105fc0
```

## Task

Run the first target-machine gate for the corrected B4 receding-horizon bypass.

The contract under test is:

```text
safe short avoidance segment exists
    -> AdjustedClear
    -> continue forward through the safe segment
    -> do NOT require immediate same-horizon return to the route

no safe short avoidance segment exists
    -> ConflictHold
    -> active braking command
    -> navigation remains active
    -> re-evaluate on fresh world truth
```

After the obstacle is passed, route reacquisition is progressive under physical
control limits. There is no fixed 30 m return distance.

## What changed

Production:
- removed mandatory exact-static proof of bypass->merge from ordinary B4;
- removed mandatory time-coupled dynamic proof of the return leg;
- current proof covers the short segment actually selected for execution;
- equal-offset candidates prefer more forward progress;
- on-route merge point is now only a reacquisition reference.

Tests:
- repaired invalid portal/blocker geometry;
- replaced mandatory-return regression with a regression that requires a valid
  bypass even when immediate return is blocked;
- preserved no-space -> active braking coverage;
- architecture checker now pins the short-segment/receding-horizon contract.

## Next gate

Run architecture + navigation runtime on the exact pulled HEAD.

Inspect especially:
- `navigation_runtime_planner`;
- `navigation_composite_proving_ground`;
- selected bypass offset and forward distance;
- projection/static/dynamic rejection counts;
- whether the composite can either execute a safe bypass or correctly brake
  when physical authoring says it cannot evade.

Do not turn a geometric AdjustedClear into a claim of physical executability;
B5/B6 still own maneuver capability/proof.

## Exit criterion

Green target gate with:
- valid bypass when safe space exists;
- active braking when no safe short segment exists;
- no navigation shutdown;
- no forced same-horizon merge;
- no restored angular fan/branch mechanism.

## Composite physical fallback

The final composite no longer treats `fit.valid == false` as an automatic
test failure. If the geometric B4 segment cannot be authored within the
vehicle's current physical authority, the test now executes active braking
through the real PilotSkill/physics path, keeps the hazard authoritative,
and then continues the receding-horizon replan loop.

This directly pins the required rule:
- can evade physically -> execute bypass;
- cannot evade physically -> brake;
- navigation ownership remains active in both cases.
