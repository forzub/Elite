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

Canonical flow:
B0..B8 planner -> AcceptedManeuverProgram.
B9 sampler -> B10 bounded tracking -> B11 safety/reflex -> B12 PilotSkill -> B13 physics.
B14 schedules planner work.

Scale target remains hundreds/thousands of registered units:
- no dense N x N;
- shared world/influence work;
- only dirty actors enter planner;
- active controlled actors run cheap B9/B10/B12/B13;
- planner work is queued, budgeted and revision-checked.

Last fully verified migration baseline:
`701881ddae861cd5593e425de91600e048bd417c`.
B8/B9 accepted.

B10 first gate failed because:
- architecture checker matched comment prose;
- MinGW rejected braced default reference.

Corrective code/contract baseline before docs:
`4a3d196c1574e91b747f05d194db7a35ad5c5517`.

Corrections:
- explicit overloads;
- precise dependency contract;
- MinGW-safe overload lock;
- timing script prints even on failed phase.

Fresh target-machine evidence now shows:
- EliteGame BUILD PASS;
- EliteServer BUILD PASS;
- build_mingw64 real_s=44.989.

So production compile/link is green.

Still missing from supplied evidence:
- architecture contract PASS;
- navigation_runtime 8/8 PASS;
- target-machine git rev-parse HEAD.

Run only:

```bash
cd /d/__elite/work

git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Do not rebuild production again unless these reveal a new change.

Do not accept B10 until both missing gates are green.

After B10 acceptance:
implement B14 Navigation Work Scheduler with urgent/normal/background queues,
stale-job rejection, bounded per-slice work, fairness and synthetic 100s/1000s actor tests.

Every state-affecting iteration:
- rewrite this file;
- rewrite CURRENT_TASK.md;
- update CURRENT_STATE.md / PROJECT_STATE.md / STAGE12_END_TO_END.md;
- update architecture/migration docs when ownership changes.
