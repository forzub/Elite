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

## Current next task

Change DriftTurn recovery so its accepted reference reaches the target attitude early enough and holds/settles that attitude while the ship continues moving at the intended exit velocity. Prefer one coherent program over a separate follower-side corrective maneuver.

After the code change, rerun the same corner-family and long-arc gate.

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
