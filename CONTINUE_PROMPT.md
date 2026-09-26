# CONTINUE PROMPT — Elite Navigation v2 / async Automatic docking live gate

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

Assisted manual flight is ACCEPTED by the user. Do not retune it without new
live evidence.

The previously successful manual docking tunnel/corridor is also restored and
must remain visible.

## Latest live evidence

Target Windows build succeeded.

Automatic docking:
- tunnel is visible again;
- START DOCKING still caused one visible freeze;
- no repeated retry storm occurred;
- requests failed before movement with:
  `phase=plan-failed reason=accepted-program-angular-kinematics-infeasible action=restore-human`;
- state-update during the plan was about 365–370 ms;
- after failure Human authority was restored, so presentation visibly returned
  to manual guidance.

## Root causes and current fixes

### 1. Automatic planning blocked fixed-step

Manual guidance already performs heavy DockingAdvisory planning on a worker from
an immutable snapshot.

Automatic previously executed:
`DockingAdvisoryPlanner -> TrajectoryGenerator/Ruckig ->
AcceptedManeuverProgramBuilder`
synchronously inside `applyAutomaticDockingControls`.

Current Automatic lifecycle adds `DockingAutomaticRuntime::Phase::Planning`:
- stabilize/stop;
- copy immutable planning inputs;
- launch heavy planning on a worker thread;
- return immediately to fixed-step;
- hold the real ship stopped while job is pending;
- poll an atomic ready flag;
- install the immutable accepted program at a short future execution epoch;
- enter Executing or Aligning.

Expected log:
`[DockAuto] ... phase=planning-async execution_t=...`
followed by
`[DockAuto] planned ... phase=executing|aligning`.

No `phase=plan-retry` loop may return.

The current worker captures no live GameSimulation pointers. NavigationWorkScheduler
remains the deterministic scheduling/budget layer for the future shared planner
worker pool.

### 2. Angular trajectory was geometric, not physical

Old mismatch:
- TrajectoryGenerator supplied quaternion samples derived from path geometry;
- AcceptedManeuverProgramBuilder inferred omega/alpha from consecutive samples;
- sharp geometric changes could imply impossible angular acceleration;
- builder correctly rejected them.

Current request supports:
- `hasInitialOrientation`, `initialForward`, `initialUp`;
- `hasInitialAngularVelocity`,
  `initialAngularVelocityRadPerSecond`;
- rotating terminal pose + terminal angular velocity.

Automatic seeds those fields from the real stabilized hull.

`compileBoundedAngularKinematics` in TrajectoryGenerator:
- starts from real hull orientation/omega;
- follows geometric desired orientation under vehicle max angular speed and
  angular acceleration;
- carries rotating-terminal feed-forward;
- rejects an unreachable terminal pose/omega instead of snapping;
- sets `Trajectory::angularKinematicsAuthored`.

AcceptedManeuverProgramBuilder preserves planner-authored omega and still checks
the resulting angular feasibility. Do NOT weaken this gate.

New native regression:
`tests/navigation_ruckig/TrajectoryGeneratorAngularTests.cpp`.
`verify_docking.sh` now configures/builds/runs
`trajectory_generator_angular`.

### 3. Docking UI state

`MapObjectOverlayRenderer` renders every `MapObjectPanelAction.active` with a
bright green fill/border/text. Dock and ship card action producers already
publish active state, so selected route/docking modes share this presentation.

Cockpit blinking docking label now uses the docking request state:
- Manual -> `cockpit.docking.manual_mode`
- Automatic -> `cockpit.docking.automatic_mode`

Automatic text was added for all current map/cockpit languages:
en, ru, zh-Hans, es, ja.

## Immediate target-machine gate

On Windows/MSYS2:

```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh

bash build_mingw64.sh

build/EliteGame.exe
```

## Live acceptance

1. CALCULATE TRAJECTORY:
   - route line visible;
   - tunnel/corridor visible;
   - selected manual/action button bright green.
2. START DOCKING:
   - Automatic button becomes bright green;
   - blinking top label changes to localized AUTOMATIC DOCKING MODE;
   - route/tunnel remains visible.
3. Planning:
   - see `phase=planning-async`;
   - game/map remains responsive; old ~350 ms fixed-step/state-update freeze must
     disappear;
   - ship stays stopped while calculation is pending.
4. Plan result:
   - expected `planned ... phase=executing` or `phase=aligning`;
   - old `accepted-program-angular-kinematics-infeasible` should be gone.
5. Execution:
   - ship physically moves along the visible route;
   - optional Aligning may consume real time/state and then replan from the new
     physical state;
   - current success scope ends at collision-free pre-capture;
   - physical contact/latch remains a later layer.
6. If planning still fails, capture all `[DockAuto]` lines and the exact new
   failure reason. One-shot failure must restore Human authority safely.

## Architecture invariants

Canonical execution:
```text
trajectory + proof
 -> AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> ShipControlState navigationActuatorProgram*
 -> DynamicMotionSystem::applyNavigationActuatorProgram
 -> shared fixed-step physics
```

Planner owns nominal reference + actuator schedule.
Follower owns bounded correction.
Physics owns installed hardware, load envelope, gas, speed and collision.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- direct velocity rewrites;
- planner-only target collision bypass;
- Automatic fixed-step `plan-retry`;
- synchronous heavy route/Ruckig planning from fixed-step;
- hidden docking corridor during Automatic;
- geometric quaternion jumps disguised as executable angular motion.
