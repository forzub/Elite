# CURRENT TASK — Architecture cleanup gate before further navigation tuning

Date: 2026-09-22

Status: **AUDIT COMPLETE / IMPLEMENT CONTINUOUS PROGRAM SEMANTICS NEXT**

Baseline entering audit:

```text
5117857c8f31992c97f393e7f2ed16a630e8efc3
```

## Fresh target result

Dense-source preservation is confirmed:

```text
10 m/s:
    1333 trajectory samples
    1332/1332 actuator coverage COMPLETE
    90 Program objects
    0 handoffs
    reference hold 56.62 s
    final speed 0

27.8 -> 26:
    613 trajectory samples
    612/612 actuator coverage COMPLETE
    42 Program objects
    0 handoffs
    reference hold 42.22 s
```

Therefore source sampling is no longer the primary failure.

## Immediate root cause

The accepted attitude stream is not angular-acceleration feasible.

At the start of the 27.8 m/s run the reference forward changes ~1.4323 degrees
in 0.00833 s, i.e. ~3 rad/s immediately from omega=0.

Current authoring clamps only:

```text
delta orientation <= maxAngularRate * dt
```

It does not enforce `angularAccel`.

Follower immediately exceeds its 0.8 rad/s angular-rate-error envelope.

Runtime then freezes program time whenever tracking is outside the envelope:

```text
activeProgramReferenceDelaySeconds += dt
programReferenceTime = wallTime - delay
```

The phase gate receives the same frozen time, so Program 0 never ends.
Recovery then homes toward a stale early reference, explaining the stop and
return behavior.

## Architecture correction required

### 1. One continuous ManeuverProgram clock

Fixed-capacity <=16-sample chunks are storage pages, not maneuver phases.

Page transitions must not:
- reset acceptedAt time;
- invoke capture semantics;
- reset reference timing;
- become a replanning event.

### 2. No indefinite stale-reference homing

For FreeTransit, material tracking loss means the accepted maneuver is no
longer proved from the current state.

Allowed:
- short bounded safety/recovery action;
- invalidate accepted program;
- replan from measured P/V/q/omega.

Forbidden:
- freeze one moving reference forever and drive back to it.

### 3. Angular reachability

Planner attitude construction must integrate:
- current angular velocity;
- angular acceleration limit;
- angular speed limit;
- required final attitude/rate.

No sample may require an instantaneous omega jump.

### 4. One vehicle-dynamics source of truth

Create one profile derived from authoritative ship descriptor.

Remove/reconcile:
- runtime hard-coded `cobraParams()`;
- fake symmetric braking/reverse scalar capability;
- stale `CapabilitySnapshot.maxReverse...` for aft-only Cobra;
- manual Assisted virtual fore-main semantics;
- navigation path ignoring throttle slew.

## What is trusted right now

Trusted only within stated scope:
- static geometry/collision queries;
- Stage-1 coarse route topology for the current fixture;
- scalar Ruckig numerical solve for the constraints it is given;
- Newtonian low-level aft-main + bounded-RCS physical allocator;
- deterministic sampling/frame conversion.

Not accepted as physical maneuver truth:
- current Stage-2 execution guide;
- current attitude author;
- current program phase/page orchestration;
- current reacquisition freeze;
- current vehicle capability snapshots.

## Implementation order

1. Introduce continuous program/global time semantics across storage pages.
2. Replace page-gated execution with transparent page indexing.
3. Replace indefinite reference hold with bounded invalidation/replan semantics.
4. Make attitude author angular-acceleration feasible.
5. Introduce authoritative VehicleDynamicsProfile and eliminate duplicates.
6. Rebuild physical maneuver compiler around that profile.
7. Then wire Planner ActuatorSegments directly into Autopilot.

## Target gate

Build:

```bash
cd /d/__elite/work
git pull --ff-only
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then run the E2E that Stage-1 script does NOT run:

```bash
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Current code is expected to fail this E2E. Do not treat a viewer build PASS as
navigation acceptance.

Dynamic avoidance remains disabled.
