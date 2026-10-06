# Elite build/test layout — authoritative command map

Updated: 2026-10-06.

This file is the command-layout reference for `forzub/Elite`.
Before giving build/test/run commands, verify the owning `CMakeLists.txt`
and the canonical helper scripts against the current `main`. Do not infer
that a CMake target exists in another build tree just because its executable
name is known.

## Repository root

Windows/MSYS2 working copy used by the project:

```text
D:/__elite/work
```

All commands below assume:

```bash
cd /d/__elite/work
```

## Canonical build directories

The authoritative variables are defined in:

```text
tests/helpers/build_layout.sh
```

Current layout:

| Purpose | Source tree | Build tree | Main output |
|---|---|---|---|
| Client game | repository root | `build/` | `build/EliteGame.exe` |
| Headless server | repository root | `build/headless_server/` | `build/headless_server/EliteServer.exe` |
| Test builds root | individual test source dirs | `build/tests/` | per-suite executables |
| Test logs | runtime output | `build/test-logs/` | `*.log` |
| Navigation runtime tests | `tests/navigation_runtime/` | `build/tests/navigation_runtime/` | test executables in that build tree |
| Navigation Ruckig tests | `tests/navigation_ruckig/` | `build/tests/navigation_ruckig/` | trajectory test executables |
| Architecture CMake tests | `tests/architecture_contracts/` | `build/tests/architecture_contracts/` | architecture test executables |

## Critical ownership rule

The root build tree `build/` owns `EliteGame` and the root runtime
libraries. It does **not** own standalone navigation-runtime test executables
declared by `tests/navigation_runtime/CMakeLists.txt`.

Therefore this is invalid:

```bash
cmake --build build --target docking_advisory_tests
```

because `docking_advisory_tests` is not a target in the root build tree.

The correct owning tree is:

```bash
cmake --build build/tests/navigation_runtime --target docking_advisory_tests
```

after that standalone tree has been configured.

## Navigation-runtime target names vs CTest names

CMake **build target** and CTest **test name** are not interchangeable.

| Purpose | Build target | CTest name |
|---|---|---|
| Docking advisory | `docking_advisory_tests` | `docking_advisory` |
| Hit-volume adapter | `navigation_hit_volume_adapter_tests` | `navigation_hit_volume_adapter` |
| Accepted maneuver builder | `accepted_maneuver_program_builder_tests` | `accepted_maneuver_program_builder` |
| Maneuver sampler | `maneuver_program_sampler_tests` | `maneuver_program_sampler` |
| Runtime control V2 | `navigation_runtime_control_tests` | `navigation_runtime_control` |
| Client route autopilot | `client_route_autopilot_tests` | `client_route_autopilot` |
| Predictive V2 tunnel proving ground | `navigation_v2_tunnel_proving_ground_tests` | `navigation_v2_tunnel_proving_ground` |
| Tracking controller legacy lab | `maneuver_tracking_controller_tests` | `maneuver_tracking_controller` |
| Corridor-capture guidance legacy lab | `corridor_capture_guidance_tests` | `corridor_capture_guidance` |
| Maneuver phase gate legacy lab | `maneuver_phase_gate_tests` | `maneuver_phase_gate` |
| Public RoutePlanner API | `route_planner_api_tests` | `route_planner_api` |
| Public RouteFollower API | `route_follower_api_tests` | `route_follower_api` |
| Docking infrastructure API | `docking_infrastructure_api_tests` | `docking_infrastructure_api` |
| Runtime planner | `navigation_runtime_planner_tests` | `navigation_runtime_planner` |
| Corner-family matrix | `maneuver_corner_family_matrix_tests` | `maneuver_corner_family_matrix` |
| Rigid-body corridor | `maneuver_rigid_body_corridor_tests` | `maneuver_rigid_body_corridor` |
| Chained-limit matrix | `maneuver_chained_limit_matrix_tests` | `maneuver_chained_limit_matrix` |
| Composite proving ground legacy lab | `navigation_composite_proving_ground_tests` | `navigation_composite_proving_ground` |
| Physical maneuver search coordinator | `physical_maneuver_search_coordinator_tests` | `physical_maneuver_search_coordinator` |

When a target/test name is not in this table, inspect the current owning
`CMakeLists.txt` before issuing a command.

## Current vs legacy navigation tests

The default navigation-runtime and docking gates validate the production V2 chain:

```text
Client RoutePlan
→ exact RouteCurveSegment geometry + AcceptedManeuverProgram storage/kinematics
→ ClientRouteAutopilot
   (continuous spatial progress + RouteSpeedGuidance + CourseCaptureGuidance)
→ PredictivePilot V2
→ ordinary ShipControlState
→ GameClient::submitInput
→ server SharedShipPhysics / DynamicMotionSystem::applyLocalFrameInput
```

