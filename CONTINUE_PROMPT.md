# CONTINUE PROMPT — Elite Navigation v2 / shared game-flight autopilot

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read the newest relevant sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update CURRENT_STATE / CURRENT_TASK / PROJECT_STATE / STAGE12_END_TO_END as appropriate;
2. update CONTROL_LAW_MANEUVER_MODEL when flight doctrine changes;
3. REGENERATE THIS CONTINUE_PROMPT FROM SCRATCH;
4. commit directly to GitHub `main`.

Do not send patch files to the user. Fresh Windows/MSYS2 evidence is required
before claiming a new code slice accepted.

## User-approved flight behavior

Manual Assisted is ACCEPTED. Do not retune it without new live evidence.

Assisted feels almost aircraft-like:
- where the hull nose points, the velocity/course should follow;
- finite hull angular acceleration/rate still applies;
- velocity realignment may lag, but should be within about 2–3 seconds;
- forward/reverse main propulsion owns longitudinal speed;
- the automatic Assisted velocity-to-nose stabilizer is a game-flight
  capability distinct from the small physical keypad/manoeuvre RCS.

Cobra current values include:
- forward main: 73.549875 m/s²;
- reverse main: 73.549875 m/s²;
- Assisted automatic stabilization (`strafeAccel`): 73.549875 m/s², bounded by
  the shared 7.5 g linear envelope;
- physical `manoeuvreThrusterAccel`: 2.0 m/s²;
- angular acceleration/rate remain bounded by ShipDynamics.

Physical manoeuvre/RCS is precision authority: manual strafe/lift, final
centering, close formation/placement, parking trim. Ordinary Assisted route
curvature MUST NOT be tested against the 2 m/s² RCS budget.

## Latest Windows evidence BEFORE the current replacement

At HEAD `c6dee4ee`:
- `navigation_runtime_control`: PASS;
- `maneuver_tracking_controller`: PASS;
- `accepted_maneuver_program_builder`: PASS;
- `docking_advisory`: FAIL:
  `manual docking corridor did not start along hull nose`.

Live Automatic:
```text
[DockAuto] ... phase=plan-failed
reason=accepted-program-propulsion-program-infeasible
action=restore-human
```

The old synchronous ~1 s docking freeze was NOT reproduced in that run.
The center cockpit boresight is accepted by the user and should remain.

## Root cause of the Automatic failure

Manual and Automatic Assisted were using different motion laws.

Manual:
`DynamicMotionSystem::applyLocalFrameInput` uses the dedicated Assisted
velocity-to-nose stabilizer to cancel old lateral VREL and bend the velocity
vector toward the new hull nose.

Old Automatic:
`applyNavigationActuatorProgram` zeroed Assisted stabilization, decomposed the
trajectory acceleration into main + physical manoeuvre/RCS, and rejected
ordinary course changes when the residual exceeded 2 m/s².

That universal actuator decomposition was conceptually wrong for Assisted.

## Current replacement on main

### Accepted program has an explicit translation mode

`AcceptedManeuverProgram::TranslationMode`:
- `AssistedVelocity`
- `NewtonianMainEngine`
- `PrecisionRcs` (reserved for explicit precision/capture phases)

Programs also carry their `LocalFlightControlLaw`.

### Assisted execution

Assisted Builder behavior:
- does NOT call propulsion decomposition for ordinary transit;
- emits `actuatorSegmentCount = 0`;
- validates speed/orientation state;
- never publishes synthetic route RCS.

Follower derives target forward speed from the accepted velocity reference.

Bridge publishes:
- `navigationAssistedFlightModelValid`
- `navigationTargetForwardSpeedMps`
- `navigationAssistedCorrectionSystemMps2`

GameSimulation routes this to
`DynamicMotionSystem::applyNavigationAssistedFlightModel`.

That function:
1. sets the target speed through
   `LocalFlightControlStateMachine::requestAssistedTargetSpeed`;
2. calls the SAME `applyLocalFrameInput` used by manual Assisted;
3. keeps ordinary physical manoeuvre/RCS at zero;
4. spends bounded lateral Follower correction inside the automatic Assisted
   stabilization budget, not keypad RCS.

Canonical Assisted execution:
```text
law-aware trajectory/reference
 -> AcceptedManeuverProgram(AssistedVelocity)
 -> TrajectoryFollower(target speed + tracking/angular intent)
 -> NavigationRuntimeControlBridge
 -> ShipControlState navigationAssistedFlightModel*
 -> DynamicMotionSystem::applyNavigationAssistedFlightModel
 -> SAME applyLocalFrameInput as manual Assisted
 -> shared fixed-step integration
```

### Planner vehicle profile

Production docking now supplies the actual control law explicitly to
`makeNavigationVehicleProfile`.

Assisted:
- course-change/lateral timing authority =
  `assistedLateralStabilizationAccelerationLimitMps2`;
- NOT `manoeuvreThrusterAccel`.

Legacy generic profile callers retain their historical hardware-envelope
semantics and were not silently converted to Assisted.

### Newtonian split

Design direction is two large ship-motion families, not two cosmetic control
modes over one parking algorithm.

