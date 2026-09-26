# CONTINUE PROMPT — Elite Navigation v2 / Automatic docking execution gate

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result update those files and regenerate this prompt from scratch again.

## Non-negotiable architecture

### One executable navigation truth

Production execution is:

```text
planner / trajectory proof
    -> AcceptedManeuverProgram
    -> TrajectoryFollower
    -> NavigationFrameBoundary
    -> NavigationRuntimeControlBridge
    -> ShipControlState
    -> SharedShipPhysics / DynamicMotionSystem
```

`TrajectoryFollower` must NOT regain an `AcceptedShortSegment` overload.

`AcceptedShortSegment` remains only as a transitional product inside the old Stage-12 NavigationRuntimeLab. Lab execution must cross
`NavigationRuntimeLabAcceptedProgramAdapter` first.

Do not restore old compatibility paths merely to make tests compile.

### Automatic docking ownership

Automatic docking is server-owned:
- UI creates `DockingRouteRequest::Automatic`;
- client sends `ClientShipCommand::BeginAutomaticDocking` with system/module/semantic-anchor identity;
- wire protocol version is 11;
- server takes `ControllerKind::Autopilot`;
- server commands `BrakeToStop` and waits for a stable Hub-relative start;
- server plans route/trajectory;
- `AcceptedManeuverProgramBuilder` converts the proved time-parameterized trajectory into immutable execution pages;
- Follower + bridge produce ordinary `ShipControlState`;
- no direct position/velocity/orientation write is allowed;
- tracking/propulsion/page/frame failure returns to controlled stabilization/replan;
- server restores Human control when the automatic lifetime ends.

Manual guidance is separate:
temporary Autopilot BrakeToStop -> client advisory route publication -> Human hand-back.

Manual and Automatic docking runtimes must be mutually exclusive for one ship.

Client Automatic state is request/ack only. Local prediction is suppressed only after an authoritative session snapshot confirms Autopilot. A rejected request must not freeze human controls.

### Rotating dock terminal state

The diagnostic cube has authored local spin. Automatic docking must not treat terminal orientation as static-only.

`AcceptedManeuverProgramBuilder` now derives angular kinematics, checks angular speed/acceleration against ship capability, and accepts
`terminalAngularVelocityMapRadPerSec`.

The server estimates the moving port's terminal linear velocity and angular velocity at the predicted capture epoch and passes both into the accepted execution product.

### Mode/state ownership

Do not regress the earlier mode cleanup:
- default local law = Assisted;
- `LocalFlightControlStateMachine` owns persistent flight-law/alignment state;
- Assisted lateral stabilization is distinct from manual gas-limited RCS;
- SimulationSnapshot wire schema remains 10;
- `ClientModeState` owns locale/constellations/sky culture/coordinate format;
- `MapModeState` owns Galaxy/System/Detail/Hub;
- no retired `SystemMapRenderer::m_mode`;
- no `CoordinateDisplayService::cycle()`.

`check_mode_state.py` now explicitly scans both SystemMapRenderer inline implementation files for retired `m_mode`.

## New focused gates

`verify_docking.sh` now runs:
- native `docking_advisory`;
- native `accepted_maneuver_program_builder`;
- static manual docking contract;
- static automatic docking ownership/execution contract.

`verify_modes.sh` now also builds/runs `wire_protocol_contracts` because ClientShipCommand wire data changed.

## Immediate target-machine gate

From `D:\__elite\work`:

```bash
cd /d/__elite/work

git status --short
git pull --ff-only origin main
git log -1 --oneline

bash verify_modes.sh
bash verify_docking.sh
bash build_mingw64.sh
```

The canonical build script builds both:
- `build/EliteGame.exe`
- `build/headless_server/EliteServer.exe`

Do not run stale binaries after a failed build.

If all gates/build pass:

```bash
build/EliteGame.exe
```

## Live acceptance to collect

For a compatible free docking port:

1. `START DOCKING` is enabled.
2. Pressing it produces `[DockAuto] begin ... phase=stabilizing`.
3. Ship visibly brakes/stabilizes before planning.
4. Then expect `[DockAuto] planned ...` with:
   - page count;
   - trajectory duration;
   - gate count;
   - terminal approach/radius diagnostics;
   - `terminal_omega_radps`.
5. Ship motion must visibly follow the planned route through normal physics.
6. No direct teleport / direct velocity assignment is acceptable.
7. If tracking is lost, expect controlled `phase=replan`, not navigation shutdown.
8. End of automatic lifetime must restore Human authority.

Capture the complete console output around `[DockAuto]`, plus any first compiler/test failure exactly.

## Current caveat

This slice implements automatic approach execution and terminal navigation envelope. Physical station latch/contact ownership is still a separate docking/game-state layer. Do not fake latch by teleporting or by ignoring physics collision. If live evidence reaches the terminal envelope cleanly, the next slice is authoritative capture/latch transfer.

Commit fixes directly to GitHub `main`. Do not provide patch files.
