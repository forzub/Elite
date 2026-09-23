# CURRENT TASK — M2 slice 1: extract scenario I/O

Date: 2026-09-23

Status: **M2 SLICE 1 TARGET RUNTIME REACHED — KNOWN M3/M4 FAILURE REPRODUCED; TESTED SHA NOT PRESENT IN EXCERPT**

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


## First target result and correction

The first MinGW64 gate reached the viewer/runtime build, then failed to compile
`NavigationScenarioRuntime.cpp` because `normalizedOr()` and
`basisFromForwardUp()` had been moved with the JSON parser into
`NavigationScenarioIo.cpp`'s private anonymous namespace.

This is an M2 ownership/refactor defect, not a navigation-behavior failure.
The correction introduces shared pure `NavigationScenarioMath.h`, used by both
runtime and scenario I/O. The parser no longer privately owns math still needed
by runtime. No flight semantics changed.

The next action is to rerun the unchanged target gate below.


## Latest target result

The corrected extraction now compiles/links and reaches runtime. The M1
non-identity frame marker remains PASS. The aggregate pipeline then stops only
at the known high-speed Newtonian physical-authoring fixture:

```text
PLANNED ACTUATOR SEGMENTS: 753
PLANNED ACTUATOR INFEASIBLE: 34
FOLLOWER FAIL REASON: PROGRAM_INVALIDATED_TRACKING_LOSS
FOLLOWER FAIL TIME: 0.51 S
MAX REFERENCE/VELOCITY ANGLE: 170.46 DEG
MAX BODY/VELOCITY ANGLE: 1.00 DEG
```

This is not an M2 scenario-I/O regression. It is the retained M3/M4 defect:
translation is timed before reachable attitude and actuator allocation are
proved. Keep the failing assertion; do not make the M2 split "green" by relaxing
tracking or redefining success.

The supplied output excerpt omitted the initial `git rev-parse HEAD` line, so
the exact tested checkout cannot be recorded as verified evidence from this
message. No rerun is required merely to understand the failure; record the hash
when available in subsequent target evidence.

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

## Corrected planning contract

M3/M4 must not merely return failure for the high-speed case. A typed
infeasibility witness feeds a bounded coordinator search over corridor,
terminal, speed and time alternatives while navigation stays active. Only a
fully physical program may be accepted; unavoidable contact belongs to a
separate damage-minimizing emergency solve. This does not change the current M2
gate classification.
