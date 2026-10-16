# ASSISTED CONTROL — CANONICAL QUICK REFERENCE

Status: **REFERENCE INDEX ONLY — derived from existing project contracts.**
This file does not redefine behavior. If it conflicts with an older source,
the cited source must be reconciled explicitly before production behavior changes.

## 1. Assisted flight-law doctrine

Source: `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

Assisted is direction-coupled:

- velocity approximately follows vehicle forward/nose;
- large persistent slip is suppressed;
- when the pilot changes hull attitude, old lateral VREL is cancelled with priority;
- remaining authority rebuilds/maintains forward speed along the nose;
- current Cobra regression: a 90-degree hull/course change from 100 m/s must bring
  velocity within 5 degrees of the new nose course in <= 3 s;
- velocity must not be rewritten directly and the maneuver must not collapse into
  a full stop.

Operational shorthand:

```text
nose is the steering reference
        ↓
ordinary Assisted FA-on bends velocity toward nose
```

## 2. Assisted physical construction

Sources:
- `src/game/ship/plant/SHIP_PHYSICAL_CONTROL_MODEL_CONTRACT.md`
- `src/game/ship/descriptors/EliteCobraMk1.cpp`

Healthy Assisted-I:

- 2 aft + 2 fore vectoring main engines;
- no dedicated ordinary RCS;
- aft bank accelerates forward;
- fore bank removes forward velocity / accelerates in reverse;
- vectoring of the same main engines supplies lateral translation and attitude authority;
- lateral translation may use opposed fore/aft vectoring.

Cobra descriptor explicit gimbal fields currently say:

- pitch travel: 7 degrees;
- yaw travel: 7 degrees;
- slew: 8 degrees/second.

The descriptor explicitly states these fields are used by Newtonian proving variants
and that Assisted currently ignores this explicit gimbal channel.

Therefore the number `8 degrees` in `AssistedSafeCourseExecutor::MaxLeadRad`
is NOT documented as the Cobra physical nozzle travel limit. It is a separate
algorithmic constant unless/until a physical Assisted-vectoring contract explicitly
links them.

## 3. Safe-course doctrine

Sources:
- `docs/navigation/ASSISTED_FINAL_ACCEPTANCE_2026-10-14.md`
- `docs/navigation/ASSISTED_PREPLANNING_ENTRY_CONTRACT.md`

For an automatic moving entry:

1. release the previous commanded acceleration;
2. inspect the live ballistic velocity course;
3. if that course is blocked, select ONE collision-free Assisted course;
4. turn the hull using ordinary Assisted controls;
5. ordinary FA-on physically bends velocity toward the hull/course;
6. acquire that safe course once;
7. settle pitch/yaw/roll rate to tolerance;
8. continue the accepted moving-entry/planning sequence;
9. do not repeatedly reselect safe course merely because planning takes time;
10. late entry is repaired by the cheap connector/rebind, not a new primary safe-course decision.

The safe-course executor is not a route follower and does not own route geometry,
ETA, planner progress or a time-indexed recovery arc.

## 4. Important distinction: selected course vs execution steering

The SELECTED SAFE COURSE is immutable for one correction unless a genuinely new
hazard invalidates it.

How the hull physically acquires that course is an execution/control question.

Current implementation (introduced in commit
`d321ed7a19f7a467b775ad0df1f4cb00ef6e9778`) uses:

```text
current velocity course
    -> intermediate course <= 8 degrees toward selected course
    -> hull target
    -> FA-on bends velocity
    -> repeat
```

The commit comment says this was introduced to avoid pointing the hull far from
current velocity in one sample and losing kinetic energy.

No existing project document found so far identifies this 8-degree lead as a
physical nozzle/TVC limit.

This implementation detail must therefore be judged against the Assisted doctrine,
not treated as hardware truth.

## 5. Planner ownership

Sources:
- `AUTOPILOT_CONTROL_CONTRACT.md`
- `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`
- `src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`

Planner owns route/corridor/motion feasibility. It does not own manual pitch/yaw,
engine selection or gimbal-axis commands.

Safe-course pre-entry execution must not be debugged by rewriting global route
geometry unless evidence shows the selected safe course itself is wrong.

## 6. Hull attitude control ownership

HullPoseGuidance:
- current hull basis + requested target forward/up
- -> pitch/yaw forward-alignment error + independent roll error.

HullAttitudeControl:
- angular error + angular rate + authority
- -> ordinary bounded pitch/yaw/roll input.

Hard separation:
- pitch/yaw forward alignment must not create roll;
- roll alignment must not create pitch/yaw;
- this layer owns no route selection and no planner geometry.

## 7. What the current investigation has ruled out

The live spin persists while main-propulsion reaction torque and corresponding
main-reaction angular acceleration are zero. The speculative physical
torque-allocation change was reverted.

## 8. Recovery boundaries

- `restore/pre-assisted-final-cleanup-2026-10-16`
  at `8d2062edc38569f1d05afc7c7acbe9275ccc1568`;
- `restore/assisted-final-accepted-2026-10-14`;
- accepted behavior documented in
  `docs/navigation/ASSISTED_FINAL_ACCEPTANCE_2026-10-14.md`.

If the current control chain cannot be reconciled with sections 1–6 without
inventing new semantics, rollback should target the last accepted Assisted restore
rather than an arbitrary intermediate commit.
