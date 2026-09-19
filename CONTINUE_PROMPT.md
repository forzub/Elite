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

## Accepted baseline

Exact target-machine checkout:
`b5f558b18c8ef8e1b5d9562df36b34cb621c648f`.

Simple execution lab:
- navigation_runtime 11/11 PASS;
- straight 100 m: 0.0285925 m final error, 0.102262 m/s residual,
  zero cross-track/overshoot;
- right-angle 100+100 m: 0.0285925 m final error,
  0.0470123 m max route cross-track, zero 5 m corridor violation.

## Current candidate

Code candidate before docs:
`d0f8b07787339074e225cd02cfe96c93bad0446a`.

New test:
`maneuver_corridor_matrix`.

Route:
P0(0,0,0)
-> P1(0,0,-120)
-> P2(85,35,-190)
-> P3(25,100,-265)
-> P4(120,55,-340).

There are four stop-to-stop translation legs and three bounded 3D attitude
transitions.

Corridor half-width: 5 m.

Matrix:
- Newtonian + expert/competent/rookie;
- Assisted + expert/competent/rookie.

Competent is the exact current NpcAiSystem execution baseline.
Rookie is deterministic and deliberately degraded.

Every row prints completion, final P/V error, route/active-leg cross-track,
corridor violation, waypoint error, overshoot, forward-angle error,
tracking-envelope exceeded ticks and simulated time.

Expert rows are strict.
Competent/rookie are first-pass diagnostic rows so measured envelopes are known
before gameplay limits are pinned.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected CTest count: 12.

Capture all six `[CORRIDOR-MATRIX]` lines.

Do not run the old 120 s obstacle live gate yet.

## Next

Use the matrix to decide B6 execution reserve:
- if expert fails, fix execution first;
- if lower-skill rows deviate, make proof clearance/tempo skill-aware rather
  than weakening the geometry test;
- if both laws diverge, isolate law-specific execution;
- after measured envelopes are pinned, proceed to B6.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
