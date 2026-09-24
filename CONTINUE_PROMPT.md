# CONTINUE PROMPT — Elite Navigation v2 / docking guidance + autopilot boundary

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

RCS is separate. Main propulsion retains priority inside the shared linear
acceleration envelope; secondary RCS is trimmed if necessary.

Healthy Assisted uses fore main for strong nose-first braking.
Failed fore -> flip + aft main.
Failed aft + live fore -> fore main becomes primary and working travel direction
becomes `-hullForward`.

Newtonian follows the same hardware truth:
- healthy aft remains primary;
- failed aft + live fore -> '+' burns fore main along `-hullForward`;
- BrakeToStop aligns for the surviving bank;
- '-' remains a no-op rather than synthetic reverse thrust.

## Verified Windows evidence

Fresh Windows target gates have passed:
- `local_flight_control_contracts`;
- `ordinary_physical_maneuver_compiler`;
- `ship_propulsion_state`;
- `docking_advisory`;
- `json_numeric_locale`;
- `check_local_flight_control.py`;
- `check_manual_docking_advisory.py`.

The canonical full game build also progressed after the Windows `near`
identifier fix and produced live docking guidance runs.

Latest live manual guidance evidence:
- request 1 gate 2: lateral -82.8289 m vs release 75 m;
- request 3 gate 17: lateral 77.9429 m vs release 75 m;
- request 4 gate 2: vertical -62.1068 m vs release 61.1359 m.
These are measured real corridor exits; the old dock-axis failure is absent.

## New guidance geometry

User observed that the station turn is too coarse with 500 m frames and wants
the final approach to be a radius, not a broken polyline.

Implemented on main:
- DockingAdvisoryPlanner corner smoothing is now a true circular fillet;
- desired radius starts from v^2/a lateral capability and shrinks only to fit
  adjacent segment room;
- downstream speed profile remains responsible for a smaller-radius feasible
  turn;
- ordinary frame spacing remains 500 m;
- within the final 2000 m, spacing becomes 250 m;
- display compression uses along-route progress rather than Euclidean chord
  distance, so curves are not collapsed into long chords;
- native docking test verifies the terminal density and multiple published
  gates lying on the expected circle.

## Docking preparation stop truth

SHOW ROUTE already exercises temporary authoritative Autopilot ownership.

Server:
Human -> Autopilot, discards pending human control, repeatedly commands
`VelocityAlignmentMode::BrakeToStop`.

Client planning does not begin until replicated authoritative state has:
- Hub-relative speed <= max(0.05 m/s, ship stop epsilon);
- angular rate <= 0.01 rad/s;
- both held for 0.25 s.

New diagnostics:
- `[DockPrep] begin ... vrel_mps=... omega_radps=... law=...`
- `[DockAdvisory] request=... phase=settled vrel_mps=... omega_radps=... hold_s=...`

Use these values to verify the user's suspicion that the initial zero-speed stop
may not be visually obvious. If a route is published, the settle gate must have
been observed in replicated state, but the new log is the required explicit
evidence.

## Automatic docking boundary

`DockingRouteRequest::Mode::Automatic` exists, but the active SpaceState
docking implementation currently accepts only `Mode::Guidance`.

Do NOT enable full docking by writing a second ad-hoc waypoint controller that
chases advisory frames.

The correct automatic chain is:
accepted physical maneuver program
-> TrajectoryFollower
-> NavigationRuntimeControlBridge
-> ShipControlState
-> shared ship physics.

SHOW ROUTE's stop phase already tests a limited Autopilot function; full route
autopilot remains an execution milestone until docking planning authors/accepts
a physical maneuver program for that chain.

## Next target gate

On Windows `D:\__elite\work`:

1. Pull current main.
2. Build/run focused docking advisory test and static check.
3. Build canonical game with `bash build_mingw64.sh`.
4. Run SHOW ROUTE with combined stdout/stderr.
5. Confirm circular station turn and 250 m frames inside final 2 km.
6. Capture DockPrep begin and phase=settled VREL/omega values.
7. Confirm route publication and Human hand-back.
8. If these pass, next implementation milestone is wiring the docking physical
   program into the existing TrajectoryFollower/control bridge for a real
   automatic flight test.

Preserve untracked trace/log artifacts. Do not weaken corridor bounds merely to
hide a turn-tracking defect, invent fore-engine hit geometry, or restore retired
DockingPathPlanner/GuidanceTunnel.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
