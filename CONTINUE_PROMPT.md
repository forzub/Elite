# CONTINUE PROMPT — Elite Navigation v2 / live Assisted + Automatic docking fixes

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`

After every state-affecting result update those files and regenerate this prompt
from scratch again.

## Latest live evidence

Manual `CALCULATE TRAJECTORY`: route appeared successfully.

Manual Assisted:
- user reports course changes are far too slow;
- requirement: Assisted is almost airplane-like: where the nose points, travel
  direction should follow with only a short lag, normally no more than ~2–3 s.

Automatic `START DOCKING`:
- hub map began freezing periodically and game field effectively stalled;
- log showed repeated `[DockAuto] ... phase=plan-retry`;
- every retry coincided with ~280–300 ms fixed-simulation work;
- root issue was a synchronous heavy Planner call hammered from fixed-step
  every ~0.5 s after plan failure.

## Implemented fixes on main

### Assisted course coupling

Old defect:
- after a sharp turn, longitudinal main demand was allocated first;
- it could consume the entire shared linear-load envelope;
- automatic lateral stabilization then had zero authority to remove old sideways
  VREL;
- FA-on therefore behaved too much like Newtonian inertia.

Current rule:
- manual keypad RCS remains the small gas-limited physical RCS;
- automatic Assisted lateral stabilization is separate;
- in a sharp Assisted turn, cancelling old sideways VREL has priority;
- longitudinal main thrust consumes remaining load authority;
- total command remains inside the shared linear-load envelope;
- no direct velocity rewrite.

Current Cobra:
- automatic Assisted lateral authority = 73.549875 m/s^2 (7.5 g linear envelope);
- manual RCS remains 2 m/s^2 and gas limited.

New native regression:
`testAssistedCourseRealignsWithinThreeSeconds`
- initial VREL = +X 100 m/s;
- hull/nose = -Z (90-degree change);
- target forward speed = 100 m/s;
- after 3 s, course error must be <=5 degrees;
- ship must not simply stop.

Newtonian behavior is unchanged.

### Automatic docking fixed-step retry storm

Removed:
- `nextPlanAttemptUniverseTimeSeconds`;
- `phase=plan-retry`;
- automatic 0.5-second synchronous retry loop.

Current behavior:
- stabilize;
- perform one Automatic plan for the stabilized state;
- if it fails:
  - store concrete `lastPlanFailureReason`;
  - log
    `[DockAuto] ... phase=plan-failed reason=<...> action=restore-human`;
  - terminate that Automatic request;
  - restore Human authority.

This is deliberate because the current Planner is synchronous and expensive.
Do not reintroduce repeated fixed-step planning.

Recoverable execution tracking failure may still:
- controlled stop/stabilize;
- replan once from the new physical state.
If that plan fails, hand back instead of retry storm.

## Immediate verification

On Windows/MSYS2:

```bash
cd /d/__elite/work
git pull --ff-only origin main
git log -1 --oneline

bash verify_modes.sh
bash verify_docking.sh
bash build_mingw64.sh
```

If green:

```bash
build/EliteGame.exe
```

Live checks:
1. Assisted: make a substantial pitch/yaw course change while moving. The
   velocity/course should visibly follow the nose quickly rather than drifting
   for many seconds.
2. Confirm `CALCULATE TRAJECTORY` still works.
3. Press `START DOCKING`.
4. There must be NO repeating `phase=plan-retry`.
5. If planning fails, capture the single
   `phase=plan-failed reason=...` line. That exact reason is the next docking
   fix target.
6. If planning succeeds, capture all `[DockAuto]` lines through
   stabilize -> optional aligning/aligned-replan -> executing -> pre-capture
   completion.

Current Automatic success remains pre-capture only; physical latch/contact is a
later docking layer.

## Architecture invariants

Accepted navigation execution remains:

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
Physics owns installed hardware, load envelope, speed, gas and collision.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- planner-only target collision bypass;
- `SystemMapRenderer::m_mode`;
- `CoordinateDisplayService::cycle()`;
- Automatic fixed-step `plan-retry` loop.

Commit fixes directly to GitHub `main`. Do not provide patch files.
