# CONTINUE PROMPT — Elite Navigation v2 / verify state-owned modes, then resume Automatic docking

Work in public repository `forzub/Elite`, branch `main`.

Read newest sections of `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md` before
changing behavior. Synchronize those files after every state-affecting result
and regenerate this prompt every iteration.

## Current mode/state architecture

One authoritative state owner per persistent selectable mode.

Flight:
- default law = Assisted via one shared default function;
- `LocalFlightControlStateMachine` owns persistent law/alignment/Assisted
  target transitions;
- DynamicMotionSystem/SharedShipPhysics/ShipController execute state but do not
  write those persistent mode fields;
- Assisted entry captures longitudinal speed, not total |VREL|;
- Assisted neutral angular damping is automatic; Newtonian neutral rotation is
  inertial;
- Assisted automatic lateral stabilization is distinct from manual gas-limited
  RCS and uses the central load-bounded ship capability;
- replicated stabilizer state bumped SimulationSnapshot wire schema to 10.

Client/UI:
- `ClientModeState` owns UI locale, constellation visibility, sky culture,
  coordinate display format;
- preferences are persistence projection only;
- LocalizationService and CoordinateDisplayService are projection/formatting
  layers, not transition owners;
- SystemMapRenderer no longer owns a loose `m_mode`; `MapModeState` owns
  Galaxy/System/Detail/Hub.

## Latest Windows evidence

First `verify_modes.sh` run:
- client_preferences_store_contracts PASS;
- local_flight_control_contracts failed because the test expected raw 20.0
  m/s² while the test fixture maxGs=2 correctly clamps Assisted lateral
  authority to 19.6133 m/s².

The test is fixed to expect the central production capability accessor.
Wire schema static contract is also updated for schema 10/new stabilizer field.

## Immediate gate

From `D:\__elite\work`:

```bash
git pull --ff-only origin main
git log -1 --oneline
bash verify_modes.sh
```

If PASS:

```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Live flight acceptance:
- fresh ship is ASSISTED;
- releasing pitch/yaw/roll damps angular motion in Assisted;
- Newtonian preserves neutral angular inertia;
- after turning the hull, Assisted materially bends/removes lateral VREL much
  faster than the 2 m/s² manual RCS path;
- Ctrl+F10 changes doctrine through state, not direct physics flags;
- locale/constellation/culture/coordinate/map modes remain functional.

After this gate return to the production Automatic docking executor:
AcceptedManeuverProgram -> TrajectoryFollower ->
NavigationRuntimeControlBridge -> ShipControlState -> shared physics.

Commit directly to GitHub; do not provide patch files.
