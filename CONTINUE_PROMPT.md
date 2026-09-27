# CONTINUE PROMPT — Elite Navigation v2 / self-contained docking preflight

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update relevant state/task/project/end-to-end MD files;
2. update CONTROL_LAW_MANEUVER_MODEL if doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Fresh Windows/MSYS2 verify/build/live evidence is
required before calling newest code accepted.

## Flight-family doctrine

Manual Assisted is accepted. Do not retune without fresh live evidence.

Assisted:
- hull nose defines intended travel direction;
- VREL/course follows nose with finite lag, target <= about 2–3 s;
- angular rate/acceleration remain bounded;
- forward/reverse main owns longitudinal speed;
- automatic velocity-to-nose stabilization is normal game-flight course
  authority;
- physical manoeuvre/RCS (Cobra currently 2 m/s²) is precision authority, not
  ordinary route curvature authority.

Automatic Assisted uses the SAME game-flight law as manual Assisted:
```text
AcceptedManeuverProgram(AssistedVelocity)
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> ShipControlState navigationAssistedFlightModel*
 -> DynamicMotionSystem::applyNavigationAssistedFlightModel
 -> DynamicMotionSystem::applyLocalFrameInput
 -> fixed-step physics
```

Ordinary Assisted programs publish no synthetic route-RCS actuator schedule.

Newtonian/heavy is a distinct motion family:
- faster, less maneuverable;
- velocity independent from attitude;
- ordinary doctrine = coast -> rotate -> main burn -> coast -> rotate/flip ->
  brake;
- docking favors long nearly straight legs and large maneuvering space;
- final placement may later use stronger class-specific RCS/tugs.

Current Newtonian boundary:
- visible and server docking geometry use no aircraft-like turn rounding;
- ordinary transit cannot spend precision RCS as fake lateral main thrust;
- invalid lateral route demand fails as
  `newtonian-main-engine-program-infeasible`.

Dedicated Newtonian coast/rotate/burn compiler remains future work.

## Existing accepted evidence

Fresh Windows native evidence before the newest live fixes:
```text
navigation_runtime_control .......... PASS
maneuver_tracking_controller ........ PASS
docking_advisory .................... PASS
accepted_maneuver_program_builder ... PASS
trajectory_generator_angular ........ PASS
manual docking static contract ...... PASS
```

Rotating terminal omega is a real boundary:
```text
|omega(t)-omega_terminal| <= alpha_max*(T-t)
```
No final-sample omega snap is allowed.

Cockpit center boresight is accepted by the user and must remain.

## Latest live failure

User tested the game and supplied a cockpit screenshot plus log.

Observed:
- tunnel did not look naturally enterable by the current ship;
- it appeared as a nose-forward piece followed by a route that looked made for
  another trajectory;
- desired shape is:
  `straight current-hull segment -> smooth arc -> route to dock`;
- START DOCKING with no prior CALCULATE TRAJECTORY made the button green and
  displayed AUTOMATIC DOCKING MODE;
- ship did not move;
- FramePerf showed `dock_request=1`;
- captured log had NO `[DockAuto]` line;
- no old cyclic fixed-step planner freeze was observed.

Interpretation: presentation request was active, but Automatic needed a
self-contained route-preflight and stronger observability.

## Newest fixes on main

### 1. Visible Assisted guidance uses the correct flight law

SpaceState previously called the generic
`makeNavigationVehicleProfile(params,envelope)`, whose lateral authority was
physical `manoeuvreThrusterAccel` = 2 m/s².

It now resolves:
```cpp
guidanceControlLaw =
    player->second.transform.motion.localControlLaw;

makeNavigationVehicleProfile(
    effectivePhysics,
    envelope,
    guidanceControlLaw
);
```

For Assisted this uses the accepted velocity-to-nose stabilization capability.
For Newtonian it uses the separate Newtonian course doctrine.

`request.roundTurns = guidanceAssisted`.

### 2. Nose-first launch is now straight -> tangent arc

The previous nose-first fix protected the entire launch lead from filleting,
creating a hard corner at `routeSearchStart`.

Current planner:
- keeps the exact initial hull-forward ray;
- stores `initialForwardAcceptedLeadMeters`;
- protects a visible straight prefix
  (`initialForwardProtectedStraightMeters`, normally 50% of accepted lead);
- allows the remaining lead to be consumed by the first circular fillet via
  `maximumLaunchCut`;
- if obstacle shortening leaves no room for a fillet, keeps the proved straight
  topology honestly;
- densifies published frames around the launch/first turn.

Normal SpaceState docking guidance now uses `gateSpacingMeters = 150.0`
instead of 500 m.

