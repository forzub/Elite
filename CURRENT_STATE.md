# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest exact target-machine evidence

Tested checkout:

```
d659416b9b1ddb2356c37eff315f9d13b70bafaa
```

- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **14/15 PASS**.
- Only failing target: `maneuver_corner_family_matrix`.
- Expert Newtonian StopTurnGo remains fixed and healthy.
- RadiusTurn remains healthy.
- Strict expert failure remains DriftTurn common-exit attitude:
  - Newtonian: 10.324757 deg;
  - Assisted: 10.324757 deg;
  - required <=5 deg.
- DriftTurn final P/V and corridor are otherwise good:
  - final P error 0.473 m;
  - final V error 0.032 m/s;
  - no corridor violation;
  - no tracking-envelope exceed ticks for expert.

## Long-arc diagnostic result

The new 180 deg, R=80 m, v=10 m/s long arc ran successfully for every PilotSkill and both control laws.

Expert:
- final P error 0.332 m;
- final V error 0.036 m/s;
- final attitude error 0.052 deg;
- maximum in-flight forward/tangent error 3.221 deg;
- maximum centerline error 0.330 m;
- no tracking-envelope exceed ticks.

Competent:
- final attitude error 0.048 deg;
- maximum in-flight forward/tangent error 4.422 deg;
- no tracking-envelope exceed ticks.

Rookie:
- final attitude error 0.859 deg;
- maximum in-flight forward/tangent error 6.149 deg;
- no tracking-envelope exceed ticks.

Therefore the general angular sampling/tracking chain is healthy. The remaining DriftTurn miss is **not** a general B9/B10 inability to rotate while translating.

## Root cause direction

The ship can correct attitude while moving. The <=5 deg rule is only the common terminal-exit requirement.

The current DriftTurn reference commands its recovery during the final 4 s / 40 m, but the reference ends while the physical ship is still about 10.3 deg short. Since the long arc proves continuous angular tracking works, the remaining defect is local maneuver authoring / recovery construction.

The clean correction is to keep the translational motion and provide an explicit in-motion attitude-settle portion **inside the same accepted maneuver program**. This is not replanning and does not require the follower to invent a maneuver.

Do not relax the 5 deg gate, widen the corridor, or increase generic tracking reserve to hide this.

## Current unverified candidate

Code candidate:

```
76346121516e5b00d14a4e6304621b55791093ab
```

DriftTurn recovery now remains one coherent 4 s / 40 m accepted moving reference, but:
- smooth 90 deg recovery completes in 2.5 s;
- final 1.5 s continues translating at 10 m/s while holding the exit yaw;
- no separate follower maneuver is introduced;
- no tolerance, corridor or generic tracking reserve was changed.

This candidate is not accepted until target-machine evidence is supplied.

## Next target-machine commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Acceptance requires the existing expert DriftTurn <=5 deg exit attitude plus unchanged P/V/corridor quality and a still-green long-arc diagnostic.

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
