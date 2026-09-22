# CONTINUE PROMPT — Elite Navigation: repair continuous program semantics before more tuning

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this `CONTINUE_PROMPT.md` from scratch.

Keep `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md` synchronized while the
audit is active. Update `CONTROL_LAW_MANEUVER_MODEL.md` when vehicle/control
ownership changes.

## Latest target evidence

Dense source preservation succeeded but runtime execution regressed:

```text
10 -> 10 Newtonian
trajectory samples: 1333
programs: 90
actuator coverage: 1332/1332 COMPLETE
phase handoffs: 0
reference hold: 56.62 s
final speed: 0
final error: 270.27 m

27.8 -> 26 Newtonian
trajectory samples: 613
programs: 42
actuator coverage: 612/612 COMPLETE
phase handoffs: 0
reference hold: 42.22 s
final speed: 14.21 m/s
final error: 148.72 m
```

## Immediate root cause

At t=0.00833 in the 27.8 m/s run, reference forward is already ~1.4323 deg
away from the initial forward. That is ~3 rad/s reference angular velocity from
an omega=0 start.

`buildReferenceAttitudes()` limits orientation step by max angular RATE but
does not enforce max angular ACCELERATION.

Tracking envelope angular-rate tolerance is 0.8 rad/s, so the follower enters
EnvelopeExceeded almost immediately.

Runtime then freezes reference time:

```text
activeProgramReferenceDelaySeconds += dt
programReferenceTime = wallTime - delay
```

ManeuverPhaseGate receives the same frozen time, so page 0 never reaches its
nominal end. B10 outside-envelope behavior removes moving feed-forward and
homes toward the frozen early reference. This causes the visible stop and
return.

## Critical representation correction

The 42/90 <=16-sample objects are STORAGE PAGES, not physical maneuver phases.

Required:

```text
one continuous ManeuverProgram
one monotonic global program clock
many fixed-capacity pages if needed
page crossing is transparent indexing
```

Do NOT reactivate/reset accepted time or apply phase-capture semantics at a
storage page boundary.

## Tracking-loss semantics

Do not freeze a FreeTransit moving reference indefinitely.

When the accepted program is materially unreachable:
- Autopilot may perform bounded immediate safety action;
- invalidate the accepted program;
- Planner replans from measured current P/V/q/omega.

Do not command return to an old trajectory point for tens of seconds.

## Vehicle dynamics SSOT audit

There is currently no single truth:
- authoritative EliteCobraMk1Descriptor;
- duplicated runtime cobraParams();
- simplified NavigationVehicleProfile;
- AcceptedManeuverProgram CapabilitySnapshot;
- manual Assisted propulsion semantics.

Create one authoritative VehicleDynamicsProfile derived from the ship descriptor.

It must include installed thruster topology, throttle slew, RCS authority,
angular rate/accel, speed/load envelope, hull geometry and required inertia.

Current Cobra has aft main only. Remove any fake symmetric fore-main truth.

## Component verdict

Relatively trusted in isolation:
- static geometry/HitVolume query;
- Stage-1 coarse topology for current fixture;
- scalar Ruckig solver for its exact 1-D request;
- Newtonian aft-main + bounded-RCS low-level allocator;
- deterministic sampler/frame transforms.

Not accepted as physical maneuver truth:
- current execution-guide physics;
- current attitude author;
- page/phase orchestration;
- reference-clock hold;
- duplicated capability descriptions.

## Implementation order

1. Continuous global program time + transparent storage pages.
2. Remove indefinite reference-clock freeze; invalidate/replan on material
   tracking loss.
3. Angular-acceleration-feasible attitude authoring.
4. VehicleDynamicsProfile SSOT.
5. Planner physical geometry/attitude/main+RCS/throttle proof.
6. Ruckig used inside already-feasible maneuver construction.
7. Direct ActuatorSegment execution by Autopilot.

Do not tune follower gains to hide these defects.
Do not enable dynamic avoidance.

## Correct target gate

The Stage-1 helper does NOT run Stage-2 E2E.

Run:

```bash
cd /d/__elite/work
git pull --ff-only
bash tests/navigation_runtime/run_stage1_mingw64.sh
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Until `navigation_runtime_pipeline` passes on the target machine, do not claim
the navigation execution chain is accepted.