Newtonian/heavy craft:
- likely faster but much less maneuverable;
- velocity independent from hull attitude;
- ordinary strategy = coast, rotate, main burn, coast, rotate/flip, brake;
- favor long nearly straight legs and large maneuvering space;
- parking/pre-capture is slower;
- final placement may later use stronger class-specific manoeuvre thrusters or
  tugs.

Current code establishes the hard boundary:
- docking geometry sets `roundTurns = false` for Newtonian;
- ordinary Newtonian Accepted programs use primary main engine only;
- ordinary Newtonian route RCS is zero;
- lateral acceleration that cannot be produced by aligned main thrust fails as
  `newtonian-main-engine-program-infeasible`.

This is intentional. Do NOT hide the remaining Newtonian compiler work by
increasing RCS or restoring arbitrary-vector allocation.

A dedicated Newtonian `coast -> rotate -> burn` /
`accelerate -> rotate -> brake` maneuver compiler is still future work and
must be treated separately from Assisted parking.

The runtime Ctrl+F10 law switch remains temporarily for development/regression.
Architecture no longer depends on it being a player preference; later a ship
descriptor can lock the family without redesigning Planner/Follower.

## Nose-first manual corridor fix

Manual planning still requests a real hull-forward lead:
```text
max(500 m, 10 * hull length)
```

The latest failed native test showed generic filleting could consume the first
straight segment after it was prepended.

Now the first semantic launch corner is protected:
`initialForwardLeadActive && i == 1` explicitly preserves
`routeSearchStart` before later fillets.

Planner may shorten a blocked lead but may not rotate it. If even the minimum
forward segment is blocked, fail with:
`initial forward corridor blocked`.

Newtonian no-round geometry additionally proves every straight segment clear.

## Regressions added

`AcceptedManeuverProgramBuilderTests`:
- Assisted accepts ordinary lateral/course demand even when physical RCS is only
  2 m/s²;
- Assisted program is `TranslationMode::AssistedVelocity`;
- Assisted ordinary program has zero actuator/RCS segments;
- Newtonian ordinary transit cannot use even a deliberately strong RCS budget
  to fake lateral route thrust.

`NavigationRuntimeControlTests`:
- Automatic Assisted calls the canonical game-flight law;
- starting with VREL +X and hull nose -Z, target forward speed 100 m/s must
  realign course to within 5 degrees in 3 seconds;
- ordinary `manoeuvreAccelerationMps2` remains zero.

Static contracts pin:
- Assisted game-flight execution channel;
- explicit control-law planner projection;
- Newtonian no-fake-RCS doctrine;
- nose-first launch lead protected from filleting.

`verify_docking.sh` now also runs
`check_local_flight_control.py`.

## Automatic docking async invariant

Heavy planning remains outside fixed-step:
- server takes Autopilot authority;
- stabilizes/stops;
- snapshots immutable planning input;
- worker runs advisory + trajectory + accepted-program construction;
- fixed-step polls the atomic result only.

Never restore:
- synchronous heavy planning in fixed-step;
- `phase=plan-retry` retry storms.

Existing entry alignment is retained:
- first accepted reference defines route-entry attitude;
- if real hull differs, `Phase::Aligning` physically turns it;
- aligned stale program is discarded;
- server stabilizes/replans from actual state;
- only fresh program executes.

Expected lifecycle when alignment is needed:
```text
phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```

Direct `phase=executing` is legal when already aligned.

## Immediate Windows/MSYS2 gate

Run exactly:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh

bash build_mingw64.sh

build/EliteGame.exe
```

If `verify_docking.sh` fails, fix the FIRST real failure before building/running.
Do not claim success from GitHub-only/static inspection.

## Live Assisted acceptance

1. Center boresight remains visible.
2. CALCULATE TRAJECTORY:
   - Autopilot stops/stabilizes;
   - manual route/tunnel is retained after Human handback;
   - first visible tunnel section starts along boresight/nose.
3. START DOCKING:
   - no old fixed-step planning freeze;
   - route/tunnel stays visible;
   - async plan completes;
   - optional physical align/replan occurs;
   - fresh plan reaches `phase=executing`;
   - ship actually moves along the route.
4. The old
   `accepted-program-propulsion-program-infeasible`
   must NOT be produced merely because Assisted needs to turn/course-correct.
5. If execution replans or cancels, capture every `[DockAuto]` line and exact
   failure reason.

## Architecture invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded tracking error; it is not a second planner.
- Manual and Automatic Assisted share the same game-flight motion law.
- Physical RCS is not ordinary Assisted lateral route authority.
- Newtonian ordinary transit cannot spend precision RCS as fake main thrust.
- ShipDynamics/descriptor remains the source of angular, speed, load and
  installed propulsion limits.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only collision bypass.
- No stale program execution after physical entry alignment.
- No hidden tunnel during Automatic.
- No synchronous Automatic planning retry loop.
- Current docking scope ends at a collision-free pre-capture pose; physical
  latch/contact is later.

## Repository checkpoint

Code HEAD immediately before this prompt regeneration: `eb8af0967904a425320e76055551e90788914abc`.

## Verification status

All changes above are committed to public `main`.
Fresh target-machine `verify_docking.sh`, canonical MinGW build and live
Assisted docking evidence are PENDING. Do not call this replacement accepted
until those gates are green.
