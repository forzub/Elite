# CONTINUE PROMPT — Elite Navigation v2 / reroute-before-tighten + Automatic docking executor

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four state documents before starting the next
implementation slice. Regenerate this prompt from current truth every iteration.

## Current manual docking truth

SHOW ROUTE already performs a real temporary server Autopilot takeover and
physical BrakeToStop. Planning begins only after authoritative Hub-relative
speed <= max(0.05 m/s, ship stop epsilon), angular rate <=0.01 rad/s, held for
0.25 s.

Manual Assisted profile:
- final docking-axis approach: 3000 m;
- terminal fillet fraction: 0.75;
- preferred human-flyable terminal radius: 1500 m.

The 1500 m value is NOT a hard route-failure floor.

DockingAdvisoryPlanner now uses ordered fallback:
1. nominal route at preferred radius;
2. expanded-clearance/full-obstacle geometric reroute at preferred radius;
3. only after reroute is exhausted, tighten the terminal circular arc as far as
   collision-free geometry requires.

A route may therefore go substantially around station geometry. Planner failure
is appropriate only when no collision-free rounded route exists, or when the
mandatory final docking-axis ingress itself is blocked.

Plan diagnostics:
- `terminalDetourUsed`;
- `terminalTurnRadiusRelaxed`;
- `terminalTurnRadiusMeters`.

SpaceState logs:
`[DockAdvisory] ... route=nominal|detour terminal_radius_m=... radius_relaxed=0|1`.

Native regression includes:
- preferred arc blocked -> valid detour, preferred radius retained;
- 1500 m impossible from segment room -> valid tighter radius, task retained.

Fresh Windows evidence for these new tests is pending.

## Automatic docking boundary

`DockingRouteRequest::Mode::Automatic` already exists, but production player
docking is not yet executable end-to-end:
- `SystemMapRenderer` keeps START DOCKING disabled;
- `SpaceState::updateDockingAdvisory()` accepts Guidance only;
- server docking preparation owns Autopilot only for BrakeToStop and
  `finishDockingGuidancePreparation()` restores Human authority.

Do NOT solve this by chasing visual DockingAdvisoryGate frames or by attaching
the transitional NPC immediate-intent controller to the player.

The required execution chain is:
`AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> ShipControlState
 -> shared ship physics`.

The next implementation slice after the focused reroute gate is a server-owned
player docking execution lifetime that retains Autopilot authority, consumes
proved accepted programs, replans/stops on invalidation, and only then enables
START DOCKING.

## Immediate Windows gate

From `D:\__elite\work`:
1. `git pull --ff-only origin main`
2. rebuild `docking_advisory_tests`
3. run the `docking_advisory` CTest
4. run `python tests/architecture_contracts/check_manual_docking_advisory.py`
5. if both pass, `bash build_mingw64.sh`
6. run `build/EliteGame.exe`
7. Assisted SHOW ROUTE: capture DockPrep begin/settled plus route/radius log and
   visually verify that blocked preferred geometry reroutes/tightens instead of
   cancelling the navigation task.

Preserve all untracked trace/log artifacts. Do not weaken corridor truth,
collision checks, propulsion truth, or the server ownership model merely to
make tests pass.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands. Do not provide patch files.
