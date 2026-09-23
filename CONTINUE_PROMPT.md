# CONTINUE PROMPT — Elite navigation M2 composition-root split

Continue in GitHub repository `forzub/Elite`, branch `main`.

The normative architecture is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. M1 is
accepted on target. M2 is active.

## Read before editing

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`;
7. `AGENTS.md`.

For the active slice inspect:

- `tools/navigation_runtime/NavigationScenarioRuntime.{h,cpp}`;
- `tools/navigation_runtime/NavigationScenarioIo.cpp` when present;
- `tools/navigation_runtime/CMakeLists.txt`;
- `tests/architecture_contracts/check_navigation_api_purity.py`;
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`.

Follow other files only where this path references them.

## Accepted evidence

The latest MinGW64 run emitted:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

Both coordinate fixtures advanced through all 90 page boundaries, completed
their 91-page programs and matched across 902 execution frames. M1 must not be
reopened by the later high-speed failure.

The high-speed Newtonian fixture reports 34 infeasible actuator segments and
`PROGRAM_INVALIDATED_TRACKING_LOSS` at 0.51 s. That is retained M3/M4 evidence:
the present chain times translation before proving reachable attitude/thrust.
Do not tune the timeout or weaken the assertion.

## Immediate work

The first M2 target gate found a compile-only extraction defect before runtime
execution: `normalizedOr()` and `basisFromForwardUp()` had moved into
`NavigationScenarioIo.cpp`'s anonymous namespace even though runtime still
uses them. The correction introduces shared pure
`NavigationScenarioMath.h`; scenario I/O and runtime both include it.

Rerun the unchanged target command in `CURRENT_TASK.md`. Scenario
JSON/file-input ownership remains in `NavigationScenarioIo.{h,cpp}` and the
`EliteNavigationScenarioToolIo` CMake target. `NavigationScenarioRuntime.cpp`
must still own no parsing and include no nlohmann JSON. Keep
`ScenarioDefinition` as the immutable boundary value.

This is a behavior-preserving split. Do not change routing, maneuver timing,
Follower gains, physics, risk semantics or navigation geometry.

## Architecture that must survive later stages

```text
certified NavigationSpace
 -> typed corridor / portals
 -> STANDARD or EXTREME doctrine + risk budget
 -> NEWTONIAN or ASSISTED physical maneuver
 -> nominal capability/resource/continuous hull proof
 -> pilot execution envelope / realized skill error
 -> authoritative physics and contact attribution
```

Squeeze is a passage profile, not a third doctrine. `ConstrainedRisk` still
requires a nominally collision-free and actuator-feasible maneuver. Broadphase
spheres/AABBs and portal centers are never exact free-space truth.

## Iteration protocol

After every state-affecting event, update the blueprint, state/task/project
documents, Stage-12 journal and this prompt. Run all available gates, inspect
the complete diff, commit and push one coherent iteration to `main`. Never claim
a target result that was not run.
