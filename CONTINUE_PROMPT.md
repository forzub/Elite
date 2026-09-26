# CONTINUE PROMPT — Elite Navigation v2 / propulsion-safe docking entry

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

After every state-affecting result:
1. update the state/task/project/end-to-end MD files;
2. REGENERATE THIS PROMPT FROM SCRATCH;
3. commit fixes directly to GitHub `main`.
Do not send patch files to the user.

## Accepted baseline

Assisted manual flight is ACCEPTED by the user. Do not retune it without fresh
live evidence.

The docking tunnel/corridor must remain visible in Manual Guidance and while
Automatic owns controls.

Automatic docking current scope ends at a collision-free pre-capture pose.
Physical contact/latch is a later game-state/physics layer.

## Latest Windows/live evidence

The client/server build succeeded.

The previous synchronous Automatic planning freeze appears fixed: the latest
START DOCKING attempts did not reproduce the old ~350 ms fixed-step/state-update
stall.

Automatic still did not begin flight. Each request ended after a short planning
period with:

```
[DockAuto] ... phase=plan-failed
reason=accepted-program-propulsion-program-infeasible
action=restore-human
```

Human control restoration is therefore working.

The user also required:
- Manual docking route must start in the direction the ship nose currently
  points, so the first tunnel section is directly ahead after returning to
  cockpit view.
- Cockpit HUD must show a fixed marker for ship nose direction: the screen
  center boresight.

## Current propulsion diagnosis

`AcceptedManeuverProgramBuilder::compilePropulsion` decomposes each trajectory
acceleration against the authored body forward vector using real hardware:
- rear/forward main;
- fore/reverse main;
- physical manoeuvre/RCS authority.

For Cobra, the physical manoeuvre thruster is 2 m/s². Do NOT substitute the
larger Assisted velocity-stabilization authority: the accepted actuator program
and `DynamicMotionSystem::applyNavigationActuatorProgram` must stay grounded in
real propulsion.

The physical angular compiler exposed an entry mismatch:
- translation could start accelerating along the route;
- body attitude could still start at the arbitrary stopped hull orientation;
- the resulting lateral residual exceeded physical RCS authority;
- Builder correctly returned `propulsion-program-infeasible`.

## Current Automatic fix

The first planned program now declares the required **route-entry attitude**:
- forward = first advisory gate direction;
- up = current hull up projected onto the new forward plane, with a safe
  orthogonal fallback;
- planned entry angular velocity = zero.

The existing server `Phase::Aligning` then has the intended job:
1. compare the real stopped hull with the first accepted reference;
2. physically rotate through normal bounded angular control;
3. on alignment, discard the now-stale program;
4. return to stabilization;
5. asynchronously replan from the real aligned state/time;
6. execute only the fresh program.

Expected live lifecycle when initial attitude differs:

```
phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```

If already aligned, direct `phase=executing` is legal.

Do not weaken AcceptedManeuverProgramBuilder to make this pass.

## Current Manual corridor fix

`DockingAdvisoryRequest` has:
- `hasInitialForward`
- `initialForward`
- `initialForwardLeadMeters`

Manual `SpaceState` planning supplies the real hull forward axis converted
into the Hub-local planning frame and requests a lead of:

```
max(500 m, 10 * hull length)
```

Planner starts the route search after that forward lead, then prepends the real
ship start so the first visible segment is nose-forward.

Obstacle policy:
- preserve the exact forward direction;
- shorten the requested lead if the ray is obstructed;
- if even the minimum safe forward segment is blocked, fail explicitly with
  `initial forward corridor blocked`;
- never silently rotate the initial segment sideways.

A native `DockingAdvisoryPlannerTests` regression checks the first published
segment against the requested nose axis.

## HUD boresight

`FlightVectorIndicatorRenderer::renderBoresight` draws a fixed optical sight at:

```
viewport.width * 0.5
viewport.height * 0.5
```

It is rendered whenever cockpit HUD is rendered (except Drone camera) and is
independent of the HudFlightVector navigation-module toggle.

It represents **hull/nose direction**, not velocity/VREL.

## Automatic async planning

Heavy Automatic planning remains outside fixed-step:
- stabilize/stop;
- snapshot immutable planning inputs;
- enter `Phase::Planning`;
- worker performs advisory + Ruckig + accepted-program construction;
- fixed-step only polls the atomic result;
- ship stays stopped while worker is pending.

Never restore:
- synchronous heavy route/Ruckig planning inside fixed-step;
- `phase=plan-retry` retry storms.

## Immediate target-machine gate

Run on Windows/MSYS2:

```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh

bash build_mingw64.sh

build/EliteGame.exe
```

Do not run an old executable if tests/build fail.

## Live acceptance

### Manual
1. Press CALCULATE TRAJECTORY.
2. Route line and tunnel remain visible.
3. Return to cockpit view.
4. Fixed center boresight is visible.
5. First tunnel frames should be directly ahead along that boresight before the
   route begins its turn.

### Automatic
1. Press START DOCKING.
2. No old planning freeze.
3. Retained route/tunnel remains visible.
4. Automatic card action remains active/bright green while the request is active.
5. Expect async planning.
6. If needed, ship physically aligns first.
7. Alignment must produce `phase=aligned-replan`, not execute the stale plan.
8. Fresh plan should reach `phase=executing`.
9. Ship then physically moves along the route.
10. The old `accepted-program-propulsion-program-infeasible` should not occur
    merely because the initial stopped hull pointed away from the route.

If it still fails, capture every `[DockAuto]` line and the exact reason.

## Architecture invariants

Canonical executable chain:

```text
trajectory + proof
 -> AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> ShipControlState navigationActuatorProgram*
 -> DynamicMotionSystem::applyNavigationActuatorProgram
 -> shared fixed-step physics
```

Ownership:
- Planner: nominal trajectory/reference + actuator schedule.
- Follower: bounded tracking correction only.
- Physics: installed hardware, load envelope, gas, speed and collision.
- Client/manual route: presentation/request state only.
- Server Automatic: control authority and execution.
- Boresight: presentation only, no control/navigation state.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- direct authoritative velocity/position/orientation rewrites;
- planner-only target collision bypass;
- fake extra RCS/Assisted authority in accepted actuator programs;
- execution of a stale program after physical alignment;
- hidden route/tunnel during Automatic;
- synchronous Automatic planning or fixed-step retry loops.

## Verification status

The newest nose-first route, entry-alignment propulsion fix, boresight and their
regression changes are committed to `main`, but fresh Windows
`verify_docking.sh`, canonical MinGW build and live flight evidence are still
pending. Do not claim these newest changes are accepted until target-machine
evidence is green.
