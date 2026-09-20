# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md` and relevant code/tests.
After every state-affecting event synchronize them and recreate this file from scratch.

## Last verified target checkout

`c8972319390622a6b825bf0fa73e8a563c9d7068`

At that checkout:
- runtime build succeeded;
- 17/19 tests passed;
- planner future-portal fixture failed;
- composite B4 executed two safe adjusted segments, then became nominal-clear,
  then later lost dynamic clearance;
- composite trace writer succeeded and wrote 770 Newtonian frames;
- standalone viewer configure succeeded but compilation failed because GLAD include
  root pointed at `glad/` instead of `glad/include/`.

## Post-run fixes now on main, unverified

1. `tools/navigation_runtime/CMakeLists.txt` uses
   `${ELITE_SOURCE_ROOT}/glad/include`, matching `<glad/gl.h>`.
2. Future-oriented portal fixture is interior and endpoint-safe:
   start X=1, blocker X=4, stage X=7.
3. Global `baseAgent()` remains unchanged at X=0; only that fixture overrides to X=1.

## Viewer

Location: `tools/navigation_runtime/`.
Trace path: `tools/navigation_runtime/last_trace_newtonian.json`.
Viewer displays route, turn points, actual ship path, oriented box + nose arrow,
hazard path/envelopes, selected/reacquisition/portal targets and replans.

## Next command

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_mingw64.sh || true
bash tools/navigation_runtime/run_mingw64.sh
```

Do not lower safety limits to green the test. The main behavioral issue to inspect
is the transition from bounded `NominalClear` to the long scripted portal leg while
the moving hazard remains authoritative.
