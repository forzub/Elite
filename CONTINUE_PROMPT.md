# CONTINUE PROMPT — Elite Navigation v2 / docking guidance density + autopilot boundary

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Accepted propulsion and stop truth

Dual-main Cobra propulsion/failure semantics have passed Windows target gates.
SHOW ROUTE already performs a real temporary server Autopilot takeover for
physical BrakeToStop. Planning is gated on authoritative replicated state:
Hub-relative speed <= max(0.05 m/s, ship stop epsilon), angular rate <=0.01
rad/s, held for 0.25 s.

New live diagnostics:
- `[DockPrep] begin ... vrel_mps=... omega_radps=... law=...`
- `[DockAdvisory] ... phase=settled vrel_mps=... omega_radps=... hold_s=...`

## Guidance geometry

User requested:
- circular final/turn geometry rather than broken-looking polyline;
- 500 m frames in open transit;
- 250 m frames in the final ~2 km.

Implemented:
- true circular fillets, desired radius from v^2/a and limited by adjacent
  segment room;
- display compression by along-route progress, not chord distance;
- terminal display spacing target 250 m;
- terminal density nominal distance 2000 m.

## Latest Windows gate evidence

First density test failed:
`terminal advisory gate spacing too sparse: 490`.

A first correction activated terminal cadence one 250 m interval early.

Second Windows rerun still failed:
`terminal advisory gate spacing too sparse: 500`.

Static manual docking architecture check passed in both runs.

Root cause of second failure:
cadence was still chosen from the current published frame. A frame at e.g.
2300 m remaining could legally take a full 500 m sparse step to 1800 m, jumping
across the activation boundary before dense cadence became active.

Current main fixes the class structurally:
- define terminal activation remaining as
  `terminalDenseDistanceMeters + terminalSpacing`;
- if current frame is already inside, use terminal spacing;
- if current frame is outside but a normal sparse step would cross activation,
  shorten THIS interval to `distanceToActivation`;
- this explicitly places a transition frame at/just before the boundary;
- all following intervals use terminal cadence.

Do not relax the native test. Fresh Windows rerun is pending.

## Automatic docking boundary

`DockingRouteRequest::Mode::Automatic` exists, but current SpaceState active
docking implementation accepts Guidance only.

Do NOT implement a second ad-hoc waypoint autopilot. Full docking must use:
accepted physical maneuver program
-> TrajectoryFollower
-> NavigationRuntimeControlBridge
-> ShipControlState
-> shared physics.

SHOW ROUTE's stop phase already tests limited real Autopilot ownership.

## Next target gate

On Windows `D:\__elite\work`:

1. Pull current main.
2. Rebuild/run only `docking_advisory_tests`.
3. Run `python tests/architecture_contracts/check_manual_docking_advisory.py`.
4. If both pass, rebuild canonical game with `bash build_mingw64.sh`.
5. Run SHOW ROUTE and inspect circular turn, final 250 m frames, and DockPrep
   begin/settled VREL+omega diagnostics.
6. Confirm Human hand-back after route publication.

Preserve all untracked trace/log artifacts. Do not weaken corridor bounds,
terminal-density assertions, or propulsion truth merely to make tests pass.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
