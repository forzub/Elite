# CONTINUE PROMPT — Elite Navigation: reducer state + free-transit corridor + engine visualization

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read those files first, then inspect:
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tools/navigation_runtime/NavigationTrace.h/.cpp`;
- `src/game/navigation/AcceptedManeuverProgram.h`;
- `src/game/navigation/ManeuverTrackingController.cpp`;
- relevant navigation runtime/guidance tests.

## Current confirmed trajectory state

The scalar path-progress architecture is much better. Latest uploaded perf tail shows
steady default wall runs with:
- one Ruckig solve;
- `min_speed_mps=10.0000`;
- `max_speed_mps=10.0000`.

The minimum-cant Newtonian attitude also visually improved hull behavior.

## Current user requests already implemented, awaiting target validation

### 1. Redux-style viewer state

Native C++ viewer now has one authoritative reducer-driven `AppState`:
- actions for control law, pilot, style, obstacle, playback;
- all controls dispatch actions;
- runtime effective control law is observed from trace frames and dispatches through the
  same reducer;
- buttons are projections of this state.

Do not introduce separate widget-owned mode booleans.

Diagnostics:
- CONTROL LAW REQUESTED
- CONTROL LAW EFFECTIVE
- CONTROL LAW SWITCHES

If runtime actually changes from Assisted to Newtonian, the button must follow.

### 2. Free-transit speed/progress corridor

`AcceptedManeuverProgram::TrackingEnvelope`:
- `alongTrackSpeedDeadbandMps`;
- `alongTrackPositionDeadbandMeters`.

`ManeuverTrackingController` removes in-corridor along-track position/velocity error
before computing feedback, while retaining cross-track correction.

Current doctrine:
- STANDARD: +/-0.5 m/s and +/-12 m;
- EXTREME: +/-1.0 m/s and +/-16 m.

Thus 10.1 m/s on a 10.0 m/s free-transit reference must not request braking solely to
recover exact speed.

Do not apply these loose corridors to PrecisionCapture/PrecisionTransit.

### 3. Main-engine use visualization

Each execution trace frame exposes:
- effective runtime law;
- `mainEngineThrottle01`, derived from actual physical positive aft-main acceleration.

Viewer:
- orange rear face only when actual aft main engine is firing;
- HUD `MAIN: N%`;
- RCS does not light the rear face.

This is the diagnostic for deciding whether a hull turn is doing useful main-engine
vectoring.

### Relevant code commits

- `92d65f86fe22ec1d0a404f952a0ebd0eb4935fb3`
- `a171510889a2d235896e6ad567011c2b651ee3b7`
- `023861170299dc7321975f2e73173af4b2547ca8`
- `ad053356ada03d5212185b7d49d0b6aeb017ed6a`
- `0b2e48b66744662e783b52b135ef26a714f43fb5`
- `b503c3e35a9b8c2b0333b026b9251d6075a1afe2`
- `1d17f9d208f9ef77a3dc8ac09753202aba7cd4c4`
- `cbb58b16a7a15803cc8e56618d639916d748a53e`
- `089d905f3207fa48afe3bba70935ce9413b145f6`
- `163bee3c58443a5d0d6b4ad092ae7e6f970feef1`
- `2e8692b68c670275e7f96654c4a990c473b885a9`
- `7a663da3b7f6bc8a92daa9433daeef40a10d41c9`
- `3cadb29a840643524f2edafba3abb0b9091d795c`
- `9e27bcd2392e7c18c36354374df33ab04882045f`
- `3845b50910310494b394ec00820c86cc97b96aff`
- `5299809e37f0a5061d56f066240619d56c6f27b4`
- `eedad40169c00509e01015f1a1777bd67f29965f`
- `a60a215fde65505826b9b96c70edf511991da91f`

## Next target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Validate four combinations:
- Assisted Standard
- Assisted Extreme
- Newtonian Standard
- Newtonian Extreme

Watch:
- selected law button;
- requested/effective/switches;
- speed + corridor;
- MAIN:% and orange rear face;
- red reference nose vs cyan hull vs yellow velocity.

If the effective law remains ASSISTED with zero switches but motion still resembles
Newtonian, debug Assisted force/attitude behavior rather than changing UI state.

Do not enable dynamic avoidance yet.
