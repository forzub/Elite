# CONTINUE PROMPT — Elite Navigation global corridor architecture

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`,
`src/world/navigation/local/LocalAvoidancePlanner.h`,
`src/world/navigation/space/NavigationSpace.h/.cpp`,
`src/game/navigation/NavigationRuntimePlanner.h/.cpp`,
and the live stand under `tools/navigation_runtime/`.

After every state-affecting event synchronize the Markdown state files and recreate this file from scratch.

## Correct ownership contract

The nominal global route/corridor is built ONCE from start to finish and remains authoritative while:
- destination/goal revision is unchanged;
- static navigation world/corridor revision is unchanged.

Moving obstacles do NOT trigger global route reconstruction.

Dynamic snapshots are consumed by the bounded local monitor/avoidance layer only.
`maxResultAgeSeconds` (~0.25 s) is a dynamic-snapshot freshness limit, NOT a global replanning cadence.

Follower/autopilot continuously executes the accepted maneuver/trajectory.
Local dynamic avoidance may temporarily depart from the nominal corridor, brake when necessary, and later progressively reacquire the same retained global corridor.

Global planning may be recomputed only on goal change or static-world/corridor invalidation/change.

## Current live-stand problem

`NavigationScenarioRuntime.cpp` currently calls `NavigationRuntimePlanner::plan()` repeatedly after short execution slices. That incorrectly recomputes the static/global part over and over. Do not keep this architecture.

## Important capability gap

`NavigationSpace::queryCostedCorridor()` currently produces coarse region/portal topology and ordered portal centers.
Exact static `NavigationObstacle` geometry is used for segment proof/local rejection, but the global corridor search does NOT yet synthesize a full geometric centerline around arbitrary exact obstacles inside one coarse region.

The default live `scenario.json` currently has one coarse region plus an exact wall. Therefore it cannot honestly demonstrate the requested start->finish global geometric corridor until this gap is solved.

## Next implementation task

1. Add a cached nominal global corridor product with geometric centerline/waypoints and clearance/envelope information.
2. Build it once from start to finish against the static world.
3. Cache it by goal revision + static-space revision.
4. Render this retained full corridor immediately after `РАССЧИТАТЬ`.
5. Execute along it using the real production maneuver compile/proof/selection/follower chain.
6. Run LocalHorizonPlanner/LocalAvoidancePlanner only for dynamic/local conflicts against the retained corridor.
7. Sudden moving obstacle may cause local bypass/braking only; it must not rebuild the global route.
8. After the obstacle, progressively reacquire the same nominal corridor.

Do not use a 0.25 s or 0.5 s timer as a global replanning trigger.

## Existing live-stand UI contracts

- ordinary decorated Windows window maximized, not exclusive fullscreen;
- Russian UI;
- fixed non-jumping right panel slots;
- Assisted/Newtonian selector;
- Expert/Average/Loser selector;
- Standard/Extreme selector;
- sudden-obstacle checkbox;
- `РАССЧИТАТЬ` button;
- Cobra horizon inset;
- exact executable launch command must always be provided after builds.

## Previous live-video issue already fixed

The first live video showed `ДАННЫЕ УСТАРЕЛИ` because absolute simulation time was incorrectly passed as dynamic snapshot age. Fresh synchronous snapshot age is now `0.0`. Do not regress.
