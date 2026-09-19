# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.
Target: Windows 10 / MSYS2 MinGW64 / g++ 15.2 / CMake + Ninja.

Read first:
1. `CURRENT_TASK.md`
2. `CURRENT_STATE.md`
3. `PROJECT_STATE.md`
4. `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
5. `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
6. `src/game/navigation/STAGE12_END_TO_END.md`

Canonical structure:
Planner B0..B8 -> AcceptedManeuverProgram.
Follower B9 sampler -> B10 bounded tracker -> B11 safety -> B12 PilotSkill -> B13 physics.
B14 schedules planner jobs.

Scaling remains mandatory:
- shared world/influence work;
- no dense N x N;
- only dirty actors enter planner;
- cheap follower execution for active controlled actors;
- queued/budgeted planning for hundreds/thousands of registered units.

Verified target-machine baseline:
`701881ddae861cd5593e425de91600e048bd417c`.
B8/B9 accepted.

Current B10 corrective candidate before docs:
`4a3d196c1574e91b747f05d194db7a35ad5c5517`.

The first B10 gate failed:
- architecture lock falsely matched the word NavigationMap in a comment;
- MinGW/g++ 15.2 rejected `const Policy& policy = {}`;
- production build failed on same header after 17.209 s;
- no B10 tests reached execution.

Fixes:
- explicit overloads instead of braced default reference;
- precise dependency contract;
- contract pins MinGW-safe overloads;
- navigation_runtime timing prints even on failed phase.

Run now:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

TIMEFORMAT='[TIMING] build_mingw64 real_s=%R user_s=%U sys_s=%S'
time bash build_mingw64.sh
```

Expected navigation_runtime: 8/8, including maneuver_tracking_controller.

No log file is required for this rerun. If a later diagnostic creates one, print its exact path at the end.

Do not accept B10 without fresh target-machine evidence.

After B10 PASS, implement B14 clean scheduler API before live AcceptedManeuverProgram migration.

Every state-affecting iteration:
- rewrite this file;
- rewrite CURRENT_TASK.md;
- update CURRENT_STATE.md / PROJECT_STATE.md / STAGE12_END_TO_END.md;
- update architecture/migration docs when ownership changes.
