# CONTINUE PROMPT — Elite Navigation v2 / dual-main propulsion + manual docking

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Current propulsion contract

Cobra now has TWO real longitudinal main-engine banks in the authoritative
descriptor:

- aft/rear main: thrust along ship forward;
- fore/nose main: thrust opposite ship forward;
- both use the current 7.5 g Cobra linear rating;
- both are bound to runtime module identities through
  `ShipDescriptor::mainPropulsion`.

Main propulsion is binary under damage:
- operational bank -> full descriptor thrust;
- failed bank -> zero;
- never proportionally derate main thrust from module health.

Runtime module state is the source of failure truth. Do not add parallel
navigation-only engine-health flags.

Control-law behavior:
- healthy Assisted uses the physical fore main for strong nose-first braking;
- if fore main fails, direct reverse authority falls to RCS and strong braking
  must flip the hull and use the aft main;
- if aft main fails while fore main survives, the fore bank becomes primary and
  the ship's working travel direction becomes `-forward` (tail-first);
- Newtonian and Assisted share the same installed hardware. Doctrine differs;
  control law must never invent or erase propulsion hardware.

## Implemented code

Current `main` contains:
- explicit directional main-engine ratings in `ShipParams` /
  `ShipDynamics`;
- `ShipDescriptor::mainPropulsion` bindings;
- Cobra fore-engine damage modules plus aft/fore bank bindings;
- `ShipPropulsionState.h` deriving effective binary propulsion from module
  runtime/snapshots;
- `ShipCore::effectivePhysics()`;
- authoritative GameSimulation motion/navigation capability and client manual
  docking planning consuming effective propulsion;
- Assisted reverse working direction after aft-main failure;
- BrakeToStop attitude selection that keeps healthy Assisted fore-main braking
  nose-first and falls back to aft-main flip when reverse main is unavailable;
- B5 `OrdinaryPhysicalManeuverCompiler` supports Assisted and distinguishes
  total body-axis/RCS authority from explicit forward/reverse MAIN authority;
- B5 can compile a fore-main primary burn after aft-main failure by rotating
  the hull so `-forward` matches the demanded thrust vector;
- `AcceptedManeuverProgram::CapabilitySnapshot` retains explicit main-bank
  authority.

Focused regressions added/updated:
- `ship_propulsion_state`;
- `ordinary_physical_maneuver_compiler`;
- `local_flight_control_contracts`;
- static `check_local_flight_control.py`.

The new fore-engine logical modules currently have no dedicated mesh hit-volume
because no real fore-engine mesh parts are authored. Do not bind them to
unrelated Cobra geometry merely to create a fake damage target. Runtime/debug
module failure semantics are valid now; physical hit geometry can be added when
real geometry exists.

## Verification status

These propulsion changes are published on GitHub `main`, but they have NOT yet
been compiled/run on the user's Windows MinGW64 target. No GitHub Actions run is
configured for the current head, and the assistant execution container cannot
clone GitHub because outbound DNS/network is unavailable. Do not claim a build
or test pass until target evidence is supplied.

Preserve the user's untracked trace JSON/TXT files.

## Existing manual docking context

Latest live evidence predates this propulsion slice. SHOW ROUTE reached an
active route and request serial 8, then the old immediate corridor rule
cancelled at gate 12 for vertical 60.1343 m against nominal 60 m. Current code
already adds nominal/warning/release corridor semantics, HUD warning blink,
500 m speed-label range, frame-size semantics and bottom marker. Automatic
DOCKING remains disabled.

Manual SHOW ROUTE lifecycle target remains:
Human -> Autopilot takeover -> physical Hub-relative stabilization -> one
authoritative planning snapshot -> route publication -> authoritative Human
hand-back.

## Next gate

On Windows `D:\__elite\work`:

1. `git pull --ff-only origin main` without deleting untracked traces.
2. Run focused propulsion/compiler/local-flight tests first.
3. Run existing docking-advisory/manual-docking focused checks.
4. Build canonical `build/EliteGame.exe` with `bash build_mingw64.sh`.
5. Test healthy Assisted braking from non-zero VREL.
6. If convenient through debug module controls, test fore-bank failure and
   aft-bank failure separately.
7. Repeat SHOW ROUTE and capture complete `[DockPrep]`, `[DockAdvisory]`
   and corridor-warning output.

Do not weaken engine truth, widen navigation geometry to hide a propulsion
failure, or restore retired `DockingPathPlanner` / `GuidanceTunnel`.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
