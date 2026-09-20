# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest actually tested checkout

```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Target evidence:
- architecture PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- failures in runtime planner and final composite.

## Current unverified code baseline before state-sync commits

```
70dc7bb9c024a388c39b79d54a12774475ca8b32
```

## B4 contract corrected after first target failure

The former same-horizon two-segment requirement is rejected.

Canonical ordinary local behavior is now:

```text
accepted route / trajectory
    -> physical visible horizon
    -> predicted dynamic/static occupancy
    -> can a safe executable short segment avoid the obstacle?
         yes -> accept that short off-route segment and keep moving
         no  -> issue active braking intent and keep navigation alive
    -> next receding-horizon update
         -> continue bypass while required
         -> begin reacquiring nominal line when safe
         -> converge back under physical steering/acceleration limits
```

There is **no arbitrary distance at which the ship must return to the route**.
In particular, no 30 m merge requirement exists.

The on-route `mergeTargetMapMeters` / runtime mirror is currently only a
reacquisition reference. It is not a mandatory endpoint of the current local
segment and is not required to be reachable inside the same horizon.

## Navigation continuity on failure

`ConflictHold` does not switch navigation off.

Runtime behavior is command-producing:

```text
no safe current bypass
    -> ConflictHold
    -> holdIntent
    -> negative velocity demand / braking
    -> physics continues
    -> navigation planner remains active
    -> fresh world state is evaluated again
```

This matches the required behavior: if there is time/space to evade, evade; if
there is not, brake and use the space that remains while continuing navigation.

## Current bypass selection

For equal lateral clearance the local solver now prefers the farther forward
bounded bypass station. This reduces gratuitous sideways kinks and better
preserves the route direction.

The selected short segment is proved against:
- exact static geometry;
- projected moving occupancy;
- time-coupled dynamic separation.

Physical maneuver authoring/capability proof remains downstream ownership.

## Test corrections

The stale portal-attitude fixture was corrected:
- start moved to X=0;
- blocker moved to X=3.5;
- staging/reacquisition reference remains X=7.

This makes both endpoints outside the 2.75 m required dynamic separation while
the nominal path is still genuinely blocked.

The old regression `testReturnLegIsAlsoProvenAgainstExactStaticGeometry()` was removed.
It is replaced by `testBypassDoesNotRequireImmediateReturnToTrajectory()`, which
deliberately blocks an immediate return leg while leaving a valid short bypass available.

The no-space regression remains and requires:
- `ConflictHold`;
- `localBypassExhausted`;
- active emergency braking intent.

## Validation status

**UNVERIFIED on target MinGW64.**

The next run must verify the new receding-horizon contract. Do not promote B4
until the target gate is green and the final composite demonstrates physically
reasonable behavior.

## Documentation protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.

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
