# CONTINUE PROMPT — Elite Navigation v2 / Assisted shared flight law / docking verification

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After each state-affecting iteration:
1. update the relevant state/task/project/end-to-end MD files;
2. update CONTROL_LAW_MANEUVER_MODEL if doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Do not claim fresh code accepted without Windows/MSYS2
verify/build/live evidence.

## Flight-family doctrine

Manual Assisted is accepted by the user. Do not retune without fresh live
evidence.

Assisted:
- hull nose defines intended travel direction;
- VREL/course follows nose with finite lag, target <= about 2–3 s;
- angular rate/acceleration remain physically bounded;
- forward/reverse main owns longitudinal speed;
- automatic velocity-to-nose stabilization is the normal game-flight course
  mechanic;
- physical manoeuvre/RCS (Cobra currently 2 m/s²) is precision authority, not
  ordinary route curvature authority.

Automatic Assisted now uses the SAME
`DynamicMotionSystem::applyLocalFrameInput` law as manual Assisted through:
```text
AcceptedManeuverProgram(AssistedVelocity)
 -> TrajectoryFollower(target forward speed + bounded feedback)
 -> NavigationRuntimeControlBridge
 -> ShipControlState navigationAssistedFlightModel*
 -> DynamicMotionSystem::applyNavigationAssistedFlightModel
 -> applyLocalFrameInput
 -> fixed-step motion
```

Assisted ordinary accepted programs publish no synthetic RCS actuator segments.

Newtonian/heavy is a separate ship-motion family:
- faster, less maneuverable;
- attitude and velocity independent;
- ordinary transit doctrine:
  coast -> rotate -> main burn -> coast -> rotate/flip -> brake;
- docking should favor long mostly straight legs and large maneuvering space;
- final placement may later use stronger class-specific RCS/tugs.

Current Newtonian hard boundary:
- docking geometry uses `roundTurns = false`;
- ordinary transit may not spend precision RCS as fake lateral main thrust;
- invalid lateral route demand fails as
  `newtonian-main-engine-program-infeasible`.

A dedicated Newtonian coast/rotate/burn compiler remains future work.

Ctrl+F10 remains temporarily for development/regression. Architecture no longer
depends on the player choosing the law; later a ship descriptor may lock the
family.

## Manual docking geometry

CALCULATE TRAJECTORY requests a real hull-forward lead:
```text
max(500 m, 10 * hull length)
```

Planner may shorten a blocked lead but may not rotate it.
If the minimum forward segment is blocked:
`initial forward corridor blocked`.

The semantic launch leg is protected from generic filleting with
`initialForwardLeadActive && i == 1`.

Center cockpit boresight is accepted and should remain.

## Automatic docking async invariant

Heavy Automatic planning stays outside fixed-step:
- server takes Autopilot authority;
- stop/stabilize;
- snapshot immutable inputs;
- worker runs advisory + trajectory + accepted-program construction;
- fixed-step polls the result.

Never restore:
- synchronous heavy route/Ruckig work in fixed-step;
- `phase=plan-retry` retry storms.

Entry alignment remains:
- first accepted reference owns route-entry attitude;
- if hull differs, physical `Aligning` rotates it;
- stale aligned program is discarded;
- stabilize/replan from actual state;
- execute only fresh program.

Expected lifecycle:
```text
phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```
or direct executing when already aligned.

## Terminal angular boundary

Rotating terminal pose/omega are real physical boundary conditions.

The old angular compiler only forced terminal omega on the final sample, which
could leave too much omega error to remove in one dt and reject a feasible
trajectory.

Current invariant:
```text
|omega(t) - omega_terminal| <= alpha_max * (T - t)
```

`TrajectoryGenerator.cpp` now:
- verifies current omega is still terminal-reachable;
- projects desired omega inside the next sample's terminal-reachable cone;
- then applies the ordinary per-step angular-acceleration clamp;
- retains max angular-rate and terminal pose/omega tolerances;
- performs no final-sample snap.

Native angular regression additionally proves the penultimate omega can reach
the requested terminal omega within the last dt.

## Latest fresh Windows/MSYS2 evidence

Latest `verify_docking.sh` produced:

```text
navigation_runtime_control .......... PASS
maneuver_tracking_controller ........ PASS
docking_advisory .................... PASS
accepted_maneuver_program_builder ... PASS
trajectory_generator_angular ........ PASS
manual docking static contract ...... PASS
```

This is fresh target evidence that:
- Assisted shared-flight runtime/native gates are green;
- nose-first docking advisory regression is green;
- accepted-program Builder split is green;
- terminal-angular reachability is green.

The verify then failed ONLY in `check_automatic_docking.py` because the static
checker mistakenly required the test message
`angular planner deferred terminal omega correction to the final sample`
inside production `TrajectoryGenerator.cpp`.

That checker bug is fixed:
- production file is checked for `remainingBefore`, `remainingAfter`,
  `maxTerminalDelta`;
- `TrajectoryGeneratorAngularTests.cpp` is checked for
  `penultimate.angularVelocityRadPerSecond`, `maxAlpha * terminalDt`, and the
  anti-snap assertion message.

No production navigation/physics code changed after the green native run.

## Immediate target gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Expected result: FULL `[DOCK-VERIFY] PASS`.

If verify passes, immediately:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

If verify fails again, fix the first real failure. Do not weaken accepted native
gates or angular tolerances merely to satisfy a static token checker.

## Live Assisted acceptance after full verify/build

CALCULATE TRAJECTORY:
- temporary Autopilot stop/stabilize;
- route/tunnel remains after Human handback;
- first tunnel section starts along boresight/nose.

START DOCKING:
- no old fixed-step freeze;
- route/tunnel stays visible;
- async plan completes;
- optional physical align/replan;
- fresh plan reaches `phase=executing`;
- ship physically follows route.

The former
`accepted-program-propulsion-program-infeasible`
must not occur merely because Assisted needs ordinary course change.

If Automatic cancels/replans, capture every `[DockAuto]` line and exact reason.

## Non-negotiable invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded error; it is not a second planner.
- Manual and Automatic Assisted share the same game-flight law.
- Physical RCS is not ordinary Assisted course authority.
- Newtonian ordinary transit cannot spend precision RCS as fake main thrust.
- Terminal pose/omega are physical angular boundary conditions.
- ShipDynamics/descriptor owns real speed/load/angular/propulsion limits.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only target collision bypass.
- No execution of stale program after entry alignment.
- No hidden corridor during Automatic.
- No synchronous Automatic planning/retry loop.
- Current docking scope ends at collision-free pre-capture; contact/latch later.

## Verification status

Native target gates listed above are GREEN.

The corrected automatic static checker and complete `verify_docking.sh` rerun
are PENDING. Full MinGW build and live Assisted docking are also PENDING.
