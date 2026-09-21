# CONTINUE PROMPT — Elite Navigation: target rerun after curvature-speed correction

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those files first, then inspect:
- `src/world/navigation/TrajectoryGenerator.h/.cpp`;
- `tests/navigation_guidance/RuckigRoutePlannerTests.cpp`;
- `tools/navigation_runtime/scenario.json`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`;
- `tools/navigation_runtime/CMakeLists.txt`.

## Latest verified target evidence

The focused Ruckig binary compiled and linked.

7/8 tests passed.

Only failure:
```text
default wall shallow corners stay moving:
default wall calculated curve contains a major unnecessary braking dip
```

Therefore this was a functional trajectory failure, not compilation.

## Current diagnosis

The dense sampled C1 guide was correct directionally, but
`buildWaypointVelocities()` retained an old speed rule based on
`blendDistance`.

That quantity shrinks when guide sampling becomes denser, so adding samples to the same
geometric curve could lower the prescribed speed. This is physically invalid.

## Current unverified candidate

Commit `4dcebe77580e8abc7a0f9f4a22cec65f3ba985d5` replaces the density-dependent
turn-speed rule with circumcircle curvature:
```text
kappa = 2*|AB x BC| / (|AB| |BC| |AC|)
v_max = sqrt(a_lateral / kappa)
```

Ruckig perf now logs `min_speed_mps` and `max_speed_mps`.

The steady wall fixture was also corrected so start and finish are not hidden heading
transients:
- start velocity/forward follows first route leg at 10 m/s;
- finish velocity/forward follows last route leg at 10 m/s.

Commits:
- `5c408b76b36a86bd6b4fecacc377142d80726796`;
- `3a22a4ae0aca037df4c9b8c76759506ed8a840c6`.

## Viewer revision requirement

The viewer MUST show which source revision is running.

Implemented:
- CMake obtains `git rev-parse --short=12 HEAD`;
- viewer window title and HUD show `REV <sha>`.

Commits:
- `2c1baf4854f083c2e3e44ed16136c70237dfafcd`;
- `1e84a9cac2f26da8e2802e1b97e8582f869f218d`.

## Exact next target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

cmake -S tests/navigation_guidance -B build/tests/navigation_guidance -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests/navigation_guidance --target ruckig_route_planner_tests
ctest --test-dir build/tests/navigation_guidance -R ruckig_route_planner -V
```

Interpretation:
- 8/8 -> build/open viewer.
- still fail -> use exact new `min_speed=<value>` and latest Ruckig perf line; fix the
  mechanism, do not weaken the regression.

Then:
```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Viewer:
- WHITE coarse retained route;
- BLUE dense execution guide;
- PURPLE calculated Ruckig reference;
- GREEN actual flight;
- HUD/title must display current `REV`.

If PURPLE is clean but GREEN is bad, move to Follower/Pilot/body-thrust semantics.
If PURPLE is bad, remain in continuous reference generation.

Do not enable dynamic avoidance yet.
