# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow

Read current `main` versions of `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, `src/game/navigation/STAGE12_END_TO_END.md`, and the
production/tests involved in the active gate.

After every state-affecting event synchronize those files and recreate this
`CONTINUE_PROMPT.md` from scratch.

## Evidence boundary

Last accepted target baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest actually target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Current unverified production/test code baseline before documentation sync:
```
abfd7a6a26168f177968f0dd4299f3c712105fc0
```

## Correct B4 behavior

Ordinary local avoidance is receding-horizon.

```text
safe short segment available
    -> prove it
    -> execute it
    -> keep forward progress
    -> no mandatory same-horizon return to nominal line

no safe geometric short segment
    -> ConflictHold
    -> active braking intent
    -> navigation remains active

geometric segment exists but physical authoring cannot execute it in time
    -> active braking through real control/physics
    -> keep world truth authoritative
    -> replan again

after obstacle clears / enough room exists
    -> progressively reacquire nominal line
```

There is no fixed 30 m merge distance.

`mergeTargetMapMeters` is only an on-route reacquisition reference, not a
mandatory endpoint of the current local segment.

## Current implementation

- B4 proves only the short segment executed now.
- Mandatory bypass->merge static and dynamic proof was removed.
- Equal lateral offsets prefer farther forward stations.
- `ConflictHold` remains command-producing braking, not planner shutdown.
- Composite now brakes if `fitAuthorityBoundedReplacement()` cannot author the
  geometric bypass within current ship authority.
- The brake path uses the real `NavigationRuntimeControlBridge`,
  `SharedShipPhysics`, and `DynamicMotionSystem`.
- After braking the same live hazard is republished and navigation replans.

## Tests

- Invalid portal blocker fixture repaired: start X=0, blocker X=3.5, staging X=7.
- `testBypassDoesNotRequireImmediateReturnToTrajectory()` proves a safe short
  bypass is accepted even when immediate route return is deliberately blocked.
- No-space regression requires explicit exhaustion plus active braking.
- Composite accepts either a physically valid bypass or the braking fallback;
  it must not fail merely because the geometric target is not authorable.

## Must stay removed

Do not restore angular fans, azimuth branches, branch continuity state,
same-branch ranking, branch-switch API, or mandatory brake-before-side-change.

## Next task

Run the target-machine architecture + navigation runtime gate on the exact
pulled HEAD and inspect the first real behavioral failure, if any.

Command:
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
echo "$PWD/$OUT"
```

Do not weaken safety radii, padding, acceleration limits, or tracking
tolerances simply to make the gate green.
