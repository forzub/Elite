# CONTINUE PROMPT — Elite Navigation v2 / dual-main propulsion + docking acceptance

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Accepted propulsion truth

Cobra has physical aft/rear and fore/nose longitudinal main-engine banks.
Runtime module state determines effective availability.

Bank health is binary:
- operational bank -> full descriptor authority;
- failed bank -> zero authority;
- never proportionally derate main thrust from health.

RCS is separate. If combined main + RCS would exceed the shared linear
acceleration envelope, preserve selected main thrust and trim only the
secondary RCS vector.

Healthy Assisted uses fore main for strong nose-first braking.
Failed fore -> flip + aft main.
Failed aft + live fore -> fore main becomes primary and working travel direction
becomes `-hullForward`.

Newtonian now follows the same hardware truth all the way through runtime:
- healthy aft main remains the preferred primary;
- if aft main fails and fore survives, ordinary '+' burns fore main along
  `-hullForward`;
- BrakeToStop aligns nose with velocity and burns fore main opposite nose;
- '-' remains a no-op, not a synthetic reverse throttle.

## Verified Windows evidence

On Windows MinGW64, fresh target runs passed:
- `local_flight_control_contracts`;
- `ordinary_physical_maneuver_compiler`;
- `ship_propulsion_state`;
- `docking_advisory`;
- `json_numeric_locale`;
- `check_local_flight_control.py`;
- `check_manual_docking_advisory.py`.

The three navigation-runtime gates were repeated and passed again with
`ninja: no work to do`.

After those passes, one lower runtime mismatch was corrected: Newtonian
DynamicMotionSystem previously still burned only aft main even though
ShipController and B5 could select fore main after aft failure. A native
regression test and static architecture tokens have been added. This newest
change still needs fresh Windows verification.

## Next gate

On Windows `D:\__elite\work`:

1. Pull current `main`.
2. Rebuild `local_flight_control_contract_tests`.
3. Run CTest `local_flight_control_contracts`.
4. Run `python tests/architecture_contracts/check_local_flight_control.py`.
5. If both pass, run canonical `bash build_mingw64.sh`.
6. Launch `build/EliteGame.exe` and perform live manual SHOW ROUTE acceptance:
   takeover -> physical stop -> plan/publish -> visible route/corridor -> hand
   control back to player.
7. Then exercise healthy Assisted behavior and, where practical through debug
   module-state controls, fore-bank and aft-bank failure cases.

Preserve all untracked trace JSON/TXT files. Do not weaken engine truth, invent
fore-engine hit geometry, widen navigation geometry to hide propulsion defects,
or restore retired DockingPathPlanner/GuidanceTunnel.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
