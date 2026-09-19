# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv
**Branch:** main

## Last target-machine evidence

Exact tested checkout:
`519313ba430b73b557622436ed7df9a5a9a832cf`.

Architecture PASS.
navigation_runtime 13/14 PASS.
Only `maneuver_corner_family_matrix` failed.

## Root cause

StopTurnGo used time-only compound phase handoff. That is wrong for a maneuver
that must actually capture waypoint / near-zero velocity / attitude before the
next phase.

Do not widen the 32 m corridor or weaken quality thresholds.

## New production candidate

Added:
- `src/game/navigation/ManeuverPhaseGate.h`
- `src/game/navigation/ManeuverPhaseGate.cpp`

Semantics:
- `ScheduledMoving`: advance when nominal reference horizon ends;
- `StateCapture`: continue tracking terminal sample until
  `TrajectoryFollower::Complete`;
- bounded capture overrun;
- explicit `CaptureTimedOut` if capture never happens.

Current component commits:
- API `5f97b8e52992f537de650e071dab5615fce9fc1f`
- implementation `c19a260333083f1d6acd472d9d3977b2864193cf`

This is unverified on target machine.

## Next

1. Wire ManeuverPhaseGate into shared runtime CMake and isolated runtime test.
2. Add deterministic tests for ScheduledMoving / StateCapture / timeout.
3. Migrate ManeuverCornerFamilyMatrix:
   - StopTurnGo brake/capture + attitude capture => StateCapture;
   - RadiusTurn/DriftTurn internal moving phases => ScheduledMoving.
4. Measure actual elapsed entry -> terminal/exit time.
5. Keep current corridor and expert thresholds unchanged.
6. Re-run runtime gate.
7. Then repair drift exit attitude if still >5 deg.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update migration/architecture docs when contracts change.
