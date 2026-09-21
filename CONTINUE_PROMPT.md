# CONTINUE PROMPT — Elite Navigation: minimum-cant Newtonian attitude

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those files first, then inspect:
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `src/game/navigation/DynamicMotionSystem.cpp`;
- `src/game/navigation/ManeuverTrackingController.cpp`;
- `src/game/ship/ShipController.cpp`;
- `src/world/navigation/TrajectoryGenerator.cpp`.

## Latest verified behavior

Scalar path-progress correction substantially improved the reference geometry.

Latest wall-case perf includes one scalar solve, `guide_points=10`, and
`min_speed_mps=max_speed_mps=10.0000`.

Remaining user-visible problem:
the physical hull rotates dramatically broadside to the direction of travel during a
gentle Newtonian turn.

## Root cause

The old reference-attitude policy was:
```cpp
if (law == Newtonian && acceleration > 0.35)
    requestedForward = normalize(acceleration);
```

On a constant-speed turn the acceleration is centripetal, so this commands roughly a
90-degree nose-to-velocity separation even when RCS alone can provide the needed lateral
acceleration.

The lower physical allocator already has the correct decomposition seam:
- main engine = positive longitudinal along hull forward in Newtonian;
- residual = bounded manoeuvre/RCS vector.

Therefore the reference attitude should not point the main engine at the full
acceleration vector by default.

## Current candidate

Commit `fcffa5bae3b4e3deab5d6f043d3c500137719dad` introduces minimum-cant
Newtonian attitude:
- velocity tangent is default nose direction;
- if |requested acceleration| <= manoeuvre/RCS authority, no main-engine cant;
- otherwise rotate only the minimum angle needed so a positive main-thrust ray plus the
  bounded RCS sphere can reproduce requested acceleration.

Commit `b943064d529935243863c30238ae0daf62f1c129` adds:
```text
MAX REFERENCE/VELOCITY ANGLE
```
alongside the existing actual `MAX BODY/VELOCITY ANGLE`.

Do not call this target-accepted until user MinGW64 viewer evidence confirms it.

## Next commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Start with Expert / Standard / Newtonian.

Viewer:
- yellow = actual velocity;
- red = reference nose;
- cyan = physical nose;
- purple = calculated path;
- green = actual path;
- NAV REV must be visible.

Expected:
- red no longer swings broadside merely because the curve has centripetal acceleration;
- cyan follows red with a modest turn;
- for this gentle 10 m/s stand, reference/velocity angle should be much smaller than the
  previous near-90/180-degree behavior.

If red is good but cyan is bad, investigate angular tracker/control-axis execution.
If red is still bad, fix reference attitude allocation.

Do not enable dynamic avoidance yet.
