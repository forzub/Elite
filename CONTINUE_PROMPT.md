# Elite Navigation v2 — continue prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.
Target: Windows 10 / MSYS2 MinGW64 / g++ 15.2 / CMake + Ninja.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`
- `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`

Current architecture:
```text
PLANNER WORLD
  known world + objective + vehicle/pilot/doctrine
  -> topology/corridor
  -> local route-aligned free-space path
  -> physically proved AcceptedManeuverProgram

AUTOPILOT/FOLLOWER WORLD
  AcceptedManeuverProgram + actual state
  -> bounded tracking/recovery + safety monitoring/reflex
  -> PilotSkill -> propulsion -> physics
  -> completion/invalidation -> planner
```

Key decisions:
- planner does not "discover" obstacles through sensor-like rays; world truth is already known;
- keep segment/sweep/ray tests only as geometric proof primitives;
- replace the 15/30/45/60/75 LocalAvoidance fan as the target search method with a route-aligned configuration-space corridor solver;
- keep NavigationSpace topology above that local solver;
- planner must prove and publish the exact time-parameterized program the follower executes;
- follower must not become a hidden second normal planner;
- bounded emergency reflex is allowed, but material deviation invalidates the program and wakes planner;
- shared dynamic broadphase/influence isolation should become scene-wide/batched; only agents needing replans enter planner work.

Immediate code task:
1. introduce bounded `AcceptedManeuverProgram`;
2. migrate `TrajectoryFollower` to sample it + bounded feedback;
3. pin proof/execution identity in deterministic tests;
4. then prototype `RouteAlignedCorridorPlanner` beside existing LocalAvoidance and A/B test before retiring anything.

Last fully accepted Stage-12 target-machine baseline:
`daaf038021cdf8b9561db60fdd35e7cefce0b2df`.

Last target-machine checkout actually exercised:
`46f6a37da6775a1d044391f773476df1bb07bc6a`.

Architecture-analysis doc commit:
`92432c842d5ad632b320f90c2ca258b82f17d26b`.

Do not claim acceptance without a fresh target-machine gate.
Do not weaken tests/geometry to make a fixture pass.
Do not restore per-frame planning.
Do not let RCS/allocator rescue an impossible planner command.

After every state-affecting iteration:
- rewrite this file completely;
- rewrite `CURRENT_TASK.md`;
- update `CURRENT_STATE.md`, `PROJECT_STATE.md`, and `STAGE12_END_TO_END.md`.
