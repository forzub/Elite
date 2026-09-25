# CONTINUE PROMPT — Elite Navigation v2 / verify final-axis shortening regression fix

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four documents and regenerate this prompt.

## Latest live failure

After increasing manual Assisted geometry to a 9 km final lead / 6 km preferred
turn radius, live SHOW ROUTE stopped producing routes:

`[DockAdvisory] request=N failed=dock alignment blocked`

Root cause: DockingAdvisoryPlanner treated the entire preferred 9000 m final
axis as hard semantic geometry and rejected it with `clear(align, stop)`
before any geometric reroute search.

## Current source fix

Final-axis semantics are split:
- hard mandatory ingress = `max(700 m, 3 * standoff)`;
- preferred manual-Assisted lead = requested 9000 m.

Only mandatory ingress can produce
`dock mandatory ingress blocked`.

If the far preferred lead is obstructed:
- binary-search the longest clear prefix of the exact docking axis;
- set `terminalApproachShortened=true`;
- continue normal route planning;
- try preferred 6000 m turn radius;
- reroute/alternate ingress before tightening radius.

New route log:
`route=... final_axis_m=... final_axis_shortened=0|1 terminal_radius_m=... radius_relaxed=0|1`.

Regression cases:
1. far blocker on preferred 9 km axis -> valid shortened route;
2. blocker in near mandatory ingress -> hard failure.

## Other current commissioning truth

Fresh local flight defaults to Assisted.
Manual corridor release is widened to 120 m in ordinary transit with 1.00 s
outside-release grace.
Assisted DockPrep sets target VREL=0 immediately and should use full healthy
reverse/fore main authority without a hull flip.
Automatic docking executor is still not complete; START DOCKING stays disabled.

## Immediate Windows gate

From `D:\__elite\work`:

1. `git pull --ff-only origin main`
2. rebuild/run `docking_advisory_tests`
3. CTest `docking_advisory`
4. `python tests/architecture_contracts/check_manual_docking_advisory.py`
5. if green, rebuild/run local-flight contract/static check as needed
6. `bash build_mingw64.sh`
7. run `build/EliteGame.exe`
8. SHOW ROUTE and capture new route/final-axis log plus DockPrep begin/settled.

Live acceptance:
- route calculation returns;
- no `dock alignment blocked` for far preferred-axis obstruction;
- shortening is visible in diagnostics when required;
- broad curve remains usable or radius relaxation is explicit rather than
  silent task cancellation;
- fresh law is ASSISTED and stop is rapid.

Preserve untracked traces/logs. Commit directly to GitHub; do not provide patch
files.
