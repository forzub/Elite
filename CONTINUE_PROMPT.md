# CONTINUE PROMPT — Elite Navigation: build fix + local variable-speed profile

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp/.h`
- `src/world/navigation/TrajectoryGenerator.cpp/.h`
- `tests/navigation_guidance/RuckigRoutePlannerTests.cpp`
- `tools/navigation_runtime/README.md`.

## Latest target evidence

The user's MinGW64 run on old HEAD `c5a2ea68434a...` failed at viewer compile:

1. `drawHud()` used `recalculationRequired(state)` before declaration.
2. speed-slider release contained dangling `if (speedChanged)`.

Fixed:
- `e2597a7714f5e7fee2200e7f47f9bfb9bafb2734`
- `b15d0e97c6897d2b099e0c6e41d60830c45776b9`

Also remove the old unused `buildReferenceAttitudes` acceleration warning.

## Non-negotiable speed semantics

Speed is a state variable, not a FlightStyle constant and not globally uniform.

Rules:
- START/FINISH speed constrain only boundary states.
- Intermediate speed may vary.
- Braking requires a concrete local physical/geometric/mission/safety reason.
- Sharp/complex maneuver may slow heavily or stop.
- Clear segment may be much faster than FINISH speed.
- Local restriction must not become a whole-route cap.
- Without a reason, do not cut speed.
- STANDARD/EXTREME only trade clearance/risk; they never define speed.

This is recorded in `CONTROL_LAW_MANEUVER_MODEL.md`.

Already fixed:
- execution `maxSpeedMps` comes from vehicle capability, not max boundary speed;
- exact moving terminal speed no longer globally caps scalar path progress;
- focused regression requires START=10 / FINISH=10 three-point straight transit to exceed
  12 m/s between boundaries.

Commits:
- `336b48a293dca0306847afbe3bc00e5787713a2e`
- `16fe6a8918fa1a9f03fce3dd2f814987198f31e8`
- `22133bbe5c9ef61a52e839338e88b44a29061f22`
- `49009e6f480c7a70848f9a89c5cb4a0f7d4c8dab`

## Known remaining speed bug

`globalGuideSpeedLimit()` still uses the worst curvature on the whole multi-point route
as one global max speed.

This violates the new locality rule.

Once target build is green, replace that global behavior with a local scalar speed
profile:
- local curvature / point / range limits by progress;
- backward braking feasibility before each restriction;
- forward acceleration feasibility;
- Ruckig remains timing/jerk owner;
- stop is legal where necessary;
- clear segments accelerate independently;
- no local limit globally clamps unrelated segments.

Do not move this responsibility into Follower.

## Calculate UX remains fixed

No automatic recalculation on selector changes.

- speed/style dirty => Stage-1 + Stage-2;
- control-law/pilot/dynamic toggle dirty => Stage-2 only;
- one click `РАССЧИТАТЬ` performs all required calculation;
- concise on-screen log;
- then dim/disabled `РАСЧЕТ ГОТОВ`;
- changing an invalidating input re-enables it;
- no second Execute / `ЗАПУСТИТЬ ПОЛЁТ`.

## Target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If green:

```bash
bash tests/navigation_guidance/run_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Do not claim PASS without target evidence. Do not enable dynamic avoidance yet.
