# CONTINUE PROMPT — Elite Navigation: requested/effective control-law state

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationTrace.h/.cpp`
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`.

## Latest bug and fix

User reported ASSISTED button did not work.

Root cause was definite:
the viewer used one `state.controlMode` both for requested selector input and effective
runtime observation.

Old sequence:
```text
old displayed trace = NEWTONIAN
click ASSISTED
 -> state.controlMode = ASSISTED
next frame observes old NEWTONIAN trace
 -> RuntimeControlLawObserved
 -> state.controlMode = NEWTONIAN
```

This made ASSISTED visually and functionally appear dead.

Current fix:
- `state.controlMode` = requested setting only;
- `state.effectiveRuntimeControlLaw` = observed displayed runtime law;
- RuntimeControlLawObserved MUST NEVER assign state.controlMode.

Commit:
- `3067ccfde2f260dcde30baa038ca82e400f233a8`.

Architecture gate:
- `d57cd3b285b0419f9bf8266887fb34811d57a592`.

README:
- `12d91b3253f68907557308b9e27ee4fe6d871822`.

## Right-panel contract

Always show:
- ПИЛОТ
- УПРАВЛЕНИЕ / ВЫБРАНО
- УПРАВЛЕНИЕ / ФАКТ
- ПОВЕДЕНИЕ

If selected inputs are dirty and old execution is still displayed, factual law must be
marked `(СТАРЫЙ РАСЧЕТ)`.

Expected sequence:
1. NEWTONIAN calculated -> selected/fact both Newtonian.
2. click ASSISTED -> selected becomes Assisted immediately; fact remains old Newtonian
   with stale marker.
3. press Calculate -> new execution should produce selected/fact Assisted.

If after fresh Calculate fact is still Newtonian, inspect runtime settings/control-law
propagation below viewer state. Do not change UI again until telemetry proves that.

## Existing contracts that must remain

Calculate:
- no auto recalculation on selector change;
- one click runs required Stage-1/Stage-2;
- disabled/dim afterward until invalidating input changes.

FlightStyle:
- no style-owned speed;
- Standard/Extreme = clearance/risk doctrine only.

Speed:
- START/FINISH = boundary states;
- intermediate speed may vary;
- no braking without local reason.

Control chain:
- accepted program -> Follower -> tracking controller -> runtime bridge -> pilot executor
  -> ShipControlState -> SharedShipPhysics/ShipController/DynamicMotionSystem.

## Secondary unresolved issue

User reports some hull oscillation while returning to reference attitude.

After mode-switch validation is green, diagnose with:
- ideal_ang_cmd;
- exec_ang_cmd;
- pyr_rate;
- reference forward/up;
- forward_ref_deg/up_ref_deg.

Do not blindly increase damping before identifying which layer generates the overshoot.

## Target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Do not claim PASS without target evidence. Do not enable dynamic avoidance yet.
