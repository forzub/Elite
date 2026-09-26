# CONTINUE PROMPT — Elite Navigation v2 / shared Assisted flight law + terminal angular reachability

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update state/task/project/end-to-end MD files as appropriate;
2. update CONTROL_LAW_MANEUVER_MODEL if doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Do not claim target acceptance without fresh
Windows/MSYS2 test/build/live evidence.

## Accepted flight doctrine

Manual Assisted is accepted by the user. Do not retune without new live
evidence.

Assisted:
- nose defines intended travel direction;
- VREL/course follows nose with finite lag, target <= about 2–3 s;
- real hull angular rate/acceleration limits still apply;
- forward/reverse main owns longitudinal speed;
- automatic velocity-to-nose stabilization is a game-flight capability;
- physical manoeuvre/RCS (Cobra: 2 m/s²) is precision authority, NOT ordinary
  route curvature authority.

Automatic Assisted now reuses the SAME `DynamicMotionSystem::applyLocalFrameInput`
law as manual Assisted.

Canonical Assisted chain:
```text
trajectory/reference
 -> AcceptedManeuverProgram(TranslationMode::AssistedVelocity)
 -> TrajectoryFollower(target forward speed + bounded feedback/angular intent)
 -> NavigationRuntimeControlBridge
 -> ShipControlState navigationAssistedFlightModel*
 -> DynamicMotionSystem::applyNavigationAssistedFlightModel
 -> DynamicMotionSystem::applyLocalFrameInput
 -> fixed-step physics
```

Assisted accepted programs publish zero synthetic RCS actuator segments.

## Newtonian split

Design direction is two materially different ship families, not one route
algorithm with a cosmetic control switch.

Newtonian/heavy:
- faster, less maneuverable;
- attitude and velocity independent;
- ordinary strategy: coast -> rotate -> main burn -> coast -> rotate/flip ->
  braking burn;
- docking should favor long, mostly straight legs and large turn space;
- final placement may later use stronger class-specific RCS and/or tugs.

Current hard boundary:
- Newtonian docking geometry requests `roundTurns = false`;
- ordinary Newtonian Accepted programs may not spend precision RCS as route
  thrust;
- invalid lateral route demand fails as
  `newtonian-main-engine-program-infeasible`.

Dedicated Newtonian coast/rotate/burn compiler is still future work.

The Ctrl+F10 law switch remains temporarily for development/regression. The
architecture no longer depends on this being a player preference; later ship
descriptors may lock the family.

## Manual nose-first corridor

Manual CALCULATE TRAJECTORY requests a real hull-forward lead:
```text
max(500 m, 10 * hull length)
```

Planner may shorten a blocked lead but may not rotate it. If the minimum forward
segment is blocked, fail with `initial forward corridor blocked`.

The first semantic launch leg is protected from generic filleting via the
`initialForwardLeadActive && i == 1` branch.

Center cockpit boresight remains accepted and should stay.

## Latest fresh Windows evidence

The newest user run reached the rotating-terminal gate:

```text
trajectory_generator_angular ..... FAILED
TRAJECTORY GENERATOR ANGULAR TESTS: FAIL:
rotating-terminal trajectory failed:
angular trajectory cannot reach requested terminal state
```

This occurs after the earlier navigation-runtime native gates complete.

## Angular-gate diagnosis

The rotating-terminal test is valid and must NOT be weakened.

Fixture:
- initial forward = -Z;
- terminal forward = +X;
- max angular velocity = 1.0 rad/s;
- max angular acceleration = 0.5 rad/s²;
- requested terminal angular velocity = (0, 0.2, 0) rad/s;
- path length = 200 m;
- terminal orientation blend = 180 m;
- final pose tolerance = 5 degrees;
- terminal omega tolerance = 0.05 rad/s.

The old `compileBoundedAngularKinematics` bounded forward per-sample angular
acceleration but only replaced `targetOmega` with terminal omega on the final
sample. A feasible trajectory could therefore reach the penultimate sample with
too much rate error to remove in the last dt, then reject itself.

## Current angular fix on main

Terminal omega is now a real backwards-reachable boundary condition.

For every intermediate state:
```text
|omega(t) - omega_terminal| <= alpha_max * (T - t)
```

Implementation:
- before each step, verify current omega is still terminal-reachable;
- clamp desired omega into the next sample's terminal-reachable ball;
- then apply the ordinary per-step `alpha_max * dt` clamp;
- keep the existing max angular-rate clamp;
- do NOT snap orientation or omega;
- do NOT relax angular limits or terminal tolerances.

The angular test now additionally checks the penultimate sample:
```text
|omega_penultimate - omega_terminal| <= alpha_max * final_dt
```
so final-sample snapping cannot return unnoticed.

Current code commits include:
- `1b39aedbe2fe0366de745734443946e36134d5a4`
  `nav: reserve angular authority for terminal omega`
- `b2890c5a00db6676f734a87b26ddab1186b9981e`
  `test: forbid final-sample angular velocity snap`

Static automatic-docking contract also pins the terminal reachability logic.

## Automatic docking async invariant

Heavy Automatic planning remains outside fixed-step:
- take Autopilot authority;
- stop/stabilize;
- snapshot immutable inputs;
- worker performs advisory + trajectory + accepted-program construction;
- fixed-step polls atomic result only.

Never restore:
- synchronous heavy planning in fixed-step;
- retry storms / `phase=plan-retry`.

Existing route-entry alignment remains:
- accepted first reference owns route-entry attitude;
- physical Aligning rotates real hull if needed;
- stale aligned program is discarded;
- server stabilizes/replans;
- only fresh program executes.

Expected lifecycle:
```text
phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```
or direct executing when already aligned.

## Immediate Windows/MSYS2 gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Do NOT build/run the game if verify fails.

First required result:
```text
trajectory_generator_angular ... Passed
```

If angular still fails:
- determine whether the remaining failure is terminal orientation or terminal
  angular velocity;
- add precise diagnostics if necessary;
- do NOT widen 5 degree / 0.05 rad/s tolerances;
- do NOT increase vehicle angular capability.

After complete `verify_docking.sh` PASS:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

## Live Assisted acceptance after verify/build

CALCULATE TRAJECTORY:
- temporary Autopilot stabilization;
- retained route/tunnel after Human handback;
- first tunnel section along boresight/nose.

START DOCKING:
- no old fixed-step planning freeze;
- tunnel remains visible;
- async plan completes;
- optional physical align/replan;
- fresh plan reaches `phase=executing`;
- ship physically follows route.

The old
`accepted-program-propulsion-program-infeasible`
must not be produced merely because Assisted needs normal course change.

## Invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded tracking error only.
- Manual and Automatic Assisted use the same game-flight law.
- Physical RCS is not ordinary Assisted lateral route authority.
- Newtonian ordinary transit cannot use precision RCS as fake main thrust.
- Angular terminal pose/omega are physical boundary conditions, never final
  sample snaps.
- ShipDynamics/descriptor owns real speed/load/angular/propulsion limits.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only collision bypass.
- No stale program execution after physical alignment.
- No hidden tunnel during Automatic.
- No synchronous Automatic planning/retry loop.
- Current docking scope ends at collision-free pre-capture; contact/latch later.

## Verification status

The terminal-angular fix and regression are committed to public `main`.
Fresh target-machine rerun is PENDING. Do not call it accepted until
`trajectory_generator_angular` and the full `verify_docking.sh` are green.
