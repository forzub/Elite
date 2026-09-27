# CONTINUE PROMPT — Elite Navigation v2 / live Automatic angular-timing gate

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
2. update CONTROL_LAW_MANEUVER_MODEL when doctrine changes;
3. REGENERATE THIS CONTINUE_PROMPT FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Fresh Windows/MSYS2 verify/build/live evidence is
required before claiming newest behavior accepted.

## User-locked manual docking presentation

This is now a hard user contract. Do NOT alter without an explicit request:

```text
ordinary manual docking corridor frames: 500 m
terminal/final-zone frames:             250 m
```

No special 125 m launch cadence.
No arbitrary fractional transition frame just to land exactly on the dense-zone
boundary.

Current implementation:
- SpaceState sets `request.gateSpacingMeters = 500.0`;
- SpaceState sets `request.terminalGateSpacingMeters = 250.0`;
- DockingAdvisoryPlanner has no launch-only cadence override;
- terminal cadence starts up to one normal interval early so the final dense
  zone remains covered without a fractional transition step;
- static contract rejects return of SpaceState 150 m cadence or
  `distanceToActivation` transition logic;
- native docking test requires first published launch gap ~= 500 m.

The nose-first geometry itself may still use dense INTERNAL samples for
collision/curvature proof. Only published corridor frames are locked to
500/250.

Normal nose-first lead is currently at least 1000 m so one full 500 m straight
published interval can remain on the hull axis while the remaining lead can
blend into a tangent first arc.

## Flight-family doctrine

Manual Assisted is accepted. Do not retune it without fresh live evidence.

Assisted:
- hull nose defines desired travel direction;
- VREL/course follows nose with finite lag, target about <=2–3 s;
- real angular rate/acceleration limits still apply;
- forward/reverse main controls longitudinal speed;
- automatic velocity-to-nose stabilization is ordinary Assisted course
  authority;
- physical manoeuvre/RCS (Cobra currently 2 m/s²) is precision authority, NOT
  ordinary route-curvature authority.

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

Newtonian/heavy is a separate family:
- faster, less maneuverable;
- inertial velocity independent from attitude;
- ordinary doctrine: coast -> rotate -> main burn -> coast -> flip/rotate ->
  braking burn;
- docking favors long nearly straight legs and large turn volume;
- precision RCS must not be used as fake sustained lateral main thrust.

Dedicated Newtonian coast/rotate/burn compiler remains future work.

## Controller ownership / NPC analogy

Do NOT create a second NPC ship entity for player Autopilot.

Correct model already exists:
```text
same Ship entity
Human controller -> Autopilot controller
same ShipDynamics / installed hardware / fixed-step physics
```

`ControlRegistry::takeAutopilotControl()` changes the command source. AI/NPC
and Autopilot may later share higher-level controller abstractions, but physics
ownership remains on the same Ship.

## Latest live evidence

User now has this real Automatic server failure:

```text
[DockAuto] request=1 phase=plan-failed
reason=trajectory:angular trajectory cannot reach requested terminal state
action=restore-human
```

The ship first displayed/calculated a route but did not move.

This proves:
- UI request reached Automatic;
- server acquired/used Autopilot authority;
- server reached the Planner;
- the safe Human hand-back occurred because no executable trajectory passed the
  angular gate.

So the current live blocker is NOT controller handoff.

The visible/advisory route is not the same acceptance level as the executable
server program. A visible route can exist while the server trajectory is
rejected before AcceptedManeuverProgram / Follower execution.

## Current angular-timing fix on main

Root defect: translational Ruckig chose the fastest collision-free trajectory
clock. Bounded angular compilation then had to fit exact terminal orientation
and rotating-target terminal omega into that same clock. If the hull needed
more time, Planner rejected the whole task.

Correct behavior now:
- retain the SAME collision-free route;
- retain the SAME angular capability/tolerances;
- lower translation speed until the hull has enough time.

For multi-point routes, only an angular-terminal failure triggers retries using:

```text
1.00, 0.80, 0.64, 0.50, 0.40, 0.32, 0.25,
0.20, 0.16, 0.125, 0.10, 0.08, 0.06, 0.05
```

A successful slowed route reports:
`angular-speed-relaxed`.

No collision rule, max angular speed, max angular acceleration, terminal pose
tolerance or terminal-omega tolerance is weakened.

New native regression:
`testTranslationSlowsWhenAngularTerminalNeedsMoreTime`
creates a translationally-fast 3-point route whose hull rotation needs more
time and requires Planner to return a slower valid trajectory.

## Exact angular diagnostics

`compileBoundedAngularKinematics` now returns detailed reasons rather than
only a generic failure. Possible live suffixes include:

- `too-few-angular-samples`
- `invalid-angular-capability`
- `initial-omega-outside-capability omega=... max=...`
- `invalid-angular-step sample=N dt=...`
- `terminal-omega-unreachable-before-sample=N error=... reachable=...`
- `terminal-orientation-error-deg=...`
- `terminal-omega-error=...`

If live planning still fails, use the exact new suffix. Do NOT loosen angular
limits/tolerances by guess.

## Docking-port click priority

