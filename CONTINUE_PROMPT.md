# CONTINUE PROMPT — Elite Navigation v2 / rerun corrected runtime-control gate

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result update those files and regenerate this prompt
from scratch again.

## Latest Windows evidence

Canonical MinGW game/server build: PASS.

Focused tests:
- `maneuver_tracking_controller` PASS;
- `docking_advisory` PASS;
- `accepted_maneuver_program_builder` PASS;
- `navigation_runtime_control` FAIL with:
  `remaining vector must clamp at manoeuvre-thruster authority`.

The failure was classified as a stale test expectation, not a production
physics defect.

The fixture had `maxLinearGs = 1g` and demanded:
- full 1g main acceleration;
- plus 2 m/s^2 manoeuvre/RCS.

Production correctly enforces one shared linear-load envelope:
main has priority, but main + secondary acceleration must still fit
`maxLinearGs/maxGs`.

The test is now corrected:
- rear main explicitly limited to 5 m/s^2 -> enough headroom exists and RCS may
  reach 2 m/s^2;
- a separate saturated-main case uses the full 1g envelope and requires
  secondary RCS to reduce to zero.

No production `DynamicMotionSystem` behavior was changed for this failure.

## Immediate commands

```bash
cd /d/__elite/work
git pull --ff-only origin main

cmake --build build/tests/navigation_runtime \
  --target navigation_runtime_control_tests \
  -j 8

ctest --test-dir build/tests/navigation_runtime \
  -R "^navigation_runtime_control$" \
  --output-on-failure
```

If green:

```bash
bash verify_docking.sh
```

The previous canonical game/server build already passed. Rebuild the whole game
only if a source change later requires it or before final live acceptance.

## Architecture that must not regress

Accepted-program execution:

```text
trajectory + proof
 -> AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> ShipControlState navigationActuatorProgram*
 -> DynamicMotionSystem::applyNavigationActuatorProgram
 -> fixed-step physics
```

Planner owns nominal rear-main / fore-main / manoeuvre feed-forward.
Follower owns bounded correction only.
Physics owns installed hardware, shared load envelope, speed, gas and collision.

Automatic docking lifecycle:
stabilize -> plan -> optional physical Aligning -> discard stale program ->
stabilize/replan -> execute accepted program -> controlled replan on failure ->
collision-free pre-capture completion -> Human handback.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- retired `SystemMapRenderer::m_mode`;
- `CoordinateDisplayService::cycle()`;
- planner-only target collision bypass.

Current Automatic success is pre-capture only. Physical latch/contact is the
next separate docking/game-state layer.

Commit fixes directly to GitHub `main`. Do not provide patch files.
