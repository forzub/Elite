# CURRENT TASK — Validate distinct Newtonian propulsion behavior + live engine indicators

Date: 2026-09-22

Status: **CODE CANDIDATE READY / TARGET MINGW64 VALIDATION REQUIRED**

Code baseline before documentation commits:

```text
421ecb1b2721d54bc0c33737a520100a3c10fac9
```

## Fresh target evidence

```text
NEWTONIAN / EXPERT / STANDARD
START 21.20 m/s
FINISH 21.20 m/s

route additional clearance   17.71 m
Ruckig min/max               20.73 / 21.20 m/s
program phases complete      NO
physical terminal state      MISSED
final position error         181.42 m
final speed                  8.17 m/s
reference hold               40.61 s
max body/velocity angle      174.31 deg
```

Planner behavior is directionally sane: higher speed widened the static detour.
The current problem is physical execution.

## Root cause

Cobra physical RCS authority is 2.0 m/s^2. The Ruckig point-mass route asks for
roughly 1.5 m/s^2 over much of this run.

Old Newtonian reference authoring said:

```text
if desired acceleration <= full RCS authority
    keep nose on travel tangent
    let RCS do all of it
```

Result: Newtonian became effectively Assisted-like. The hull stayed near the
route reference while manoeuvre thrusters continuously changed velocity and
bled speed. Main engine did not participate until body/velocity were already
almost opposite.

## Candidate correction

`propulsionReferenceForward(sample, previous, law, params)` is law-specific:

- Assisted: may consider full real RCS authority for its coupled-flight
  attitude choice.
- Newtonian: only 0.35 m/s^2 (existing tiny-correction threshold) counts as
  primary attitude-authoring RCS authority.
- Above that, Newtonian authors a physical hull cant/flip toward the requested
  acceleration so aft main can participate.
- Actual physical RCS limit remains 2.0 m/s^2 downstream for recovery/trim; it
  is not artificially removed.

## Viewer indicators

Bottom of screen now always shows three fixed lamps:

```text
[ ] МАРШЕВЫЙ
[ ] ПЕРЕДНИЙ МАРШЕВЫЙ
[ ] МАНЕВРОВЫЙ
```

They light from physical acceleration channels, not from planned intent:
- `МАРШЕВЫЙ` = main acceleration projected along hull forward;
- `ПЕРЕДНИЙ МАРШЕВЫЙ` = negative main projection (should never light on
  current Cobra);
- `МАНЕВРОВЫЙ` = non-zero real RCS/manoeuvre acceleration.

This makes the next visual pass immediately diagnostic.

## New regression

`testNewtonianHigherSpeedUsesMainEngineDominantManeuver()`

Exact fixture:
`NEWTONIAN / EXPERT / STANDARD, 21.20 -> 21.20 m/s`.

Checks:
- main engine materially participates within first 8 s;
- no impossible fore main thrust is produced.

Existing Assisted and high-speed reacquisition regressions remain.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First interactive gate:
`NEWTONIAN / EXPERT / STANDARD, 21.20 -> 21.20 m/s`.

What should visibly change:
1. Early material route correction must no longer be `МАНЕВРОВЫЙ` alone for
   tens of seconds.
2. Hull should lead-rotate/cant toward the required burn direction.
3. `МАРШЕВЫЙ` should light when the aft engine actually supplies the material
   delta-v.
4. Rotation alone must not bend velocity.
5. `ПЕРЕДНИЙ МАРШЕВЫЙ` should remain dark.
6. Short RCS trim is fine; sustained RCS-only route propulsion is not.

If main participation is now visible but the craft still misses the Ruckig
curve, the next correction is finite lead-rotation timing in maneuver authoring:
the point-mass trajectory currently does not reserve time/distance for the hull
to acquire burn attitude before the acceleration segment.

Do not enable dynamic avoidance.
Do not widen tracking envelopes/tolerances to hide the mismatch.
