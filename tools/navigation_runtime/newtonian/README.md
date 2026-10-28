# Newtonian pilot diagnostics (DEV)

Run from the MSYS2 MinGW64 shell. Transient outputs go to `$ELITE_DIAG_DIR`
(default `D:/tmp`, files prefixed `elite_`) and are disposable; anything worth
keeping belongs in this directory or in `PROJECT_STATE.md`.

| script | purpose |
|---|---|
| `unit.sh` | build + run `tests/navigation_runtime/NewtonianOptimalControllerTests.cpp` |
| `replay_matrix.sh TAG [build]` | offline regression: production route autopilot + production ShipCore step (`EliteNewtonianReplay`), cases I (delay 1/3/6) and Y/X (delay 3) |
| `replay.sh CASE TAG [MODEL] [SECONDS] [DELAY]` | one offline replay |
| `fly.sh MODEL SECONDS TAG` | one real game flight (`nav-stress:simple`) with the decision trace and a case dump |
| `series.sh TAG SECONDS MODEL...` | game build + several flights + summaries |
| `decision.py TAG` | acceptance summary of a decision trace (corridor, lateral velocity, target jumps, reversals, phases) |
| `window.py TAG T0 T1 [STEP]` | decision rows and events in a time window |
| `terminal.py TAG` | terminal sequence report (steps, distance, speed, main, RCS, attitude/roll errors, angular rate) |

`cases/newtonian-{I,Y,X}.case`: accepted Step 1a routes dumped by the game
(`ELITE_AUTOPILOT_CASE_DUMP`) and replayed bit-exactly. A new case from a
flight is `$ELITE_DIAG_DIR/elite_TAG_case.txt`.

DEV environment variables read by the game/pilot:
`ELITE_NEWTONIAN_POLICY_TRACE`, `ELITE_NEWTONIAN_EVENT_TRACE`,
`ELITE_AUTOPILOT_CASE_DUMP`, `ELITE_ROUTE_DUMP`, `ELITE_SHIP_PHYSICAL_TEST_MODEL`.
