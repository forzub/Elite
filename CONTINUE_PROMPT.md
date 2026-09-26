# CONTINUE PROMPT — Elite Navigation v2 / docking tunnel + Automatic execution

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

After every state-affecting result update those files and REGENERATE THIS PROMPT
FROM SCRATCH again. Commit fixes directly to GitHub `main`; do not send patch
files to the user.

## Accepted baseline

Assisted manual flight is ACCEPTED by the user. Do not retune it unless new live
evidence explicitly shows a new defect.

Assisted doctrine:
- approximately airplane-like nose/course coupling;
- course follows hull direction within roughly 2–3 s for a substantial turn;
- automatic lateral stabilization may use the configured Assisted authority;
- manual keypad RCS remains the small physical gas-limited manoeuvre thruster;
- no direct velocity rewrite;
- shared linear-load envelope remains authoritative.

## Latest live evidence

User ran the game after the previous fixes.

Observed:
- Assisted: good, accepted.
- manual docking mode: previously successful tunnel/corridor disappeared.
- START DOCKING: route disappeared, one heavy freeze occurred, ship did not move.
- server log:
  `[DockAuto] request=2 phase=plan-failed reason=accepted-program-build-failed action=restore-human`.
- old repeated `phase=plan-retry` storm is gone.

User requirement:
- manual route must show the successful tunnel again;
- Automatic must also keep showing the route/tunnel;
- if a route has already been calculated for the same dock, Automatic may reuse
  it instead of recalculating presentation geometry;
- Autopilot should physically fly the route after stabilization/planning.

## Fixes now on main

### Route/tunnel presentation

Control ownership and presentation are separate.

`SystemMapRenderer::applyDockingAction` enables:
- RoutePlanning
- LocalGuidance
- HudGuidanceCorridor

for both Guidance and Automatic.

Automatic route selection may resolve the same retained dock corridor.

`SpaceState::updateDockingAdvisory` no longer clears an already calculated
corridor merely because START DOCKING takes server Autopilot authority. On
server hand-back, a retained route is returned to Guidance ownership instead of
being erased.

Architecture regression:
`tests/architecture_contracts/check_automatic_docking.py` pins visible corridor
ownership across Automatic.

### Accepted-program diagnostics

`AcceptedManeuverProgramBuilder::Result` exposes `failureReason`.

Automatic server failures now preserve the exact builder reason, e.g.:
- `accepted-program-angular-kinematics-infeasible`
- `accepted-program-propulsion-program-infeasible`

Do not collapse this back to generic `accepted-program-build-failed`.

### Rotating terminal attitude fix

Root mismatch:
- GameServer computed non-zero angular velocity for the rotating docking port;
- AcceptedManeuverProgramBuilder required that terminal omega;
- TrajectoryGenerator previously authored orientation toward one STATIC terminal
  quaternion;
- this could create a physically impossible last-sample omega jump, which the
  builder correctly rejected.

Current contract:
- `TrajectoryGenerationRequest` carries
  `hasTerminalAngularVelocity` and
  `terminalAngularVelocityRadPerSecond`;
- GameServer feeds the rotating-port omega into TrajectoryGenerator;
- terminal orientation is time-varying near capture: it is propagated backwards
  from the exact final pose using the terminal omega;
- final pose remains exact;
- AcceptedManeuverProgramBuilder still independently rejects angular states that
  exceed ship capability.

Do NOT solve this by weakening angular feasibility checks.

Architecture regression now pins the server -> trajectory terminal-spin handoff.

## Automatic lifecycle

```text
START DOCKING
 -> server takes Autopilot authority
 -> BrakeToStop / stabilize
 -> one heavy plan for the stabilized state
 -> trajectory includes moving terminal velocity + rotating terminal attitude
 -> AcceptedManeuverProgram
 -> optional physical hull Aligning
 -> if aligned state changed: discard stale program, stabilize, replan once
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> shared physics
 -> collision-free pre-capture completion
 -> Human hand-back
```

Initial plan failure is one-shot. Never restore the old synchronous fixed-step
retry timer/loop.

Current success scope ends at pre-capture outside solid station geometry.
Physical latch/contact remains a later layer.

## Immediate Windows verification

From MSYS2:

```bash
cd /d/__elite/work
git pull --ff-only origin main
git log -1 --oneline

python tests/architecture_contracts/check_automatic_docking.py

cmake --build build/tests/navigation_runtime \
  --target accepted_maneuver_program_builder_tests \
  -j 8

ctest --test-dir build/tests/navigation_runtime \
  -R "^accepted_maneuver_program_builder$" \
  --output-on-failure

bash verify_docking.sh
bash build_mingw64.sh
```

If green:

```bash
build/EliteGame.exe
```

Live gate:
1. CALCULATE TRAJECTORY.
2. Confirm both route line and guidance tunnel/corridor are visible.
3. Without discarding that route, press START DOCKING.
4. Route/tunnel must remain visible.
5. Ship may stop/stabilize first, then must begin physical flight if planning is
   accepted.
6. Capture every `[DockAuto]` line.
7. There must be no repeating `phase=plan-retry`.
8. If plan still fails, use the new exact reason as the next fix target.
9. If it executes, continue through optional align/replan and pre-capture.

## Architecture invariants

Canonical execution remains:

```text
trajectory + proof
 -> AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> ShipControlState navigationActuatorProgram*
 -> DynamicMotionSystem::applyNavigationActuatorProgram
 -> shared fixed-step physics
```

Planner owns nominal actuator schedule.
Follower owns bounded correction.
Physics owns installed hardware, load envelope, gas, speed and collision.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- planner-only target collision bypass;
- Automatic fixed-step `plan-retry` loop;
- hidden/disabled corridor presentation during Automatic;
- direct velocity rewrites.
