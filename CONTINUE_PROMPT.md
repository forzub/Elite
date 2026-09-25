# CONTINUE PROMPT — Elite Navigation v2 / rerun final-axis fix with correct standalone test build

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four documents and regenerate this prompt.

## Current source truth

Manual Assisted profile:
- preferred final-axis lead = 9000 m;
- preferred terminal radius = 6000 m;
- terminal fillet fraction = 0.85;
- fresh flight-law default = Assisted;
- open-transit nominal corridor = 60 m;
- release corridor = 120 m;
- outside-release grace = 1.00 s.

Final-axis semantics:
- hard mandatory ingress = max(700 m, 3 * standoff);
- long 9000 m lead is soft;
- if its far part is obstructed, Planner probes `clear(align,stop)`, finds the
  longest clear prefix, sets `terminalApproachShortened=true`, and continues;
- only the close-in ingress may fail as `dock mandatory ingress blocked`;
- retired `dock alignment blocked` must not exist.

## Latest verification result

The attempted native command was invalid:
`cmake --build build --target docking_advisory_tests`
returned `unknown target`, and `ctest --test-dir build` found no tests.

Reason: navigation runtime tests are a standalone CMake project at
`tests/navigation_runtime`. Their build tree is
`build/tests/navigation_runtime`.

The static manual-docking checker also false-positive matched any
`if (!clear(align,stop))`. It is corrected to reject only the retired
immediate hard-failure path and to require soft shortening + dedicated mandatory
ingress failure.

## Immediate Windows gate

From `D:\__elite\work`:

```bash
git pull --ff-only origin main

cmake -S tests/navigation_runtime \
  -B build/tests/navigation_runtime \
  -G Ninja

cmake --build build/tests/navigation_runtime \
  --target docking_advisory_tests \
  -j 8

ctest --test-dir build/tests/navigation_runtime \
  -R "^docking_advisory$" \
  --output-on-failure

python tests/architecture_contracts/check_manual_docking_advisory.py
```

If both pass:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Live acceptance:
- route calculation returns;
- far obstruction may produce `final_axis_shortened=1`, never retired
  `dock alignment blocked`;
- only true close-in obstruction may report
  `dock mandatory ingress blocked`;
- broad curve remains usable or radius relaxation is explicit;
- fresh law is ASSISTED and DockPrep stop is rapid.

Automatic docking executor remains the next implementation milestone after this
gate:
AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics.

Preserve untracked trace/log files. Commit directly to GitHub; do not provide
patch files.
