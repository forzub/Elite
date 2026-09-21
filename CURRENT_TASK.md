# CURRENT TASK — Validate hull-coupled Assisted braking at higher speed

Date: 2026-09-22

Status: **CODE CANDIDATE READY / TARGET VALIDATION REQUIRED**

Code baseline before documentation commits:

```text
b833eddb7bd04b5c025b2be0fd8334c33f8824e6
```

## Fresh reproduced failure

```text
ASSISTED / EXPERT / STANDARD
START 20.90 m/s
FINISH 20.00 m/s

Ruckig max speed          20.90 m/s
program phases complete   NO
physical terminal state   MISSED
final position error      175.20 m
final speed               7.45 m/s
max body/velocity angle   180 deg
reference clock hold      40.70 s
```

The exponential speed runaway is fixed.

## Root cause now

The runtime still had an old Assisted-only propulsion shortcut:

```text
negative longitudinal demand
    -> symmetric "fore main" thrust
```

So the point-mass velocity could slow, stop and reverse without a hull flip.
The body remained aligned to the route reference, while velocity crossed through
90 degrees and eventually became almost exactly backwards.

This is precisely the visual/physics disconnect the current gate must remove.

## Candidate correction

1. `DynamicMotionSystem::applySystemAccelerationDemand`
   - aft main only for BOTH control laws;
   - reverse demand gets only real bounded RCS until hull orientation allows
     aft main contribution.

2. `buildReferenceAttitudes`
   - Assisted no longer blindly points the nose along velocity;
   - both laws use the physical acceleration vector + RCS authority to decide
     whether the hull must cant/flip for main-engine participation.

3. Regression
   - exact Assisted / Expert / Standard 20.90 -> 20.00 run;
   - execution must complete physically;
   - physical max speed <= 35 m/s;
   - every non-zero main acceleration must have non-negative projection on hull
     forward;
   - final error <= 5 m and final speed within 1.5 m/s.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then viewer:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First reproduce exactly:
`ASSISTED / EXPERT / STANDARD, 20.90 -> 20.00 m/s`.

Watch specifically:
- hull must visibly rotate when substantial braking requires main thrust;
- without sufficient alignment, main engine must not magically brake backwards;
- with hull rotated but main off, velocity must coast except for bounded RCS;
- no velocity reversal caused by hidden fore thrust;
- loop/turn must emerge from actual thrust + attitude, not a point-mass curve.

If this still fails, next layer is the deeper one already implied by the
architecture: Ruckig point-mass trajectory timing must include finite
lead-rotation time before a main-engine-dominant acceleration segment.

Do not enable dynamic avoidance yet.
Do not hide failure by widening envelopes/tolerances.
