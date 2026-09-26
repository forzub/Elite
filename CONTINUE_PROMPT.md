# CONTINUE PROMPT — Elite Navigation v2 / verify mode-state refactor, then resume Automatic docking

Work in public repository `forzub/Elite`, branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those files and regenerate this prompt.

## Current mode/state architecture

Persistent selectable modes have one authoritative owner:
- local flight law/alignment/Assisted persistent controls:
  `LocalFlightControlStateMachine` over `DynamicMotionState`;
- global UI locale / constellation visibility / sky culture / coordinate
  display format: `ui::platform::ClientModeState`;
- navigation module state: `NavigationModuleState`;
- application presentation: `GamePresentationCoordinator`;
- SystemMap Galaxy/System/Detail/Hub submode: `MapModeState`.

Renderers/formatters/services are projections and must not own independent mode
transitions.

## Flight-control changes awaiting Windows verification

- single shared default law: Assisted;
- DynamicMotionState / ShipControlState / GameClient latch consume that default;
- Assisted entry captures forward VREL rather than total |VREL|;
- Assisted damps released angular motion;
- Newtonian preserves angular inertia without explicit alignment;
- Assisted has separate
  `assistedStabilizationAccelerationMps2`, using central ShipDynamics
  `strafeAccel` capability;
- manual keypad RCS remains gas-limited `manoeuvreThrusterAccel`;
- DynamicMotionSystem/SharedShipPhysics/ShipController may not directly mutate
  persistent flight mode fields;
- snapshot wire schema version is 10.

A native regression verifies Assisted cancels side-slip while Newtonian
preserves it and Assisted stabilization does not consume manual RCS gas.

## Client/map mode changes awaiting verification

- ClientModeState owns locale, constellation visibility, sky-culture and
  coordinate display format;
- preferences are persistence only;
- CoordinateDisplayService is projection-only and has no hidden cycle;
- SystemMapRenderer no longer resets coordinate mode during init;
- SystemMapRenderer no longer stores loose `m_mode`; it uses MapModeState.

## Immediate Windows gate

From `D:\__elite\work`:

```bash
git pull --ff-only origin main
git log -1 --oneline
bash verify_modes.sh
```

If PASS:

```bash
bash verify_docking.sh
bash build_mingw64.sh
build/EliteGame.exe
```

Live acceptance:
- fresh mode Assisted;
- Assisted hull turn actively removes/bends lateral VREL;
- Assisted neutral angular rotation damps;
- Newtonian side-slip and angular inertia persist without explicit thrust;
- Ctrl+F10 transitions cleanly between laws;
- locale/constellation/sky-culture/coordinate modes still switch and persist;
- Galaxy/System/Detail/Hub map transitions remain unchanged.

If compile/test failures appear, fix only the concrete regressions and rerun the
same gate. Do not start another broad architectural sweep.

After green mode + docking gates, resume production Automatic docking:
server Autopilot ownership -> accepted proved maneuver program ->
TrajectoryFollower -> NavigationRuntimeControlBridge -> ShipControlState ->
shared physics -> completion/replan/controlled-stop -> Human handback.

Commit directly to GitHub; do not provide patch files.
