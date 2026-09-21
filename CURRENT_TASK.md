# CURRENT TASK — validate minimum-cant Newtonian attitude on the clean scalar path

**Date:** 2026-09-21  
**Status:** SCALAR PATH LOOKS MUCH BETTER / EXCESSIVE NEWTONIAN HULL ROTATION ROOT-CAUSED / FIX UNVERIFIED

## Latest evidence

Latest navigation perf for the default wall stand includes:
```text
legs=1
guide_points=10
rounded_corners=2
min_speed_mps=10.0000
max_speed_mps=10.0000
valid=1
```

So the previous dense-waypoint / near-stop problem is no longer the dominant issue.

The uploaded viewer video shows the hull rotating dramatically broadside to the velocity
vector during the gentle turn.

## Root cause

`buildReferenceAttitudes()` previously did:
```cpp
if (Newtonian && |acceleration| > 0.35)
    nose = normalize(acceleration);
```

At constant speed on a curve, acceleration is centripetal and almost perpendicular to
velocity. So the code explicitly ordered the broadside rotation.

This was unnecessary because the same trajectory is already curvature-limited by the
ship's manoeuvre/RCS acceleration authority.

## Candidate fix

Newtonian attitude is now minimum-cant:
- keep nose along velocity while RCS can reproduce the requested acceleration;
- only rotate enough for main-engine participation when the RCS sphere is insufficient;
- hard braking / high lateral demand may still legitimately require a large rotation.

Commits:
- fcffa5bae3b4e3deab5d6f043d3c500137719dad
- b943064d529935243863c30238ae0daf62f1c129

New execution diagnostic:
```text
MAX REFERENCE/VELOCITY ANGLE
```
This separates planned attitude from actual body tracking.

## Immediate target validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Test Expert / Standard / Newtonian first.

Check:
- NAV REV visible and current;
- purple path remains clean;
- red reference nose should stay much closer to yellow velocity on the gentle turn;
- cyan physical hull should follow red without large unnecessary broadside rotation;
- report `MAX REFERENCE/VELOCITY ANGLE` and `MAX BODY/VELOCITY ANGLE`.

Then compare Assisted.

## Decision

If reference/velocity angle is small but physical body/velocity angle remains large:
- the remaining defect is angular tracking / body-axis execution.

If both remain large:
- revisit minimum-cant allocation or acceleration decomposition.

Do not enable dynamic avoidance yet.

## Mandatory state protocol

Every iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
