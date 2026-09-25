# CONTINUE PROMPT — Elite Navigation v2 / verify docking through canonical script

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four documents and regenerate this prompt.

## Current source truth

Manual Assisted:
- preferred final-axis lead = 9000 m;
- preferred terminal radius = 6000 m;
- terminal fillet fraction = 0.85;
- default local flight law = Assisted;
- nominal open-transit corridor = 60 m;
- release corridor = 120 m;
- outside-release grace = 1.00 s.

Final-axis semantics:
- hard mandatory ingress = max(700 m, 3 * standoff);
- far 9000 m lead is a soft preference;
- obstruction of the soft lead shortens it and continues route search;
- only close-in ingress may hard-fail as
  `dock mandatory ingress blocked`;
- retired `dock alignment blocked` must not return.

## Verification tooling truth

Do not call `docking_advisory_tests` from canonical `build/`.

Root `verify_docking.sh` is now the canonical focused gate. It:
1. configures `tests/navigation_runtime` into
   `build/tests/navigation_runtime`;
2. builds `docking_advisory_tests`;
3. runs CTest `docking_advisory`;
4. runs the static manual-docking contract.

The corrected static checker prints revision
`20260925-soft-axis-v2` on PASS.

## Immediate Windows gate

From `D:\__elite\work`:

```bash
git pull --ff-only origin main
git log -1 --oneline
bash verify_docking.sh
```

If PASS:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Live acceptance:
- route calculation returns;
- far preferred-axis obstruction may set
  `final_axis_shortened=1`;
- retired `dock alignment blocked` does not appear;
- broad curve remains usable or explicit radius relaxation is logged;
- default law is ASSISTED and DockPrep braking is rapid.

Automatic docking executor remains next after this gate:
AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics.

Preserve untracked traces/logs. Commit directly to GitHub; do not provide patch
files.