Latest user feedback: distant docking ports were hard to select; clicking near
them often selected the entire parent construction.

Root cause:
- docking items had a high `pickPriority`;
- but when `hitPolygonPx` existed it replaced the normal hit radius;
- at distant zoom the real opening projected to only a few pixels;
- overlay miss then fell through to `pickHubInfrastructureBody()`, which
  selected the assembly mesh.

Current behavior:
- a DockingPort is hit by precise physical polygon OR semantic screen radius;
- dock `hitRadiusPx = 22.0`;
- dock `pickPriority = 1000`;
- among overlapping dock markers, nearest cursor distance wins
  (`nearerDock`);
- infrastructure triangle pick runs only when overlay picking did not consume
  the press.

`verify_docking.sh` now also runs
`tests/system_map/check_object_overlay.py` so this selection contract is part
of docking verification.

## Self-contained START DOCKING

CALCULATE TRAJECTORY is not a prerequisite.

Cold Automatic flow:
```text
START DOCKING
 -> route-preflight if no matching visible corridor
 -> temporary authoritative BrakeToStop / settle
 -> authoritative Hub snapshot
 -> law-aware advisory route / visible tunnel
 -> temporary Human hand-back
 -> BeginAutomaticDocking
 -> server Autopilot stabilization
 -> async server planning
 -> AcceptedManeuverProgram
 -> Follower / RuntimeControlBridge
 -> shared Ship physics
```

The client preflight route is presentation/safe-start preparation only. Server
Automatic still builds/proves the executable program.

Server Automatic uses actual stopped hull forward and a nose-first lead. Its
internal execution-guide sampling may remain denser than the user-visible
500/250 corridor.

## Automatic async invariant

Heavy planning stays outside fixed-step:
- immutable planning snapshot;
- worker performs advisory + trajectory + Accepted-program construction;
- fixed-step polls result only.

Never restore:
- synchronous heavy route/Ruckig work in fixed-step;
- `phase=plan-retry` retry storms.

Entry alignment remains:
- accepted first reference owns route-entry attitude;
- if needed, physical Aligning turns the real hull;
- stale program is discarded;
- stabilize/replan;
- execute only a fresh program.

Expected successful lifecycle after current fix:
```text
[DockAuto] ... phase=planning-async
[DockAuto] planned ... phase=aligning
[DockAuto] ... phase=aligned-replan
[DockAuto] ... phase=planning-async
[DockAuto] planned ... phase=executing
```
or direct `phase=executing` if already aligned.

## Existing fresh target evidence before newest changes

Previously green on Windows:
```text
navigation_runtime_control .......... PASS
maneuver_tracking_controller ........ PASS
docking_advisory .................... PASS
accepted_maneuver_program_builder ... PASS
trajectory_generator_angular ........ PASS
manual docking static contract ...... PASS
```

Newest angular-time relaxation, strict cadence lock and dock-picking changes
still need a fresh target rerun.

## Immediate Windows/MSYS2 gate

Run:

```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Do not build/run if verify fails.

If verify is fully green:

```bash
bash build_mingw64.sh
build/EliteGame.exe
```

## Exact live test

1. In Hub map, test selecting a docking port from a relatively distant zoom.
   The dock card/marker must win instead of the whole parent assembly.
2. In Assisted, do NOT press CALCULATE TRAJECTORY first.
3. Press START DOCKING directly.
4. Capture every `[DockAuto]` and `[DockAdvisory]` line.
5. Manual/preflight visible corridor must remain 500 m ordinary / 250 m final.
6. The old generic angular plan-fail should no longer occur merely because the
   fastest translational clock is too short. Planner should slow translation.
7. Expected next real milestone is:
   `planned ... phase=aligning` or `planned ... phase=executing`, followed
   by physical ship movement.
8. If it still fails, use the exact detailed angular reason now emitted.

## Non-negotiable invariants

- manual visible docking frames = 500 m ordinary / 250 m final;
- do not change that cadence without explicit user instruction;
- Planner owns route/reference/control-law-compatible maneuver program;
- Follower closes bounded error only;
- Manual and Automatic Assisted share one game-flight law;
- physical RCS is not ordinary Assisted course authority;
- Newtonian transit cannot spend precision RCS as fake main thrust;
- terminal pose/omega are physical boundary conditions;
- translation may slow to satisfy angular feasibility, never widen angular
  capability;
- same Ship entity changes controller Human -> Autopilot;
- no direct authoritative position/velocity/orientation rewrites;
- no planner-only target collision bypass;
- no stale program execution after physical alignment;
- no hidden corridor during Automatic;
- no synchronous Automatic planner retry loop;
- START DOCKING must not require prior manual route calculation;
- current docking scope ends at collision-free pre-capture; latch/contact later.

## Verification status

Current code includes:
- strict visible 500/250 cadence with no special launch/transition densification;
- angular-time translation relaxation;
- detailed angular rejection diagnostics;
- distant dock semantic hit priority and nearest-dock arbitration;
- new native/static regressions for all of the above.

Fresh target `verify_docking.sh`, canonical MinGW build and live movement
evidence are PENDING.
