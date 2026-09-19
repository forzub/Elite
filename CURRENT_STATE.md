# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Target-machine result:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **15/15 PASS**.
- Total runtime test time: ~0.38 s.

This closes the previously failing strict expert DriftTurn corner-family gate.

## Closed defect: DriftTurn exit attitude capture

The root cause was not a generic follower inability to rotate while moving.

The failure came from incorrect exit-authoring semantics:
1. early attempts treated a route checkpoint/time horizon as an attitude deadline;
2. the constant-heading experiment then removed planner-authored large-angle dynamics and incorrectly asked B10's small tracking reserve to execute the whole 90 deg turn;
3. the accepted solution restores planner ownership of the large-angle transition.

Accepted mechanism:
- start from actual yaw after the drift arc;
- start from actual yaw rate after the drift arc;
- target outgoing corridor yaw;
- target terminal yaw rate = 0;
- continue translating at 10 m/s;
- construct a quintic yaw boundary-value profile;
- derive duration from effective physical angular acceleration/rate envelopes;
- reserve B10 authority for residual tracking only.

Expert DriftTurn, Newtonian and Assisted:
- final position error: ~0.111 m;
- final velocity error: ~0.00075 m/s;
- final forward error: ~0.03884 deg;
- tracking-envelope exceeded ticks: 0;
- corridor violation: 0;
- outgoing attitude capture: yes;
- moving capture duration: ~3.12469 s;
- actual starting yaw rate: ~-0.73158 rad/s;
- peak feed-forward yaw rate: ~1.29608 rad/s;
- peak feed-forward yaw accel: ~1.85469 rad/s2.

The long 180 deg angular-tracking diagnostic remains green.

## Architecture conclusion

The planner/follower ownership split is now supported by the corner-family evidence:
- planner authors physically meaningful maneuver/reference dynamics;
- follower tracks with bounded residual authority;
- follower is not used as an implicit maneuver planner.

## Next stage

Proceed to **mixed-angle multi-segment 3D corridor quality**.

The next test must go beyond the existing stop-to-stop orthogonal 3D corridor:
- non-orthogonal segment angles;
- simultaneous X/Y/Z direction changes;
- consecutive turns without full stop where appropriate;
- full rigid-body corridor occupancy;
- Newtonian and Assisted laws;
- expert strict acceptance;
- lower PilotSkill rows diagnostic;
- preserve planner/follower ownership and exact physical envelopes.

Before coding, inspect the existing `ManeuverCorridorMatrixTests.cpp` and choose the smallest extension that exercises real chained 3D maneuver composition rather than another isolated primitive.

## Documentation protocol

After every state-affecting iteration, synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- active Stage-12 document

And recreate `CONTINUE_PROMPT.md` **from scratch** from current truth.
