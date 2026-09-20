# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

## Mandatory workflow

Read current `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`, and relevant production/tests.
After every state-affecting event synchronize those files and recreate this
`CONTINUE_PROMPT.md` from scratch.

## Latest evidence

Latest uploaded target log: `navigation_test_20260920-182430.txt`.
The log itself does not contain a tested HEAD line; do not invent one.

Result:
- Stage-12 architecture PASS;
- build/link PASS;
- 17/19 runtime PASS;
- planner fixture FAIL: future oriented portal route context not retained;
- composite FAIL: dynamic clearance lost for Newtonian.

## B4 interpretation

B4 is now clearly producing useful behavior in the composite:
- first `AdjustedClear`: lateral offset 29.381694 m, forward station 22.5 m;
- first physical replacement actual dynamic clearance 4.747075 m;
- zero tracking exceeded ticks;
- next `AdjustedClear` continuation actual dynamic clearance 22.820979 m;
- zero tracking exceeded ticks;
- next solve becomes `NominalClear`.

The failure is after this sequence. The test then exits the local replan loop and
runs a fixed long narrow-portal phase while the hazard is still active. That violates
the required command-continuous/receding-horizon navigation model.

Correct rule:
```text
safe short segment -> execute
next world update -> monitor/replan
safe short segment -> execute
...
nominal short segment becomes clear -> continue monitoring/replanning while reacquiring
no safe executable segment -> brake, navigation stays active
```

`NominalClear` for one bounded segment is NOT authority to execute an unmonitored
10-second/topology-wide segment.

## Planner fixture

The previous repair put the agent at X=0, on the region-1 minimum X boundary.
The failure now occurs on route-context retention before the bypass assertion.
Repair the fixture with an interior start and valid dynamic separation; candidate:
- start X=1;
- blocker X=4;
- staging X=7;
- required dynamic separation = 2.75 m;
- both endpoint distances = 3.0 m.

Do not weaken radius/padding/safety rules.

## Next engineering work

1. Fix the oriented-portal fixture geometry only.
2. Keep planner/monitor/replan active through resumed topology travel.
3. Add a visual trace/export: ship path, hazard path, inflated hazard envelope,
   bypass targets, reacquisition references, portal center, and replan points.
4. Rerun architecture + full runtime gate.

Legacy angular fan/branch mechanisms remain forbidden.
