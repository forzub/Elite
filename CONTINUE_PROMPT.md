# CONTINUE PROMPT — Elite Navigation v2 / live Automatic docking validation

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read the newest sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result update those files and regenerate this prompt
from scratch again.

## Accepted target-machine baseline

Canonical MinGW build: PASS.

Focused Automatic/runtime gates:
- `navigation_runtime_control` PASS;
- `maneuver_tracking_controller` PASS;
- `docking_advisory` PASS;
- `accepted_maneuver_program_builder` PASS;
- `verify_docking.sh` fully PASS.

Do not re-open architecture before collecting live evidence unless the live run
shows a concrete failure.

## Current authoritative execution chain

```text
trajectory + proof
 -> AcceptedManeuverProgram
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge::stepProgram
 -> ShipControlState navigationActuatorProgram*
 -> DynamicMotionSystem::applyNavigationActuatorProgram
 -> shared fixed-step physics
```

Planner owns nominal rear-main / fore-main / manoeuvre feed-forward.
Follower owns bounded correction only.
Physics owns installed hardware, shared load envelope, speed, gas and collision.

`TrajectoryFollower` must not regain an `AcceptedShortSegment` overload.

## Automatic docking lifecycle

```text
START DOCKING
 -> server takes Autopilot authority
 -> BrakeToStop / stabilize
 -> plan trajectory + accepted program
 -> if hull attitude differs:
      Aligning via bounded angular control
      discard stale program
      stabilize
      replan from new real state/time
 -> execute accepted program
 -> controlled stabilize/replan on tracking/propulsion/frame failure
 -> collision-free pre-capture completion
 -> restore Human authority
```

Current success is PRE-CAPTURE only. Physical station contact/latch remains the
next separate game-state/physics layer.

No planner-only target collision bypass is allowed.

## Immediate live test

Launch the freshly built game:

```bash
cd /d/__elite/work
build/EliteGame.exe
```

Select a compatible/free docking port and press `START DOCKING`.

Collect every `[DockAuto]` console line plus a short description of visible
ship behavior.

Expected logs:
- `phase=stabilizing`;
- planned log with pages, trajectory duration, pre-capture depth, terminal omega,
  initial attitude error;
- either `phase=executing` directly or `phase=aligning`;
- if aligning: `phase=aligned-replan` followed by a fresh plan;
- on recoverable execution deviation: controlled `phase=replan`, not
  navigation shutdown;
- successful current-stage completion:
  `approach-complete ... reason=pre-capture-envelope-complete`;
- Human authority restored.

If live behavior disagrees with the log, trust the actual authoritative motion
and diagnose from the first divergence. Do not weaken envelopes or restore
legacy paths to make visuals appear successful.

Commit fixes directly to GitHub `main`. Do not provide patch files.
