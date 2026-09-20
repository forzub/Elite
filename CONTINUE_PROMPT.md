# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow

At the beginning of every dialog read the current `main` versions of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `NAVIGATION_WORLD_V2.md` when local-navigation semantics are relevant
- the production/tests involved in the active failure.

After every state-affecting project event synchronize the state/task/stage
documentation and recreate this entire `CONTINUE_PROMPT.md` **from scratch**.

## Evidence boundary

Last accepted target-machine baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest actually target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

That target run passed architecture and build/link but finished 17/19 runtime
tests. It exposed an invalid forced same-horizon merge assumption.

Current unverified production/test code baseline before documentation commits:
```
70dc7bb9c024a388c39b79d54a12774475ca8b32
```

The exact pulled HEAD of the next target-machine run must be recorded; commits
after the code baseline are documentation/state synchronization only unless
inspection proves otherwise.

## Corrected B4 contract

Ordinary local avoidance is receding-horizon.

```text
accepted route / nominal trajectory
        |
        v
physical visible horizon
        |
        +-- projected dynamic occupancy
        +-- exact static geometry
        |
        v
is there a safe short segment we can execute now?
        |
        +-- YES -> AdjustedClear
        |          accept short off-route segment
        |          keep forward progress
        |          no mandatory same-horizon return
        |
        +-- NO  -> ConflictHold
                   active braking intent
                   navigation remains active
                   re-evaluate fresh world truth

next horizon:
    continue bypass if still necessary
    otherwise begin progressive route reacquisition
```

There is no fixed 30 m merge distance.

`mergeTargetMapMeters` / runtime mirror is currently only an on-route
reacquisition reference. It is not a current-segment endpoint that must be
reachable or safe inside the same horizon.

## Important ownership

- B4 chooses/proves bounded local geometry.
- B5/B6 own physical maneuver authoring and capability/continuous proof.
- A geometric `AdjustedClear` is not by itself proof the ship can physically
  execute the maneuver.
- If physical authoring cannot evade in time, higher maneuver ownership must
  brake/replan while navigation remains active.
- Follower tracks an accepted program; it does not invent a new maneuver.
- Stable automatic execution does not replan every frame.

## What changed in the current candidate

Production:
- removed mandatory exact-static bypass->merge proof;
- removed mandatory dynamic return-leg proof;
- selected short segment alone is proved for the current horizon;
- equal lateral offsets prefer the farther forward station, reducing needless
  sideways kink;
- no fan/branch continuity mechanism was restored.

Tests:
- portal-attitude fixture corrected from impossible endpoint geometry:
  start X=0, blocker X=3.5, staging/reacquisition reference X=7;
- `testReturnLegIsAlsoProvenAgainstExactStaticGeometry()` removed;
- new `testBypassDoesNotRequireImmediateReturnToTrajectory()` deliberately
  makes immediate return impossible while keeping a safe short bypass;
- obstacle-gone test now describes nominal-line reacquisition without planner
  shutdown rather than an artificial immediate merge;
- no-space test still requires `ConflictHold`, explicit bypass exhaustion and
  active braking intent;
- architecture checker pins `timeCoupledSegmentClear` and the new regressions.

## Removed mechanisms that must stay absent

Do not restore:
- angular deflection rings/fan;
- azimuth branches;
- branch continuity hints/state;
- same-branch ranking;
- branch-switch-required API;
- mandatory Brake-before-changing-side recovery.

## Next task

Run the fresh target-machine Stage-12 architecture + navigation runtime gate.

Pay special attention to:
- `navigation_runtime_planner`;
- `navigation_composite_proving_ground`;
- bypass offset and forward distance;
- projection/static/dynamic rejection counts;
- whether a geometrically valid short bypass reaches physical authoring;
- if physical authoring rejects it, confirm braking/replan rather than
  navigation shutdown.

Do not weaken radii, padding, safety margin, acceleration limits, or tracking
tolerances merely to get green.

## Target command

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

## Acceptance

Do not accept B4 until fresh target evidence shows:
- valid short bypass when safe local free space exists;
- active braking when it does not;
- navigation ownership remains alive;
- no forced same-horizon route return;
- no legacy angular fan/branch mechanism;
- final composite behavior is physically plausible.
