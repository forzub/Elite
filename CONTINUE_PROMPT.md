# CONTINUE PROMPT — Elite Navigation v2 / isolate final runtime-control failure

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result update those files and regenerate this prompt
from scratch.

## Latest target-machine evidence

Canonical MinGW build: PASS.

Focused Automatic/runtime tests:
- `maneuver_tracking_controller` PASS;
- `docking_advisory` PASS;
- `accepted_maneuver_program_builder` PASS;
- `navigation_runtime_control` FAIL.

Do NOT call Automatic docking accepted yet. The implementation compiles and the
planner/program/tracking layers are green, but the final execution/control seam
still has one failing native contract.

The user-provided CTest summary did NOT include the actual failing assertion.
Do not guess and do not weaken production behavior to satisfy an unknown failure.

## Immediate command

Run only the failing test:

```bash
cd /d/__elite/work
ctest --test-dir build/tests/navigation_runtime \
  -R "^navigation_runtime_control$" \
  --output-on-failure
```

Use the FIRST actual assertion/error line as the next fix target.

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
Physics owns physical capability, speed/load/gas/collision enforcement.

Do not restore:
- `TrajectoryFollower(AcceptedShortSegment)`;
- retired `SystemMapRenderer::m_mode`;
- `CoordinateDisplayService::cycle()`;
- planner-only target collision bypass in Automatic docking.

Automatic lifecycle remains:
stabilize -> plan -> optional physical Aligning -> discard stale program ->
stabilize/replan -> execute accepted program -> controlled replan on failure ->
collision-free pre-capture completion -> Human handback.

Current terminal success is pre-capture only. Physical latch/contact remains a
separate next layer.

Commit fixes directly to GitHub `main`. No patch files.
