# CONTINUE PROMPT — Elite Navigation v2 / finish mode-state build gate, then resume Automatic docking

Work in public repository `forzub/Elite`, branch `main`.

Read newest sections of `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md` before
changing behavior. After every state-affecting result synchronize those files.
Regenerate this prompt from scratch every iteration.

## Current architecture

Persistent selectable modes have one authoritative state owner.

Flight:
- default local flight law = Assisted;
- `LocalFlightControlStateMachine` owns persistent law/alignment/Assisted
  target transitions;
- DynamicMotionSystem/SharedShipPhysics/ShipController execute the state but do
  not directly own those persistent transitions;
- Assisted entry captures longitudinal VREL, not total |VREL|;
- Assisted neutral angular motion damps; Newtonian neutral angular motion is
  inertial;
- Assisted automatic lateral stabilization is separate from manual gas-limited
  RCS and is load-bounded through ShipDynamics;
- SimulationSnapshot wire schema = 10 with
  `assistedStabilizationAccelerationMps2`.

Client/UI:
- `ui::platform::ClientModeState` owns UI locale, constellation visibility,
  sky culture and coordinate display format;
- preferences are persistence projections only;
- `CoordinateDisplayService` and LocalizationService are projection/formatting
  services, not state-transition owners;
- `MapModeState` owns Galaxy/System/Detail/Hub mode.

## Latest Windows evidence

1. Focused mode-state test initially failed because the test expected raw
   20.0 m/s² Assisted lateral authority while its fixture had maxGs=2. The
   correct production limit is 19.6133 m/s². The test now derives the expected
   value from `assistedLateralStabilizationAccelerationLimitMps2(params)`.

2. The subsequent canonical game build reached
   `ClientAcceptanceHarness.cpp` and failed because the harness still called
   removed `CoordinateDisplayService::cycle()`.

That harness is now corrected. It exercises:
`ClientModeState transition -> nextCoordinateDisplayFormat ->
CoordinateDisplayService::setFormat projection`,
including three transitions, wraparound and revision increments. Do NOT restore
a service-owned `cycle()`; that would reintroduce shadow mode state.

Fresh target-machine verification is pending.

## Immediate gate

From `D:\__elite\work`:

```bash
git status --short
git pull --ff-only origin main
git log -1 --oneline
bash verify_modes.sh
bash build_mingw64.sh
```

If both are green:

```bash
build/EliteGame.exe
```

Live acceptance:
- fresh ship is Assisted;
- releasing pitch/yaw/roll damps angular motion in Assisted;
- Newtonian preserves neutral angular inertia;
- after hull rotation, Assisted materially removes/bends lateral VREL faster
  than the 2 m/s² manual RCS path;
- Ctrl+F10 cleanly switches doctrine;
- Ctrl+F11 coordinate display still cycles and persists;
- locale/constellation/sky-culture/map modes remain functional.

After this gate, return to the production Automatic docking executor:
server Autopilot ownership -> accepted proved `AcceptedManeuverProgram` ->
`TrajectoryFollower` -> `NavigationRuntimeControlBridge` ->
`ShipControlState` -> shared physics -> completion/replan/controlled stop ->
Human handback.

Commit directly to GitHub. Do not provide patch files.
