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

Newtonian consumes the same hardware truth:
- healthy aft remains primary;
- if aft fails and fore survives, '+' burns fore main along `-hullForward`;
- BrakeToStop aligns nose with velocity and burns fore main opposite nose;
- '-' remains a no-op.

## Verified Windows evidence

Fresh Windows MinGW64 target evidence passed:
- `local_flight_control_contracts`;
- `ordinary_physical_maneuver_compiler`;
- `ship_propulsion_state`;
- `docking_advisory`;
- `json_numeric_locale`;
- `check_local_flight_control.py`;
- `check_manual_docking_advisory.py`.

The navigation-runtime trio was repeated with no rebuild and passed again.

A later final Newtonian fore-main runtime completion was added and needs fresh
focused verification if not already run after the latest pull.

## Latest canonical game-build blocker and fix

The full Windows `bash build_mingw64.sh` reached EliteGame translation units
and failed in `src/game/navigation/DockingAdvisoryCorridor.h` at:

`const auto near = ...`

The full Windows header stack makes `near` unsafe as an identifier. Narrow
test targets did not reproduce that macro environment.

The helper is now renamed to `nearBoundary`. The manual-docking static check
explicitly rejects reintroduction of the unsafe declaration. This is a build
portability fix only; corridor math/semantics are unchanged.

The concurrent winsock2-before-windows.h message is a warning, not the build
failure.

## Next gate

On Windows `D:\__elite\work`:

1. Pull current `main`.
2. Run:
   `python tests/architecture_contracts/check_manual_docking_advisory.py`
   and, if the latest Newtonian fallback has not yet been verified after pull,
   rebuild/run `local_flight_control_contracts` plus
   `check_local_flight_control.py`.
3. Run canonical `bash build_mingw64.sh`.
4. If `build/EliteGame.exe` links, launch it with combined stdout/stderr log.
5. Perform live manual SHOW ROUTE acceptance:
   Human -> Autopilot takeover -> physical Hub-relative stop/settle ->
   authoritative start snapshot -> plan/publish -> visible route/corridor ->
   acknowledged Autopilot off -> Human hand-back.
6. Diagnose any route removal from numeric DockAdvisory axis/left logs; do not
   widen geometry blindly.

Preserve all untracked trace JSON/TXT/log artifacts. Do not weaken engine truth,
invent fore-engine hit geometry, or restore retired DockingPathPlanner /
GuidanceTunnel.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
