# CURRENT TASK — M2 slice 1: extract scenario I/O

Date: 2026-09-23

Status: **M1 ACCEPTED / M2 SLICE 1 IMPLEMENTED — TARGET EVIDENCE REQUIRED**

The target emitted the required frame-equivalence marker after both coordinate
fixtures traversed all 91 storage pages. M1 is closed.

The aggregate pipeline remains red only in the later high-speed Newtonian case:
34 actuator segments are infeasible and the accepted reference is invalidated
after 0.51 s. That is retained evidence for M3/M4 and is outside this structural
slice.

## Implemented slice

Scenario parsing/tool file I/O has been split out of the 3849-line
`NavigationScenarioRuntime.cpp`:

1. JSON helpers and `loadScenarioDefinition()` now live in
   `NavigationScenarioIo.{h,cpp}`;
2. `ScenarioDefinition` remains the immutable Stage-1/Stage-2 input;
3. runtime calculation/orchestration no longer includes nlohmann JSON or opens
   scenario input streams;
4. CMake owns an explicit `EliteNavigationScenarioToolIo` target;
5. the API-purity contract pins the new physical boundary.

No control tuning, timeout relaxation, portal/octree work or high-speed physics
repair belongs in this slice.

## Completed local gates

All commands below pass. No local C++ build ran because CMake is not installed:

```bash
export PYTHONDONTWRITEBYTECODE=1
python tests/architecture_contracts/check_navigation_api_purity.py
python tests/architecture_contracts/check_navigation_stage1_nominal_route.py
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
python tests/architecture_contracts/check_geometric_path_planner.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
git diff --check
```

## Target gate after the slice

Run as one chained command in MSYS2 MinGW64:

```bash
cd /d/__elite/work && \
git pull --ff-only && \
git rev-parse HEAD && \
bash tests/navigation_runtime/run_stage1_mingw64.sh && \
cmake --build build/tests/navigation_runtime \
  --target maneuver_program_sampler_tests && \
ctest --test-dir build/tests/navigation_runtime \
  -R "^maneuver_program_sampler$" \
  --output-on-failure && \
cmake --build build/tools/navigation_runtime \
  --target navigation_runtime_pipeline_tests && \
ctest --test-dir build/tools/navigation_runtime \
  -R "^navigation_runtime_pipeline$" \
  --output-on-failure
```

Return the complete output beginning with the checkout hash. Required
invariants:

- scenario parsing/build/link succeeds from the new module;
- the M1 non-identity marker remains PASS;
- identity/non-identity metrics remain unchanged;
- the high-speed case remains classified by its actual physical-authoring
  failure until M3/M4, rather than being hidden or redefined.
