# CONTINUE PROMPT — Elite Navigation v2 / docking guidance + autopilot boundary

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next implementation slice.
Regenerate this prompt from current truth every iteration.

## Propulsion and stop contract

Dual-main Cobra propulsion and binary failure semantics are accepted on Windows:
healthy aft/fore banks retain full authority, failed banks have zero authority,
RCS remains separate, and Planner/B5 plus live local-flight contracts pass.

SHOW ROUTE already performs a real temporary server Autopilot takeover for the
pre-plan stop. Server repeatedly commands BrakeToStop and planning is gated on
replicated authoritative Hub-relative speed <= max(0.05 m/s, ship stop epsilon)
and angular rate <=0.01 rad/s for 0.25 s.

New live diagnostics:
- `[DockPrep] begin ... vrel_mps=... omega_radps=... law=...`
- `[DockAdvisory] ... phase=settled vrel_mps=... omega_radps=... hold_s=...`

## Live guidance evidence

Current game no longer exhibits the old dock-axis cancellation. Latest route
removals are real measured corridor exits, including lateral 82.8289/75 m and
77.9429/75 m cases near turns.

User requested smoother terminal geometry and denser frames:
- circular fillets instead of quadratic Bezier corner smoothing;
- 500 m frames in open transit;
- 250 m frames inside final 2 km.

Implemented circular fillets use v^2/a desired radius, limited by adjacent
segment room. Display compression uses along-route progress rather than chord
distance.

## Latest Windows gate result

The first fresh `docking_advisory` run after final-density changes failed:
`terminal advisory gate spacing too sparse: 490`.

The static manual-docking architecture check passed.

Root cause: the 500->250 transition selected spacing from the candidate endpoint,
so a final sparse ~500 m chord could cross the 2 km threshold and land inside
the dense region before the denser cadence activated.

Fix on current main:
- compute cadence from the current published frame;
- activate 250 m cadence one terminal interval early:
  `terminalDenseDistanceMeters + terminalSpacing` (2000 + 250 m);
- therefore the 2 km boundary is already bracketed by <=250 m frames;
- native regression remains strict for every interval inside the final band and
  requires a transition frame within one terminal spacing of 2 km.

Fresh Windows rerun is pending.

## Automatic docking boundary

`DockingRouteRequest::Mode::Automatic` exists, but current SpaceState docking
execution accepts Guidance only.

Do not create an ad-hoc waypoint autopilot. Full automatic docking must use:
accepted physical maneuver program
-> TrajectoryFollower
-> NavigationRuntimeControlBridge
-> ShipControlState
-> shared ship physics.

SHOW ROUTE's physical stop is already a limited real Autopilot test.

## Next target gate

On Windows `D:\__elite\work`:

1. Pull current main.
2. Rebuild/run only `docking_advisory_tests`.
3. Run `python tests/architecture_contracts/check_manual_docking_advisory.py`.
4. If both pass, rebuild canonical game with `bash build_mingw64.sh`.
5. Run SHOW ROUTE with combined log.
6. Inspect circular final turn, 250 m frame density inside final 2 km, and
   DockPrep begin/phase=settled VREL+omega values.
7. Confirm Human hand-back after route publication.

Preserve all untracked trace/log artifacts. Do not weaken corridor bounds,
engine truth, or terminal-density tests merely to make a gate pass.

The user wants changes applied directly to GitHub and exact Windows commands.
Do not provide patch files.
