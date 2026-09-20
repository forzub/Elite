# CONTINUE PROMPT — Elite Navigation route/corridor vs physical tunnel

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md`,
`src/world/navigation/local/LocalAvoidancePlanner.h`,
`src/world/navigation/space/NavigationSpace.h/.cpp`,
`src/game/navigation/NavigationRuntimePlanner.h/.cpp`,
and the live stand under `tools/navigation_runtime/`.

After every state-affecting event synchronize the Markdown state files and recreate this file from scratch.

## Global/local ownership

The nominal global route is built once from start to finish and remains authoritative while goal and static world are unchanged.
Moving obstacles do not rebuild the global route. They are handled by bounded local monitoring/avoidance and later route reacquisition.
`maxResultAgeSeconds` is dynamic snapshot freshness, not a global replanning timer.

## Terminology — corridor vs tunnel

`corridor` is a navigation/test abstraction around the nominal route. It may be a centerline/polyline plus a coarse envelope and is used to ask whether the ship can generally proceed along the route.

It is NOT the authoritative physical hull-clearance volume.

`tunnel` is the physically meaningful product: the exact or conservative time-parameterized swept volume of the actual Cobra hull along an accepted trajectory, including body orientation.

Whether Cobra clips tunnel walls / apertures is decided by tunnel/trajectory physical proof against exact static geometry and dynamic occupancy, not by the coarse nominal corridor.

Do not over-engineer the global corridor into exact hull collision geometry.

## Current live-stand status

`tools/navigation_runtime` is a live scenario-driven stand. The first video exposed an integration bug where absolute simulation time was passed as dynamic snapshot age. That is fixed: fresh synchronous snapshots use age `0.0`.

The right diagnostic panel must use fixed Y slots and never vertically reflow.

## Current implementation direction

1. Stop periodic global replanning.
2. Build/cache one nominal start->finish route/corridor from the static world.
3. Render that retained nominal route immediately after `РАССЧИТАТЬ`.
4. Follow it with the production execution chain.
5. Use LocalHorizonPlanner/LocalAvoidancePlanner only for local dynamic conflicts.
6. Local bypass/braking may temporarily leave the nominal route, then progressively reacquire it.
7. Exact physical feasibility is later/ downstream tunnel proof, including real hull orientation.

## Important remaining gap

The live stand still uses a stand-local handcrafted `makeShortProgram` instead of the complete production physical compile/proof/selection/acceptance path. Replace this before treating live-stand success as final navigation evidence.

## UI contracts

- ordinary decorated Windows window maximized;
- Russian UI;
- fixed non-jumping right panel;
- Assisted/Newtonian;
- Expert/Average/Loser;
- Standard/Extreme;
- sudden obstacle checkbox;
- `РАССЧИТАТЬ`;
- Cobra horizon inset;
- always provide a separate exact executable launch command after builds.