Native DockingAdvisoryPlanner regression now requires:
- first segment along hull nose;
- second segment also along hull nose;
- at least four launch/turn frames;
- no >30 degree discrete heading kink over the first 1200 m;
- a real transition away from the initial direction.

### 3. START DOCKING is self-contained

A manual CALCULATE TRAJECTORY is NOT a prerequisite.

SpaceState determines whether a matching visible corridor already exists.

Cold Automatic:
```text
START DOCKING
 -> phase=route-preflight
 -> BeginDockingGuidancePreparation
 -> physical BrakeToStop / settle
 -> authoritative Hub snapshot
 -> law-aware DockingAdvisoryPlanner
 -> publish visible route/tunnel
 -> CompleteDockingGuidancePreparation
 -> wait for authoritative hand-back
 -> phase=requested
 -> BeginAutomaticDocking
 -> server Stabilizing / async planning / execution
```

An existing corridor for the same dock is reused.

The route-preflight remains presentation/safe-start preparation only; server
Automatic retains execution authority and builds/proves its own accepted
program.

### 4. Server Automatic shares the same launch semantics

Server `DockingAdvisoryRequest` now sets:
```cpp
request.hasInitialForward = true;
request.initialForward = currentForwardMap;
request.initialForwardLeadMeters =
    max(500.0, hull.lengthMeters*10.0);
request.gateSpacingMeters = 150.0;
request.terminalGateSpacingMeters = 150.0;
```

Server Automatic already uses explicit control-law vehicle profile and
`request.roundTurns = assisted`.

### 5. Automatic logging is now observable

Client logs use flushed output for:
```text
[DockAuto] request=N phase=route-preflight
[DockAuto] request=N phase=requested ...
```

Server success begin is flushed:
```text
[DockAuto] begin entity=... request=N ... phase=stabilizing
```

Early server rejections now log explicit reasons:
- `invalid-command`
- `ship-not-found`
- `ship-not-matched-to-target-hub`
- `autopilot-authority-denied controller=...`

Therefore the next inert Automatic cannot be silent.

## Automatic async invariant

Heavy Automatic planning stays outside fixed-step:
- Autopilot authority;
- stop/stabilize;
- immutable planning snapshot;
- worker does advisory + trajectory + Accepted program;
- fixed-step polls the result only.

Never restore:
- synchronous heavy planning inside fixed-step;
- `phase=plan-retry` storms.

Entry alignment remains:
- first accepted reference owns route-entry attitude;
- physical Aligning rotates real hull when needed;
- stale program is discarded;
- stabilize/replan;
- execute only fresh program.

Expected later lifecycle:
```text
[DockAuto] request=N phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```
or direct executing if already aligned.

## Immediate Windows/MSYS2 gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Do not build/run if verify fails.

If full verify passes:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

## Exact live test

Do NOT press CALCULATE TRAJECTORY first.

1. Start in Assisted.
2. Select the docking port.
3. Press START DOCKING directly.
4. Capture all `[DockAuto]` and `[DockAdvisory]` lines.
5. Expected beginning:
```text
[DockAuto] request=N phase=route-preflight
[DockAdvisory] request=N phase=stabilizing ...
[DockAdvisory] request=N phase=settled ...
[DockAdvisory] request=N phase=planning ...
[DockAdvisory] request=N route=...
[DockAdvisory] request=N phase=handoff_wait ...
[DockAdvisory] request=N ... human_control=1
[DockAuto] request=N phase=requested ...
[DockAuto] begin ... phase=stabilizing
```
6. Then async planning/alignment/execution must follow.
7. Tunnel must visibly show several frames directly ahead of the boresight,
   then a smooth bend, then route toward dock.
8. Ship must actually move after accepted execution starts.

If it stops, use the first missing/failed lifecycle phase as the next defect.
Do not infer from the green UI button alone.

## Non-negotiable invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded error only.
- Manual and Automatic Assisted share one game-flight law.
- Visible Assisted guidance uses the same law-aware capability model.
- Physical RCS is not ordinary Assisted course authority.
- Newtonian ordinary transit cannot use precision RCS as fake main thrust.
- Terminal pose/omega are physical boundary conditions.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only target collision bypass.
- No stale program execution after entry alignment.
- No hidden corridor during Automatic.
- No synchronous Automatic planner retry loop.
- START DOCKING must not depend on a prior manual route.
- Current docking scope ends at collision-free pre-capture; latch/contact later.

## Verification status

Newest route-preflight, law-aware visible guidance, straight->arc launch geometry,
server nose-first Automatic geometry and stronger diagnostics are committed to
public `main`.

Fresh target `verify_docking.sh`, MinGW build and live cold START DOCKING
evidence for these newest edits are PENDING.
