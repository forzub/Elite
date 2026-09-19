# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read first:
1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
6. src/game/navigation/STAGE12_END_TO_END.md
7. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md

## Exact failed target-machine checkout

`519313ba430b73b557622436ed7df9a5a9a832cf`.

Architecture PASS.
navigation_runtime 13/14 PASS.
Only maneuver_corner_family_matrix failed.

Primary failure:
expert/Newtonian/StopTurnGo:
- P error 27.257718 m;
- minimum corner speed 2.933115 m/s, so it never achieved the required near-stop;
- center cross-track 21.026823 m;
- rigid hull required half-width 34.330940 m;
- 32 m corridor violation 2.330940 m;
- 40 tracking-envelope exceeded ticks.

Root cause is fixture phase semantics:
all compound phases currently hand off by authored time. This is appropriate for
moving RadiusTurn/DriftTurn internals but invalid for StopTurnGo capture phases.
Newtonian proceeds from flip/brake/rotate/exit before actual waypoint/zero-speed/
attitude capture, so errors accumulate.

Do not widen corridor or weaken thresholds.

RadiusTurn already looks healthy for expert in both laws.
DriftTurn preserves 10 m/s and ~101 deg slip with good P/V and zero corridor
violation, but final attitude is still ~14.38 deg and is expected to become the
next quality issue after StopTurnGo is repaired.

Current printed total times are schedule-authored, not real comparative timing;
Newtonian and Assisted equality is therefore by construction.

## Next implementation

- make StopTurnGo phase handoff terminal-state/capture based;
- keep RadiusTurn/DriftTurn moving handoff schedule/program based;
- compute real common entry -> actual exit/terminal capture elapsed time;
- preserve 32 m corridor and current expert quality thresholds;
- rerun all 18 rows and 9 comparisons.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
