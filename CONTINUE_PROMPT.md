# CONTINUE PROMPT — Elite Navigation v2 / wide manual Assisted docking + automatic execution boundary

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

Live diagnostics:
- `[DockPrep] begin ... vrel_mps=... omega_radps=... law=...`
- `[DockAdvisory] ... phase=settled vrel_mps=... omega_radps=... hold_s=...`

## Live manual guidance evidence

The historical dock-axis failure is gone. Recent Windows cancellations are real
corridor departures, e.g. lateral 77.8971/75 m and 82.4922/75.3754 m near the
station turn.

Frame density was improved:
- 500 m open transit;
- 250 m in final 2 km;
- explicit transition anchor so sparse 500 m cadence cannot jump into the dense
  region.

Circular fillets replaced the old quadratic Bezier smoothing.

## New manual Assisted terminal policy

User confirmed the final Assisted turn is still too tight and visually flattens
the corridor near the dock.

Root cause: desired radius was large, but the final axis was only about 900 m
and fillets could consume only 40% of adjacent segments, constraining a
90-degree turn toward roughly 360 m radius.

Current main now applies a human-flyable profile only for manual Assisted:
- terminal docking-axis approach length = 3000 m;
- terminal fillet segment fraction = 0.75;
- minimum terminal turn radius = 1500 m;
- if obstacle clearance or segment room would force the terminal turn below
  1500 m, planner rejects the route instead of silently shrinking it.

Manual Newtonian keeps the sharper legacy geometry because hull attitude and
velocity are decoupled.

SpaceState logs the selected profile:
`[DockAdvisory] request=... profile=manual-assisted final_axis_m=3000 turn_fraction=0.75 min_turn_radius_m=1500`.

Native docking regression now configures the same Assisted profile and requires
the analytically expected terminal radius to be >=1500 m.

Fresh Windows rerun is pending.

## Automatic docking boundary

`DockingRouteRequest::Mode::Automatic` exists, but current SpaceState docking
execution explicitly accepts only `Mode::Guidance`. Therefore full player
route autopilot is not yet testable end-to-end.

Do NOT implement a second ad-hoc waypoint autopilot that chases visual frames.
The correct execution chain is:
accepted physical maneuver program
-> TrajectoryFollower
-> NavigationRuntimeControlBridge
-> ShipControlState
-> shared ship physics.

SHOW ROUTE's pre-plan BrakeToStop is already a limited real Autopilot test.

## Next target gate

On Windows `D:\__elite\work`:

1. Pull current main.
2. Rebuild/run `docking_advisory_tests`.
3. Run `python tests/architecture_contracts/check_manual_docking_advisory.py`.
4. If both pass, rebuild canonical game with `bash build_mingw64.sh`.
5. Run Assisted SHOW ROUTE.
6. Confirm profile log reports 3000 / 0.75 / 1500.
7. Visually confirm the station turn is broad and readable.
8. Capture DockPrep begin/settled VREL and Human hand-back.
9. After this live geometry gate, start the Automatic execution slice using the
   existing accepted-program/follower/control-bridge architecture.

Preserve all untracked trace/log artifacts. Do not weaken the 1.5 km Assisted
terminal-radius floor or corridor bounds merely to pass a test.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
