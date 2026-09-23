# CONTINUE PROMPT — validate Elite navigation M1, then activate M2

Continue in GitHub repository `forzub/Elite`, branch `main`.

The navigation-layer target architecture is normative. The current code is an
M1 candidate, not an accepted implementation.

## Read first

Read completely, in order:

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`
2. `CURRENT_STATE.md`
3. `CURRENT_TASK.md`
4. `PROJECT_STATE.md`
5. the final dated sections of
   `src/game/navigation/STAGE12_END_TO_END.md`
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`

Then inspect the M1 diff in:

- `src/game/navigation/NavigationFrameBoundary.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tests/navigation_runtime/NavigationRuntimePlannerTests.cpp`;
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`;
- the three modified navigation architecture-check scripts.

## Current M1 candidate

Implemented:

- private `toSystemIntent()` removed;
- Follower intent crosses `NavigationFrameBoundary`;
- initial NavLocal P/V/basis/angular velocity cross the canonical boundary;
- runtime consumes an explicit `KinematicFrame` snapshot and epoch;
- execution deadline and runtime clock share the same absolute epoch;
- translating/accelerating/rotating frame state advances during execution;
- Follower, trace and terminal checks receive converted NavLocal observation;
- identity versus non-identity moving/rotating product-chain E2E added;
- brittle exact-layout checker assertions replaced with normalized semantic
  operation checks.

Local PASS:

- `check_navigation_api_purity.py`;
- `check_navigation_stage1_nominal_route.py`;
- `check_navigation_stage12_runtime_planner.py`;
- `check_geometric_path_planner.py`;
- checker byte-compilation and `git diff --check`.

Not available locally: CMake, GLM headers, C++ compile/link and runtime E2E.

## Immediate action

Obtain the Windows MSYS2/MinGW64 output requested in `CURRENT_TASK.md`.

Required independent M1 marker:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

If compilation or that fixture fails, diagnose and repair M1 without weakening
tolerances or removing non-identity motion. Preserve the exact target log.

If the M1 marker passes but a later known physical-planner case fails, record
M1 frame-boundary acceptance separately from that later failure. Do not pretend
the full navigation layer is green.

## Governing invariant

```text
accepted maneuver == capability-checked maneuver
                  == continuously collision-proved maneuver
                  == actuator program executed by physics
```

Do not tune follower gains/timeouts, accept infeasible propulsion, add hidden
lookups, restore identity-frame copies, or start octree integration to evade a
failure.

## After M1 acceptance

Activate blueprint stage M2 only:

- split scenario/tool I/O from calculation;
- split route-request composition;
- split maneuver planning/proof composition;
- split execution harness;
- split trace/diagnostic adapters;
- keep production algorithms in production libraries, not the tool monolith.

M2 must preserve behavior. It is not permission to redesign the physical
planner yet.

## Mandatory iteration protocol

Every state-affecting iteration must:

1. update progress/status inside
   `NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. update `CURRENT_STATE.md`;
3. update `CURRENT_TASK.md`;
4. update `PROJECT_STATE.md`;
5. update `src/game/navigation/STAGE12_END_TO_END.md`;
6. synchronize API/audit/control-model documents when their contracts change;
7. recreate this file from scratch for the next exact action;
8. review the complete diff, run available gates, commit and push the coherent
   iteration to `main`.

Never claim a check that did not run. Hashes are evidence baselines, not
self-invalidating “current HEAD” fields.
