# CONTINUE PROMPT — Elite Navigation v2 / verify broad Assisted docking, then Automatic execution

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four state documents before the next slice. Regenerate
this prompt every iteration.

## Implemented source truth

Latest user live screenshot rejected the previous docking bend as far too sharp.

Manual Assisted commissioning source now uses:
- final docking-axis lead = 9000 m;
- terminal fillet segment fraction = 0.85;
- preferred terminal radius = 6000 m.

The radius is a preference, not a route failure threshold. Planner order remains:
1. nominal geometry at preferred radius;
2. 12 alternate pre-alignment ingress directions with full obstacle search;
3. expanded-clearance/full-obstacle reroute;
4. only then tighten radius as much as collision-free geometry requires.

Manual corridor lifetime:
- nominal open-flight tolerance remains 60 m and still drives HUD warning;
- release = nominal + max(100% nominal, 30 m), normally 120 m;
- sustained outside-release grace = 1.00 s;
- longitudinal release uses the same 100% / 30 m expansion.

Fresh local flight now defaults to Assisted in DynamicMotionState,
ShipControlState and client pending-law/reset state. Newtonian remains selectable.

## DockPrep stop contract

Assisted BrakeToStop:
- sets target VREL to 0 immediately;
- bypasses ordinary target-speed/throttle response using the fixed-step
  stop gain;
- with a healthy fore/reverse main, commands full bounded reverse-main
  acceleration without a 180-degree hull flip;
- falls back to hull rotation/aft main only when reverse main is unavailable.

Server DockPrep begin diagnostic now prints:
`law=ASSISTED|NEWTONIAN forward_main_mps2=... reverse_main_mps2=...`.

Native regression locks Assisted default, immediate zero target and
full-authority healthy reverse-main braking.

## Target gate — still pending

On Windows `D:\__elite\work`:
1. `git pull --ff-only origin main`
2. rebuild `local_flight_control_contract_tests`
3. run CTest `local_flight_control_contracts`
4. run `python tests/architecture_contracts/check_local_flight_control.py`
5. rebuild `docking_advisory_tests`
6. run CTest `docking_advisory`
7. run `python tests/architecture_contracts/check_manual_docking_advisory.py`
8. `bash build_mingw64.sh`
9. run `build/EliteGame.exe`

Live acceptance:
- fresh/default law is Assisted;
- DockPrep begin shows ASSISTED and healthy reverse-main authority;
- VREL falls rapidly to settle;
- SHOW ROUTE logs preferred 6000 m radius unless `radius_relaxed=1`;
- visible station curve is broad;
- nominal tunnel excursions no longer delete guidance prematurely.

If live braking is still slow despite law=ASSISTED and healthy reverse main,
capture VREL/applied acceleration; do not fake velocity or weaken physics.

## Automatic docking boundary

After the above gate, continue the real server-owned executor:
AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics.
Do not chase visual DockingAdvisoryGate frames. START DOCKING remains disabled
until that execution owner exists.

Preserve untracked traces/logs. Commit directly to GitHub; do not provide patch
files.
