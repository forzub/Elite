# CONTINUE PROMPT — Elite Navigation autonomous E2E evidence reset

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read before changing behavior:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`;
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`;
- `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`;
- current code under `tools/navigation_runtime/`.

After every state-affecting event synchronize the project Markdown state and recreate
this `CONTINUE_PROMPT.md` from scratch.

## Evidence rule

Do not infer full-system acceptance by adding together green component/slice tests.

Classify evidence explicitly as:
1. component/unit proof;
2. execution of authored AcceptedManeuverProgram;
3. planner/topology slice;
4. authoritative authored-world integration;
5. true autonomous scenario end-to-end.

Only category 5 proves the complete navigation chain for the tested scenario.

## Findings from the 2026-09-20 full-chain audit

Most movement/corridor/fly-through/chained tests use real Follower, PilotSkillExecutor
and SharedShipPhysics but construct the AcceptedManeuverProgram inside the test.
They prove execution of a supplied good program, not production generation of it.

`NavigationRuntimePlannerTests` use hand-authored regions/portals, so they prove
topology/local planner contracts, not arbitrary raw-obstacle route synthesis.

`OrdinaryPhysicalManeuverCompilerTests` are genuine B5 tests but current B5 supports
Newtonian only; Assisted intentionally returns `UnsupportedControlLaw`. B5 candidates
still require continuous downstream proof.

`NavigationCompositeProvingGroundTests` is real-component glue but not production
orchestration: topology is authored; `makeProgram`, `buildDoctrineChoice` and
`fitAuthorityBoundedReplacement` provide test-owned programs/annotations/fitting.

The authoritative GameSimulation NavigationRuntimeLab is genuine live evidence for its
scope: real HitVolumes/map/space/planner/control/physics/replication. However the
slit/tunnel route is itself authored as deterministic regions/portals/start/goal. It
does not prove arbitrary start+raw-world+finish autonomous route/program synthesis.

The repository architecture audit already warned that P6 ordinary maneuver generation,
P7 proof integration, P8 ordinary decision integration and P9/P10 handoff were
incomplete/transitional. The project error was over-interpreting narrower green gates.

## Correct runtime ownership

Global route/corridor is built once start->finish and retained while goal and static
route/world revisions remain valid.

Moving/dynamic obstacles do NOT rebuild the global route. They are handled by a bounded
local monitor/avoidance layer. After a local bypass, progressively reacquire the same
nominal route.

`maxResultAgeSeconds` is dynamic-snapshot freshness, not a global replanning period.

`corridor` is a navigation/test abstraction. Exact physical wall/aperture contact is
owned by the later time-parameterized swept-hull `tunnel`/continuous proof.

## Existing GameSimulation clue

The authoritative lab already contains the correct scheduler principle:
`Monitoring is allowed every fixed step; planning is not.`

It uses `NavigationExecutionReplanPolicy` + `NavigationWorkScheduler` and wakes planner
work on missing/completed/expired/invalidated/tracking/goal/capability/topology events.
Reuse this event-driven ownership instead of inventing a 0.25/0.5 s global planner loop.

## Current live stand defects/status

The first live video exposed a stand-side bug: absolute simulation time was passed as
dynamic snapshot age, causing StaleHold after 0.25 s. That has been fixed to fresh
snapshot age `0.0`.

The right HUD has fixed Y slots and must never vertically reflow.

However `NavigationScenarioRuntime.cpp` is still architecturally transitional:
- it periodically calls combined `NavigationRuntimePlanner::plan()`;
- it uses stand-local `makeShortProgram()` quintic programs;
- therefore it bypasses the full production B5/B6/B7/B8 chain.

Do not treat a visually successful run of that transitional stand as final acceptance.

## Next canonical implementation target

Build one shared autonomous scenario chain:

`objective/world`
` -> cached global route`
` -> local route-aligned geometric path`
` -> B5 physical maneuver candidates`
` -> B6 continuous/tunnel proof`
` -> B7 doctrine decision`
` -> B8 AcceptedManeuverProgram`
` -> B9/B10 follower`
` -> B12 PilotSkill`
` -> B13 authoritative physics`
` -> B11/B14 monitor + event-driven replan`.

Hard rules:
- no test/local `makeProgram()` / `makeShortProgram()` in the canonical E2E gate;
- no hand-authored B7 safety/risk annotations standing in for production proof;
- preserve all useful component tests but describe their evidence narrowly;
- Assisted cannot be called production-complete until its B5+ downstream chain exists;
- global planner is event-driven, not periodic;
- dynamic surprise obstacle triggers local response, not route reconstruction.

## Command rule

Whenever compilation produces an executable, always give a separate exact executable
launch command from the documented working directory.
