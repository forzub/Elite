# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The recreated prompt must contain this same rule again.

## Accepted target-machine baseline

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Target result:
- Stage-12 architecture contract PASS;
- navigation_runtime 15/15 PASS;
- total runtime test time ~0.38 s.

The previously failing DriftTurn corner-family gate is closed.

Expert DriftTurn Newtonian/Assisted:
- final P ~0.111 m;
- final V ~0.00075 m/s;
- final forward error ~0.03884 deg;
- tracking-envelope exceeded ticks 0;
- corridor violation 0;
- outgoing attitude captured;
- capture duration ~3.12469 s;
- actual capture start yaw rate ~-0.73158 rad/s;
- peak yaw-rate feed-forward ~1.29608 rad/s;
- peak yaw-accel feed-forward ~1.85469 rad/s2.

## Closed root cause

The defect was planner-side exit attitude authoring, not generic follower angular tracking.

Correct ownership:
- planner authors the physical large-angle transition;
- transition starts from actual/planned angular state;
- duration derives from physical angular acceleration/rate limits;
- B10 retains bounded residual tracking authority only.

Do not regress to:
- fixed arbitrary route-time attitude deadlines;
- raw large target-heading steps executed by B10;
- inflated generic feedback reserve.

## Current task

Proceed to **mixed-angle multi-segment 3D corridor quality**.

Inspect the existing `tests/navigation_runtime/ManeuverCorridorMatrixTests.cpp` first.

The new coverage must go beyond its current axis-aligned stop-to-stop 3D legs:
- 3-4 connected segments;
- non-orthogonal directions;
- at least one direction with simultaneous X/Y/Z components;
- mixed turn angles;
- preferably chained motion without a full stop at every turn where physically appropriate;
- continuous rigid-body corridor occupancy;
- final P/V/attitude metrics;
- tracking-envelope exceed count;
- Newtonian + Assisted;
- expert strict acceptance;
- competent/rookie diagnostic.

Keep the same B9/B10 -> PilotSkill -> real physics execution path.

## After this stage

Then proceed to speed/doctrine coverage, reconciling current contract names `Rational / Precision / Extreme / CombatEscape` with any older “Freestyle / Extreme” terminology before adding tests.

**Again:** recreate this prompt from scratch after every state-affecting iteration.
