# CURRENT TASK — Validate dense ManeuverProgram source preservation before direct actuator execution

Date: 2026-09-22

Status: **SPARSE-ALIAS ROOT CAUSE FIXED / TARGET VALIDATION REQUIRED**

Code baseline before documentation commits:

```text
b51b0abc22270104f75f198935d857582c9b4445
```

## Fresh target evidence

The failing run had:

```text
TRAJECTORY SAMPLES: 693
PROGRAM PHASES: 3
PLANNED ACTUATOR SEGMENTS: 45
PLANNED ACTUATOR INFEASIBLE: 4
AUTOPILOT ACTUATOR EXECUTION: OBSERVE-ONLY MIGRATION
```

This is the core mismatch.

A 693-sample physical trajectory was compressed into only 45 actuator
intervals. B9 then linearly interpolated P/V/A/attitude between distant keys.

At the beginning of the run Planner reported:
`plan_main_pct=0, plan_rcs=(0,0,0)`,
while Follower immediately generated a lateral acceleration correction and
actual RCS became active.

## Engine-selection rule in current live path

Actual execution still does NOT obey the Planner actuator schedule.

Current live allocator receives a net acceleration vector from
Follower -> PilotSkillExecutor and does:

```text
forwardComponent = dot(executedAcceleration, actualHullForward)

if forwardComponent > 0:
    rear main supplies the positive forward part

RCS supplies the residual vector
```

So if the accepted feed-forward is missing and Follower's correction points
sideways/backward, RCS performs the work.

## What the oscillation work changed

`e2b5270`:
- Kd 1.0 -> 3.0;
- no propulsion logic change.

`56aca8a`:
- removed dense angular derivative alias;
- re-derived omega from sparse accepted bases;
- correct fix for course oscillation;
- but retained the bad <=16-key compression of a long physical trajectory.

Thus oscillation removal did not directly disable the main engine. It removed
an angular artifact while leaving state/feed-forward sparsification in place.
The remaining alias then became obvious as RCS-only tracking.

## Current correction

`9d4b599...` changes route-program construction:

```text
OLD:
entire route leg -> <=16 uniformly spaced accepted samples

NEW:
dense source samples -> consecutive chunks of <=16
adjacent chunks share one boundary sample
```

No P/V/A/attitude state is skipped between accepted chunks.

New diagnostic:

```text
PLANNED ACTUATOR SOURCE COVERAGE: N/N COMPLETE
```

The new invariant should equal:
`planned actuator segments == trajectory samples - 1`.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Repeat the same Newtonian / Expert / Standard case.

Check summary first:
- PROGRAM SOURCE SAMPLING: CONSECUTIVE DENSE CHUNKS
- PLANNED ACTUATOR SOURCE COVERAGE: N/N COMPLETE
- program phases should be much more than 3 for ~693 dense samples.

Then inspect the first bend:
- planned MAIN/RCS line;
- pink current reference;
- actual engine lamps.

If Planner now emits a sensible main+RCS schedule, next iteration removes
`OBSERVE-ONLY` and makes Autopilot execute that schedule directly.

If Planner still emits MAIN=0 through the material turn, inspect
`propulsionReferenceForward()` / required acceleration geometry next; do not
blame the damping loop and do not loosen tracking envelopes.
