# CONTINUE PROMPT — Elite Navigation: validate dense accepted maneuver source

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST update:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- recreate this file from scratch.

## Current root cause

Fresh failing target run:

```text
TRAJECTORY SAMPLES: 693
PROGRAM PHASES: 3
PLANNED ACTUATOR SEGMENTS: 45
AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION
```

The accepted program was massively undersampling the dense trajectory.

At runtime the Planner actuator interval could say MAIN=0/RCS=0 while Follower
generated ~1.5 m/s^2 lateral correction and actual RCS performed the maneuver.

## Important historical finding

The course-oscillation fix did NOT directly change engine allocation.

`e2b5270` only raised angular damping Kd.

`56aca8a` correctly removed dense angular-derivative smear and re-derived
angular velocity from sparse accepted basis keys.

But the full P/V/A/attitude trajectory was still compressed into <=16 keys per
long route leg. That representation remained wrong.

## Current code correction

`9d4b599c7339fc7dc30803a0aa3c57b7b543fd5a`:

- do not uniformly compress an arbitrarily long route leg to <=16 keys;
- split it into consecutive dense chunks of <=16 samples;
- chunks share a boundary state;
- therefore B9 interpolation only spans adjacent dense source samples.

Additional diagnostic code baseline before docs:

```text
b51b0abc22270104f75f198935d857582c9b4445
```

Summary now reports:
`PLANNED ACTUATOR SOURCE COVERAGE: actual/expected COMPLETE`.

Expected invariant:
`actual == dense trajectory sample count - 1`.

## Current actual engine-selection rule

Planner actuator schedule is STILL observe-only.

Live path:
```text
Follower net acceleration
 -> PilotSkillExecutor
 -> DynamicMotionSystem

dot(acceleration, actualHullForward) > 0
    -> aft main gets positive forward component

residual
    -> RCS
```

So do not claim Planner directly controls engines yet.

## Next run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Repeat same Newtonian / Expert / Standard high-speed case.

Check:
1. `PROGRAM SOURCE SAMPLING: CONSECUTIVE DENSE CHUNKS`.
2. `PLANNED ACTUATOR SOURCE COVERAGE: N/N COMPLETE`.
3. program phase count should no longer be only 3 for ~693 source samples.
4. At first material turn, inspect planned MAIN/RCS.

If planned engine schedule is now sane, next iteration wires sampled
`ActuatorSegment` into Autopilot/physics.

If MAIN still remains zero in Planner schedule, inspect
`propulsionReferenceForward()` and trajectory acceleration decomposition next.
Do not undo critical damping merely to make main thrust appear accidentally.
