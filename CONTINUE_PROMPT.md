# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read first:
1. `CURRENT_TASK.md`
2. `CURRENT_STATE.md`
3. `PROJECT_STATE.md`
4. `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
5. `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
6. `src/game/navigation/STAGE12_END_TO_END.md`
7. `src/game/navigation/NAVIGATION_PURITY_CONTRACT.md`

## Current evidence

Exact target-machine checkout:
`a69771e3efb5b54834b002a5000b79d75a8f5e80`.

On it:
- architecture PASS 0.211 s;
- navigation_runtime 9/9 PASS;
- B14 scheduler 5000 jobs total 3314 us;
- client/server build PASS 45.371 s;
- live ordered navigation FAIL rc=56, log:
  `D:\__elite\work\build\logs\navigation_live_scheduler_20260919-221240.log`.

The live failure is the old pre-B14 ordinary-maneuver defect:
geometric AdjustedClear is found and replicated but is not converted into a
physically executable ordinary maneuver for anisotropic Newtonian propulsion.

Do not blame/rollback B14 and do not weaken the live self-test.

## Current implementation

First isolated B5 candidate before docs:
`233d4023e81d5d466043a08c67acd8d49846c4b5`.

New `OrdinaryPhysicalManeuverCompiler`:
- strict pure;
- fixed-capacity;
- Newtonian first;
- Coast / Trim / LeadRotateMainBurn;
- directional linear authority;
- angular acceleration/speed authority;
- B10 feedback reserve;
- control response reserve;
- every result requires B6 proof;
- no world query / planner call / clock / vector.

Assisted returns UnsupportedControlLaw intentionally.

Regression includes live 75-degree failure class and 10,000 compile timing.

## Run now

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

Expected navigation_runtime: 10/10 including
`ordinary_physical_maneuver_compiler`.

Do not rerun long live self-test in this slice; B5 is isolated only.

## Next after green

Implement B6 proof around the exact B5 candidate, then integrate
B4 -> B5 -> B6 -> B7/B8 and rerun the logged live ordered-flight gate.

Every state-affecting iteration must rewrite this file and CURRENT_TASK and
update CURRENT_STATE / PROJECT_STATE / STAGE12_END_TO_END.
