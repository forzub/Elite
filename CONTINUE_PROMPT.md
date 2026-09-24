# CONTINUE PROMPT — Elite Navigation v2 / dual-main propulsion + docking acceptance

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Propulsion truth

Cobra has physical aft/rear and fore/nose longitudinal main-engine banks in
the authoritative descriptor, both using the existing 7.5 g linear rating.
Static bank membership lives in `ShipDescriptor::mainPropulsion`; current
availability comes from runtime module state.

Main-bank damage semantics are binary:
- operational bank -> full descriptor authority;
- failed bank -> zero authority;
- never proportionally derate main thrust from module health.

RCS remains a separate actuator. Planner and physics must never treat residual
RCS as a surviving main engine.

Control-law behavior:
- healthy Assisted uses the real fore main for strong nose-first braking;
- failed fore bank -> strong braking requires flip + aft main;
- failed aft bank with live fore bank -> fore main may become primary and the
  working travel direction becomes `-forward`;
- Newtonian and Assisted consume the same hardware truth.

B5 now supports Assisted and keeps explicit forward/reverse MAIN authority
separate from total body-axis authority.

## Latest Windows target evidence

The first focused target run reported:
- `json_numeric_locale`: PASS;
- `check_local_flight_control.py`: PASS;
- `local_flight_control_contracts`: FAIL:
  `Assisted controller exceeded ship maxGs envelope`;
- `check_manual_docking_advisory.py`: FAIL:
  `speed label is still tied to arbitrary corner[1]`.

Both failures have been diagnosed and corrected on `main`.

### Flight correction

The Assisted controller could combine a main-engine vector already at the
linear envelope with a perpendicular RCS correction, so the magnitude of
`main + RCS` slightly exceeded the allowed linear envelope.

Main thrust must NOT be derated to solve this. Current code preserves the
selected main-engine acceleration and trims only the subordinate RCS vector
until the combined vector fits the total linear acceleration envelope.

This is not health-based power scaling. Main bank health remains full-or-zero.

### Docking checker correction

The speed label already uses
`projectedUpperLeft(projected.corners)`. The Python test searched globally for
`projected.corners[1]` and accidentally matched the separate semantic dock
bottom-marker geometry. The check now inspects only the speed-label placement
block and requires the stable projected upper-left anchor.

## Next gate

On Windows `D:\__elite\work`:

1. Pull current `main`.
2. Rebuild/rerun `local_flight_control_contracts`.
3. Rerun `check_manual_docking_advisory.py`.
4. If both pass, run:
   - `ship_propulsion_state`;
   - `ordinary_physical_maneuver_compiler`;
   - `docking_advisory`;
   - `check_local_flight_control.py`.
5. Build canonical `build/EliteGame.exe`.
6. Test healthy Assisted SHOW ROUTE from non-zero Hub-relative speed.
7. If practical through debug module controls, test fore-bank and aft-bank
   failures separately.
8. Continue corridor-warning/HUD acceptance.

Preserve all untracked trace JSON/TXT files. Do not weaken engine truth, invent
hit geometry for the logical fore-engine modules, widen navigation geometry to
hide propulsion defects, or restore retired DockingPathPlanner/GuidanceTunnel.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
