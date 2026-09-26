# CONTINUE PROMPT — Elite Navigation v2 / Automatic docking target-machine gate

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result update those files and regenerate this prompt
from scratch again.

## Current authoritative architecture

### Planner / Follower / physics ownership

Production accepted-program execution is:

```text
trajectory + physical proof
    -> AcceptedManeuverProgram
       reference state + actuator schedule + explicit feedback reserve
    -> TrajectoryFollower
       sampled nominal rear/fore/RCS + bounded B10 feedback
    -> NavigationRuntimeControlBridge::stepProgram
    -> ShipControlState navigationActuatorProgram*
    -> DynamicMotionSystem::applyNavigationActuatorProgram
    -> shared fixed-step physics
```

The nominal propulsion split is Planner-owned. Physics must not reconstruct it
from one net acceleration vector for accepted-program execution.

Follower feedback remains separate and may consume only remaining installed
main authority plus remaining manoeuvre/RCS authority. Controlled-speed,
linear-load, angular, gas and collision physics remain authoritative.

`TrajectoryFollower` has exactly one executable program input:
`AcceptedManeuverProgram`.

`AcceptedShortSegment` is transitional diagnostics-only output from the old
NavigationRuntimeLab and may reach Follower only through
`NavigationRuntimeLabAcceptedProgramAdapter`. Do not restore a Follower
overload for it.

### Automatic docking lifecycle

```text
START DOCKING
 -> client Automatic request
 -> ClientShipCommand::BeginAutomaticDocking
 -> server ControllerKind::Autopilot
 -> BrakeToStop / stabilization
 -> plan trajectory + accepted program
 -> if hull attitude is outside first accepted tolerance:
      Aligning through normal bounded angular demand
      discard stale program
      BrakeToStop / stabilize
      replan from new real state/time
 -> execute accepted program
 -> controlled replan on tracking/propulsion/frame failure
 -> collision-free pre-capture completion
 -> restore Human control
```

Manual guidance remains a separate lifecycle and hands control back after route
publication.

### No planner-only collision bypass

The docking target is still solid shared-physics collision geometry.

Automatic navigation must not use `terminalAllowedObstacleId` or
`terminalObstacleEntrySourceProgressMeters` in `GameServer` to fly into a
solid target that physics would reject.

The current Automatic stage ends at a collision-free pre-capture center outside
the target hit volume. Client and server use
`HubNavigationClearancePolicy::DiagnosticHubInfrastructureClearanceMeters`.

Physical contact/capture/latch is the NEXT separate game-state/physics layer.
Do not fake it with teleportation, direct velocity writes, or planner-only
collision exceptions.

### Angular correctness

- `ManeuverTrackingController` uses exact shortest-arc quaternion attitude
  error, not the old cross-product small-angle approximation; exact 180-degree
  mismatch must still command bounded correction.
- `AcceptedManeuverProgramBuilder` derives omega/alpha across the complete
  trajectory so storage-page boundaries do not reset angular state.
- rotating docking targets supply explicit terminal angular velocity and the
  builder rejects angular capability violations.
- the current ship angular velocity is supplied as the accepted-program initial
  angular state.

### Mode/state ownership still applies

Do not regress:
- default local law = Assisted;
- `LocalFlightControlStateMachine` owns persistent flight-law/alignment state;
- Assisted automatic lateral stabilization is distinct from manual gas-limited
  RCS;
- SimulationSnapshot wire schema = 10;
- Automatic command wire protocol = 11;
- `ClientModeState` owns locale/constellations/sky culture/coordinate format;
- `MapModeState` owns Galaxy/System/Detail/Hub;
- no retired `SystemMapRenderer::m_mode`;
- no `CoordinateDisplayService::cycle()`.

## Focused gates

`verify_modes.sh` includes:
- local flight native contract;
- client preferences native contract;
- wire protocol native round-trip;
- mode/state and wire-schema static contracts.

`verify_docking.sh` now includes:
- native `docking_advisory`;
- native `accepted_maneuver_program_builder`;
- native `navigation_runtime_control`;
- native `maneuver_tracking_controller`;
- static manual docking contract;
- static Automatic docking ownership/execution contract;
- static live navigation control contract.

## Immediate target-machine commands

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

Do not run stale binaries after a failed test/build.

If all commands pass:

```bash
build/EliteGame.exe
```

## Live evidence to collect

For a compatible/free port:

1. `START DOCKING` is enabled.
2. Expect `[DockAuto] ... phase=stabilizing`.
3. Planned log contains pages, trajectory duration, pre-capture depth,
   terminal omega and initial attitude error.
4. If initial attitude is outside tolerance:
   - planned log says `phase=aligning`;
   - hull visibly turns under Autopilot;
   - log reaches `phase=aligned-replan`;
   - server recalculates before any accepted translation is executed.
5. Once aligned, expect a fresh plan with `phase=executing`.
6. Translation must use the accepted rear/fore/RCS schedule through
   `stepProgram`; no direct transform/velocity mutation.
7. Tracking/propulsion/frame failure must return to controlled stabilization
   and replan, never silently disable navigation.
8. Successful current slice ends with:
   `approach-complete ... reason=pre-capture-envelope-complete`
   and Human authority restored.

If a gate fails, use the FIRST real compiler/test error. Do not restore retired
compatibility APIs to silence stale tests.

Commit fixes directly to GitHub `main`. Do not provide patch files.
