# Elite Navigation v2 — continuation prompt

Repo: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read:
1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md
6. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
7. src/game/navigation/STAGE12_END_TO_END.md

Last exact target-machine checkout:
`519313ba430b73b557622436ed7df9a5a9a832cf`.

Result:
- architecture PASS;
- runtime 13/14 PASS;
- only corner-family matrix failed.

Root cause:
StopTurnGo incorrectly advanced compound phases by authored time instead of
actual waypoint/velocity/attitude capture.

New unverified production component:
`ManeuverPhaseGate`.

Modes:
- ScheduledMoving -> advance at nominal horizon;
- StateCapture -> keep tracking terminal sample until Follower::Complete;
- bounded capture timeout -> CaptureTimedOut.

Component commits:
- 5f97b8e52992f537de650e071dab5615fce9fc1f
- c19a260333083f1d6acd472d9d3977b2864193cf

Next:
- wire CMake;
- add isolated ManeuverPhaseGate tests;
- migrate corner fixture;
- keep StopTurnGo state-gated;
- keep Radius/Drift moving phases scheduled;
- measure actual entry->exit timing;
- do not weaken corridor/tolerances;
- rerun target-machine gate.

Current known quality:
- RadiusTurn strong;
- Drift physical execution works but expert exit attitude ~14.38 deg;
- StopTurnGo orchestration is the current blocker.

Always rewrite this prompt and CURRENT_TASK after state-affecting work.
