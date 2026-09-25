# CONTINUE PROMPT — Elite Navigation v2 / broad manual docking + Assisted default + hard Assisted stop

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four state documents before the next slice. Regenerate
this prompt every iteration.

## Current user acceptance

The latest live screenshot rejects the current docking turn as far too sharp.

Required behavior:
- manual Assisted route must use a much broader visible turn at station scale;
- blocked preferred geometry must reroute/try another ingress first, potentially
  around the entire station;
- only after reroute is exhausted may radius tighten as far as needed;
- nominal tunnel departure warns, but route cancellation must use a much wider
  release envelope and longer grace;
- Assisted is the default local flight law for fresh ship motion;
- Newtonian remains selectable, not removed.

## DockPrep stop truth

With healthy fore/reverse main authority, Assisted BrakeToStop must:
- set target VREL to 0 immediately;
- keep the hull nose-first (no 180-degree flip);
- use the installed reverse/fore main directly at bounded physical authority;
- not wait on the ordinary throttle-response gain.

If reverse main is unavailable, Assisted may rotate and use aft main. Newtonian
retains its explicit hull-alignment/flip semantics.

Current production code already contains the immediate Assisted BrakeToStop
gain and fore-main path, so the live slow-stop report must be validated against
the actual active law. Default law is currently Newtonian and must be changed to
Assisted. Add/retain regression evidence and live law/deceleration diagnostics.

## Automatic docking boundary

Do not implement a visual-frame waypoint autopilot. Full automatic docking still
requires server-owned execution of proved AcceptedManeuverProgram through:
AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics.

START DOCKING stays disabled until that execution owner exists.

## Immediate implementation/gates

Implement broad Assisted route constants, widened release envelope/grace,
Assisted default, and regression/diagnostic coverage.

Then on Windows:
1. `git pull --ff-only origin main`
2. build/run local-flight contract
3. `python tests/architecture_contracts/check_local_flight_control.py`
4. build/run `docking_advisory_tests`
5. `python tests/architecture_contracts/check_manual_docking_advisory.py`
6. `bash build_mingw64.sh`
7. run `build/EliteGame.exe`
8. verify DockPrep reports Assisted and VREL falls rapidly to settle
9. inspect SHOW ROUTE: broad arc and route persistence through larger manual
   tunnel excursions.

Preserve untracked traces/logs. Do not provide patch files; commit directly to
GitHub and give exact pull/test/build/run commands.
