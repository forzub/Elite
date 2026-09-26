# CONTINUE PROMPT — Elite Navigation v2 / Assisted shared flight law / docking verification

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update the relevant state/task/project/end-to-end MD files;
2. update CONTROL_LAW_MANEUVER_MODEL if flight doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to GitHub `main`.

Do not send patch files. Do not claim new behavior accepted without fresh
Windows/MSYS2 verify/build/live evidence.

## Flight doctrine

Manual Assisted is accepted. Do not retune without fresh live evidence.

Assisted:
- hull nose defines desired travel direction;
- VREL/course follows nose with finite lag, target about <=2–3 s;
- angular rate/acceleration remain bounded;
- forward/reverse main controls longitudinal speed;
- automatic velocity-to-nose stabilization is the normal game-flight course
  mechanic;
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

Ordinary Assisted programs publish zero synthetic RCS actuator segments.

Newtonian/heavy is a separate motion family:
- faster, less maneuverable;
- velocity independent from attitude;
- ordinary doctrine: coast -> rotate -> main burn -> coast -> rotate/flip ->
  brake;
- docking favors long, nearly straight legs and large maneuvering space;
- final placement may later use stronger class-specific RCS/tugs.

Current Newtonian boundary:
- docking geometry sets `roundTurns = false`;
- ordinary transit cannot spend precision RCS as fake lateral main thrust;
- invalid lateral route demand fails as
  `newtonian-main-engine-program-infeasible`.

Dedicated Newtonian coast/rotate/burn compiler is future work.

## Manual docking geometry

CALCULATE TRAJECTORY requests a hull-forward lead:
```text
max(500 m, 10 * hull length)
```

Planner may shorten a blocked lead but may not rotate it.
Minimum blocked forward segment fails as:
`initial forward corridor blocked`.

The semantic first leg is protected from generic filleting by
`initialForwardLeadActive && i == 1`.

Center cockpit boresight is accepted and should remain.

## Terminal angular boundary

Rotating target pose and omega are physical boundary conditions.

Current angular invariant:
```text
|omega(t) - omega_terminal| <= alpha_max * (T - t)
```

`TrajectoryGenerator.cpp`:
- checks current omega remains terminal-reachable;
- projects desired omega into the next sample's terminal-reachable cone;
- applies ordinary per-step angular acceleration clamp;
- keeps max angular-rate and terminal pose/omega tolerances;
- never performs a final-sample snap.

Native regression checks the penultimate omega can reach terminal omega within
the final dt.

## Latest fresh Windows evidence

The newest target run is GREEN for all native gates:

```text
navigation_runtime_control .......... PASS
maneuver_tracking_controller ........ PASS
docking_advisory .................... PASS
accepted_maneuver_program_builder ... PASS
trajectory_generator_angular ........ PASS
manual docking static contract ...... PASS
```

Two subsequent failures were static-checker bugs only.

First checker bug:
- required the anti-snap assertion message inside production
  `TrajectoryGenerator.cpp`;
- fixed by checking production reachability tokens in the production file and
  the assertion in `TrajectoryGeneratorAngularTests.cpp`.

Second checker bug:
```text
missing token:
trajectoryRequest.hasTerminalAngularVelocity = true
```

Production server code was manually verified correct:
```cpp
trajectoryRequest.
    hasTerminalAngularVelocity = true;
trajectoryRequest.
    terminalAngularVelocityRadPerSecond =
        terminalAngularVelocityMapRadPerSec;
```

The same terminal omega is passed to
`AcceptedManeuverProgramBuilder::Request`.

Root cause was exact-whitespace matching in the static checker.

Current checker fix:
- terminal angular handoff uses semantic tokens;
- `check_automatic_docking.py::require()` first checks raw text, then a
  whitespace-compacted representation;
- source formatting/line wrapping alone can no longer trigger this class of
  false architecture failure.

No production navigation/physics code changed after the native gates went green.

## Automatic docking async invariant

Heavy Automatic planning stays outside fixed-step:
- server takes Autopilot authority;
- stop/stabilize;
- immutable planning snapshot;
- worker performs advisory + trajectory + accepted-program construction;
- fixed-step polls only.

Never restore:
- synchronous heavy route/Ruckig work in fixed-step;
- `phase=plan-retry` storms.

Entry alignment remains:
- first accepted reference owns route-entry attitude;
- physical Aligning rotates real hull if needed;
- stale aligned program is discarded;
- stabilize/replan from actual state;
- only fresh program executes.

Expected live lifecycle:
```text
phase=planning-async
planned ... phase=aligning
phase=aligned-replan
phase=planning-async
planned ... phase=executing
```
or direct executing when already aligned.

## Immediate target gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Expected:
```text
[DOCK-VERIFY] PASS
```

If verify passes:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Then test live Assisted docking and capture all `[DockAuto]` lines.

## Live acceptance

CALCULATE TRAJECTORY:
- Autopilot stops/stabilizes;
- route/tunnel remains after Human handback;
- first tunnel section starts along boresight/nose.

START DOCKING:
- no old fixed-step freeze;
- tunnel remains visible;
- async plan completes;
- optional physical align/replan;
- fresh plan reaches `phase=executing`;
- ship physically follows route.

The former
`accepted-program-propulsion-program-infeasible`
must not occur merely because Assisted needs normal course change.

## Invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded tracking error only.
- Manual and Automatic Assisted share one game-flight law.
- Physical RCS is not ordinary Assisted course authority.
- Newtonian ordinary transit cannot use precision RCS as fake main thrust.
- Terminal pose/omega are real angular boundary conditions.
- ShipDynamics/descriptor owns speed/load/angular/propulsion limits.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only collision bypass.
- No stale program execution after physical alignment.
- No hidden corridor during Automatic.
- No synchronous Automatic planning/retry loop.
- Current docking scope ends at collision-free pre-capture; latch/contact later.

## Verification status

Fresh native target gates are GREEN.

The whitespace-hardened automatic static checker and complete
`verify_docking.sh` rerun are PENDING.
Full MinGW build and live Assisted docking are PENDING.
