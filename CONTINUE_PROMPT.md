# CONTINUE PROMPT — Elite Navigation v2 / dual-main propulsion + docking acceptance

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Propulsion contract

Cobra has physical aft/rear and fore/nose longitudinal main-engine banks.
Current availability comes from runtime module state.

Main-bank damage is binary:
- operational -> full descriptor authority;
- failed -> zero;
- never proportionally derate main thrust from module health.

RCS is separate. If total main + RCS acceleration would exceed the shared
linear envelope, preserve the selected main command and trim only the secondary
RCS vector.

Healthy Assisted uses fore main for strong nose-first braking. Failed fore bank
falls back to flip + aft main. Failed aft bank with live fore bank may reverse
working travel direction and use fore main as primary. B5 distinguishes
explicit forward/reverse MAIN authority from body-axis/RCS authority.

## Latest Windows evidence

Latest target run:
- `check_local_flight_control.py`: PASS;
- `check_manual_docking_advisory.py`: PASS;
- `json_numeric_locale`: PASS;
- updated `local_flight_control_contract_tests` FAILED TO BUILD because the
  test referenced `game::ship::mainAccelerationLimitMps2()` without including
  `src/game/ship/core/ShipDynamics.h`.

After Ninja failed, CTest executed the old local-flight executable left in the
build tree and repeated:
`Assisted controller exceeded ship maxGs envelope`.

That repeated failure is stale evidence. Do not diagnose current controller
physics from it.

The missing include is now fixed on public `main`.

## Next gate

On Windows `D:\__elite\work`:

1. Pull current `main`.
2. Rebuild ONLY `local_flight_control_contract_tests`.
3. Run ONLY CTest `local_flight_control_contracts`.
4. If the freshly linked test passes, continue with:
   - `ship_propulsion_state`;
   - `ordinary_physical_maneuver_compiler`;
   - `docking_advisory`.
5. Then build canonical `build/EliteGame.exe` and run live Assisted/manual
   docking acceptance.

Preserve all untracked trace JSON/TXT files. Do not weaken engine truth, invent
fore-engine hit geometry, or restore retired DockingPathPlanner/GuidanceTunnel.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
