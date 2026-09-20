# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read current `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`, and relevant production/tests.
After every state-affecting event synchronize those files and recreate this prompt from scratch.

## Current focus

The latest target run shows B4 can find and physically execute multiple safe
receding-horizon bypass segments, but the composite later loses dynamic clearance
after returning to nominal/topology travel.

## Visualization decision

Build the first visualizer beside the composite test, not in the main game renderer.

Location:
`tests/navigation_runtime/visualizer/`

The composite test should emit a deterministic trace containing at least:
- universe/test time;
- ship position and velocity;
- hazard position and velocity;
- inflated dynamic safety radius/envelope;
- selected B4 target;
- on-route reacquisition reference;
- current portal/topology target;
- replan event/status;
- actual dynamic clearance.

The viewer should render ship path, hazard path/envelope, targets, portal geometry
and replan points for the exact test run. Keep it simple and deterministic.

After the synthetic behavior is understood, the same debug product can be exposed
in the real NAV STRESS in-game overlay.

## Immediate engineering work

1. Repair the oriented-portal fixture geometry.
2. Keep planner/monitor/replan active through resumed topology travel.
3. Add the composite trace + standalone visualizer.
4. Rerun architecture + full runtime gate.

Legacy angular fan/branch mechanisms remain forbidden.