Tests labeled `legacy_navigation_lab` intentionally exercise retired comparison paths such as `TrajectoryFollower`, `NavigationRuntimeControlBridge`, `ShipControlAdapter`, the old CorridorCaptureGuidance path, or the old direct Assisted helper. They are retained only as opt-in diagnostic/oracle tests and must not define production V2 acceptance.

For current execution semantics, `src/game/navigation/NAVIGATION_V2_EXECUTION_ARCHITECTURE.md` has precedence over older navigation notes. `RouteFollowerApi` remains a lower-level/public compatibility seam and dedicated test surface, but the current player `ClientRouteAutopilot` does not delegate its moving course/capture composition to `RouteFollower::follow`.

## Verified navigation-runtime configure/build/test commands

Configure the standalone test tree:

```bash
cmake -S tests/navigation_runtime \
  -B build/tests/navigation_runtime \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
```

Build the current production/V2 suite:

```bash
cmake --build build/tests/navigation_runtime -j 8
```

Legacy comparison labs are `EXCLUDE_FROM_ALL` and are not built by this command. To build/run them explicitly, use:

```bash
ELITE_RUN_LEGACY_NAVIGATION_LABS=1 bash tests/navigation_runtime/run_mingw64.sh
```

Build only the current docking gates:

```bash
cmake --build build/tests/navigation_runtime \
  --target docking_advisory_tests \
           navigation_hit_volume_adapter_tests \
           accepted_maneuver_program_builder_tests \
           maneuver_program_sampler_tests \
           navigation_runtime_control_tests \
           client_route_autopilot_tests \
           navigation_v2_tunnel_proving_ground_tests \
           route_planner_api_tests \
           route_follower_api_tests \
           docking_infrastructure_api_tests \
  -j 8
```

Run those CTest cases:

```bash
ctest --test-dir build/tests/navigation_runtime \
  -R "^(docking_advisory|navigation_hit_volume_adapter|accepted_maneuver_program_builder|maneuver_program_sampler|navigation_runtime_control|client_route_autopilot|navigation_v2_tunnel_proving_ground|route_planner_api|route_follower_api|docking_infrastructure_api)$" \
  --output-on-failure
```

Run the full standalone navigation-runtime suite:

```bash
bash tests/navigation_runtime/run_mingw64.sh
```

## Verified client build/run commands

Canonical client configure:

```bash
cmake -S . -B build -G Ninja \
  -DELITE_BUILD_CLIENT=ON \
  -DELITE_BUILD_SERVER=OFF
```

Build only the game:

```bash
cmake --build build --target EliteGame -j 8
```

Run with docking log capture:

```bash
build/EliteGame.exe 2>&1 | tee build/test-logs/docking-live.log
```

## Canonical project scripts

### Docking gate

```bash
bash verify_docking.sh
```

This script currently:
1. configures `build/tests/navigation_runtime`;
2. builds the selected docking/runtime native test targets;
3. runs their CTest names;
4. configures/tests `build/tests/navigation_ruckig`;
5. runs the relevant Python architecture contracts.

Use this instead of reconstructing the same gate manually unless a specific
failed step needs isolated reproduction.

### Canonical runtime build

```bash
bash build_mingw64.sh
```

This uses `tests/helpers/build_layout.sh`, builds the canonical client and
headless server, removes obsolete duplicate runtime layouts, and verifies:

```text
build/EliteGame.exe
build/headless_server/EliteServer.exe
```

## Architecture contracts

A single known contract can be run directly, for example:

```bash
python tests/architecture_contracts/check_automatic_docking.py
```

The complete architecture block is:

```bash
bash tests/architecture_contracts/run_mingw64.sh
```

Do not assume a stale architecture script matches a changed runtime contract.
When a contract fails because it expects a retired token/API, inspect both the
production source and that contract before treating the runtime implementation
as wrong.

## Command-validation rule for future work

Before giving the user any non-trivial CMake/build/test command:

1. Identify the source file or test being exercised.
2. Inspect its current owning `CMakeLists.txt`.
3. Confirm the exact build target name.
4. Confirm which configured build tree owns that target.
5. For CTest, separately confirm the `add_test(NAME ...)` name.
6. Prefer an existing canonical project script when it already performs the
   requested gate.
7. After API changes, check sibling tests for stale callers before asking the
   user to rebuild the whole suite.
8. Never launch `EliteGame.exe` after a failed build step when validating new
   code; otherwise an old executable can masquerade as the current revision.

## Current docking binary revision marker

Current Automatic docking code prints:

```text
impl=dock-auto-20261003-predictive-pilot-v2
```

If that marker is absent from a new Automatic docking run, the running
`EliteGame.exe` does not contain the current docking implementation and the
flight log must not be used to judge the latest source changes.
