# CURRENT TASK — target-validate Assisted selection and right-panel effective modes

**Date:** 2026-09-22  
**Status:** ASSISTED UI ROOT CAUSE FIXED / TARGET VALIDATION REQUIRED

## User-visible bug

ASSISTED appeared not to switch.

Root cause was the viewer store:
- user click changed `state.controlMode`;
- current old trace was still NEWTONIAN;
- every render observed that frame and wrote NEWTONIAN back into `state.controlMode`;
- the stale execution therefore undid the requested setting before Calculate.

## Fix

Requested and effective law are now separate.

```text
state.controlMode
  -> user-selected input for next Calculate

state.effectiveRuntimeControlLaw
  -> law actually reported by displayed runtime frame
```

Runtime observation cannot modify requested selector state.

Commit:
- 3067ccfde2f260dcde30baa038ca82e400f233a8

## Right panel

New `ТЕКУЩИЕ РЕЖИМЫ` section:
- ПИЛОТ
- УПРАВЛЕНИЕ / ВЫБРАНО
- УПРАВЛЕНИЕ / ФАКТ
- ПОВЕДЕНИЕ

If settings changed but the old trace is still displayed:
```text
УПРАВЛЕНИЕ / ФАКТ: ... (СТАРЫЙ РАСЧЕТ)
```

Architecture gate now prevents regression where runtime playback overwrites
`state.controlMode`.

## Target validation

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Then verify this exact sequence:

1. Calculate in NEWTONIAN.
2. Right panel should show selected NEWTONIAN and fact NEWTONIAN.
3. Click ASSISTED without calculating.
4. ASSISTED top button and `УПРАВЛЕНИЕ / ВЫБРАНО` must immediately change to Assisted.
5. `УПРАВЛЕНИЕ / ФАКТ` may remain Newtonian but must say `(СТАРЫЙ РАСЧЕТ)`.
6. Calculate.
7. Fresh trace/right panel must show:
   - selected Assisted;
   - fact Assisted.
8. Send new `last_execution_telemetry.log` if fact remains Newtonian; at that point the
   bug is below the viewer state layer and can be isolated from runtime diagnostics.

Also compare pilot and Standard/Extreme changes against the new right-panel fields.

## Secondary issue

There is still modest body oscillation while returning to reference attitude.
Do not tune it yet until Assisted/Newtonian selection is proven correct. Then use
ideal_ang_cmd / exec_ang_cmd / pyr_rate / reference-angle telemetry to identify the
damping layer.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
