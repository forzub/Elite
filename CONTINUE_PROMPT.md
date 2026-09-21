# CONTINUE PROMPT — Elite Navigation static corner maneuver quality

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

After every state-affecting event, update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
and **recreate this CONTINUE_PROMPT.md from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `src/world/navigation/GeometricPathPlanner.cpp`
- `src/world/navigation/TrajectoryGenerator.cpp`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.h/.cpp`
- `src/game/navigation/AcceptedManeuverProgram.h`
- `src/game/navigation/ManeuverProgramSampler.h/.cpp`
- `src/game/navigation/ManeuverTrackingController.h/.cpp`
- `src/game/navigation/TrajectoryFollower.h/.cpp`
- `src/game/navigation/DynamicMotionSystem.cpp`
- `src/game/ship/ShipController.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/scenario.json`
- `tests/navigation_runtime/NominalRoutePlannerTests.cpp`

## Latest user evidence

A target run before this candidate completed retained-route execution in Expert:
- Standard/Newtonian: final P error 0.15 m, final speed 0.10 m/s;
- Extreme/Newtonian: final P error 0.10 m, final speed 0.48 m/s;
- Extreme/Assisted: same observed terminal quality;
- no coarse static contact.

However user video shows that the motion is behaviorally wrong:
- shallow corners cause near/full stops;
- Newtonian velocity visibly changes direction while the hull is still acquiring
  attitude;
- Newtonian rotates much more than a small course change should require;
- Assisted also stops on a smooth/shallow bend;
- Standard/Extreme look behaviorally the same.

Treat these as genuine quality blockers, not presentation complaints.

## Confirmed source diagnosis

### Stop at shallow corners

The old retained route had about 25.5 degree interior bends.

`TrajectoryGenerator::buildWaypointVelocities()` tries a shortcut chord using
25% of adjacent leg length. For the old wall route, that chord intersects the inflated
box. The waypoint velocity remains zero. Ruckig therefore authors StopTurnGo.

There is no Expert-pilot reason for the stop. It is a fallback artifact.

### Newtonian body/velocity mismatch

The Stage-2 trajectory currently authors translation first. Then
`buildReferenceAttitudes()`:
- Newtonian points body-forward toward trajectory acceleration when |A| > 0.35;
- otherwise it tends toward velocity direction;
- Assisted tends toward velocity direction.

Because RCS can already produce bounded lateral acceleration, Newtonian velocity can
bend before the hull has rotated. Before a zero-speed waypoint, the reference
acceleration becomes braking-oriented, causing additional body rotation.

Low-level propulsion allocation is not the primary fault:
- Newtonian main channel is forward-only;
- reverse/main braking requires body reorientation;
- RCS remains bounded;
- Assisted has symmetric longitudinal main authority but bounded transverse RCS.

The missing piece is a physically authored control-law-specific maneuver before ACCEPT.

### Flight styles

Current diagnostic Standard/Extreme is mostly 10 vs 18 m/s max-speed request.
This is not yet a full flight doctrine. Standard/Extreme must eventually choose
meaningfully different proved maneuver families/aggressiveness.

## Route bug and current candidate

Old support graph lacked box edge midpoints, producing:
`(0,0,0) -> (110,-27,-45) -> (190,-27,-45) -> (300,0,0)`,
~323.75 m.

Nearest-face route should be approximately:
`(0,0,0) -> (110,-27,0) -> (190,-27,0) -> (300,0,0)`,
~306.53 m.

Current unverified commits:
- `e53312cc9b00119e69f1c7676edf14b5d21fea64` box edge support nodes;
- `8e0d4c4c6ca7d4d7ae3e7cc71387ebff8b335302` nearest-face detour regression;
- `ac30d441ebdafe232af0bf0fe7e8882da9f38767` triangular-prism ship, small
  translucent nose, thick speed-proportional velocity vector, numeric speed;
- `c4c63c9751a4b5b381da290a570ee98775148eb6` route-point, waypoint-speed and
  body/velocity-slip diagnostics.

Do not call them accepted until the target MinGW64 gate runs.

## Active task

Pause the dynamic-obstacle overlay. First make ordinary static corner execution
credible.

Target semantics:
- a blocked synthetic corner-cut chord does not mean stop;
- shallow/medium bends remain moving whenever a physically safe maneuver exists;
- Newtonian Expert uses an intentional body/thrust maneuver, not arbitrary vector
  translation followed by attitude catch-up;
- Assisted Expert also preserves speed on normal bends;
- stop/flip only when actually required;
- B4 geometry -> B5 physical maneuver -> B6 continuous proof -> B7 doctrine ->
  B8 AcceptedManeuverProgram -> B9/B10 execution;
- do not bypass the missing generalized ordinary B6 proof.

## Target-machine commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then launch separately:

```bash
cd /d/__elite/work
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Collect:
- HEAD;
- full Stage-1 route points and length;
- `RETAINED WAYPOINT SPEEDS`;
- `MAX BODY/VELOCITY ANGLE`;
- any build/test failure;
- optionally a short video of Expert/Standard/Newtonian and Assisted.

After target evidence, update all mandatory MD files and recreate this prompt from
scratch.
