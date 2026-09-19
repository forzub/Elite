# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest exact target-machine evidence

Tested checkout:

```
be4686f4dcba419f451813b1ddc246088c145e48
```

- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **14/15 PASS**.
- Only failing target: `maneuver_corner_family_matrix`.
- The previous Newtonian StopTurnGo authoring defect is fixed for expert execution:
  - completed=1;
  - final P error 0.369 m;
  - final V error 0.018 m/s;
  - final attitude error 0.030 deg;
  - real near-stop 0.000324 m/s;
  - no corridor violation;
  - no capture timeout.
- RadiusTurn remains healthy.
- Remaining strict expert failure is DriftTurn exit attitude:
  - Newtonian/Assisted final forward error about 10.325 deg;
  - final P/V and corridor remain good;
  - strict common exit requirement remains <=5 deg.
- Rookie StopTurnGo still times out in both laws; this is diagnostic, not the current strict expert failure.

## Interpretation

The ship is allowed to correct attitude while translating. The <=5 deg criterion is a **terminal common-exit requirement**, not a rule that attitude must remain fixed during motion.

The current DriftTurn already requests a moving attitude recovery over 4 s / 40 m. Its remaining ~10 deg terminal error therefore needs to be separated into:

1. a general continuous angular-tracking defect in B9/B10, or
2. a DriftTurn-specific reference/handoff/recovery defect.

Do not weaken the 5 deg gate, widen the corridor, or hide the issue with larger tracking reserve before that split is measured.

## Current unverified candidate

Code candidate:

```
5c16bedc25c422f2c79ae5def14396839ef4ee7c
```

Adds a long-arc diagnostic to `maneuver_corner_family_matrix` without changing the existing corner tolerances or DriftTurn behavior.

Long arc:
- 180 deg sweep;
- radius 80 m;
- speed 10 m/s;
- arc length ~=251.33 m;
- nominal duration ~=25.13 s;
- all 3 PilotSkill profiles;
- Newtonian + Assisted.

Reported continuously over the arc:
- max centerline error;
- max rigid-hull required half-width;
- max forward/tangent angular error;
- tracking-envelope exceed ticks;
- final P/V/attitude errors.

Expert arc gates:
- hull remains inside the same 32 m half-width corridor;
- final P <=1.5 m;
- final V <=1.0 m/s;
- final attitude <=5 deg;
- maximum in-flight forward/tangent error <=10 deg.

This candidate is **not accepted** until target-machine evidence is supplied.

## Next target-machine commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

The next result must be interpreted using both `[CORNER-MATRIX]` and `[LONG-ARC]` rows.

## Architecture invariants

- Planner owns maneuver choice, trajectory/corridor and proof.
- Follower owns sampling/tracking, bounded feedback and safety monitoring; it must not silently choose a different maneuver family.
- AcceptedManeuverProgram remains the planner/follower contract.
- Manual guidance must visualize the same accepted route/trajectory, not run a second planner.
- Newtonian and Assisted physical laws remain distinct.
- Navigation v2 typed frame boundaries remain sealed; planner cannot mutate authoritative physics state.

## Documentation protocol

After every state-affecting iteration, synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- active Stage-12 document

And recreate `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally preserve stale prompt prose.
