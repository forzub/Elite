#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

TEST_BUILD_DIR="${ROOT_DIR}/build/tests/navigation_runtime"

echo "[DOCK-VERIFY] repo=${ROOT_DIR}"
echo "[DOCK-VERIFY] head=$(git rev-parse --short HEAD)"
echo "[DOCK-VERIFY] configure standalone navigation-runtime tests"

cmake -S "${ROOT_DIR}/tests/navigation_runtime" \
    -B "${TEST_BUILD_DIR}" \
    -G Ninja

echo "[DOCK-VERIFY] build docking + exact-geometry + accepted-program + sampler + live-control native gates"
cmake --build "${TEST_BUILD_DIR}" \
    --target docking_advisory_tests \
             navigation_hit_volume_adapter_tests \
             accepted_maneuver_program_builder_tests \
             maneuver_program_sampler_tests \
             navigation_runtime_control_tests \
             maneuver_tracking_controller_tests \
             route_planner_api_tests \
             route_follower_api_tests \
             docking_infrastructure_api_tests \
    -j 8

echo "[DOCK-VERIFY] run native docking + execution gates"
ctest --test-dir "${TEST_BUILD_DIR}" \
    -R "^(docking_advisory|navigation_hit_volume_adapter|accepted_maneuver_program_builder|maneuver_program_sampler|navigation_runtime_control|maneuver_tracking_controller|route_planner_api|route_follower_api|docking_infrastructure_api)$" \
    --output-on-failure

echo "[DOCK-VERIFY] configure rotating-terminal trajectory gate"
RUCKIG_BUILD_DIR="${ROOT_DIR}/build/tests/navigation_ruckig"
cmake -S "${ROOT_DIR}/tests/navigation_ruckig" \
    -B "${RUCKIG_BUILD_DIR}" \
    -G Ninja
cmake --build "${RUCKIG_BUILD_DIR}" \
    --target trajectory_generator_angular_tests \
    -j 8
ctest --test-dir "${RUCKIG_BUILD_DIR}" \
    -R "^trajectory_generator_angular$" \
    --output-on-failure

echo "[DOCK-VERIFY] run static manual + automatic + live-control contracts"
python "${ROOT_DIR}/tests/architecture_contracts/check_manual_docking_advisory.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_automatic_docking.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_docking_module_boundaries.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_local_flight_control.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_navigation_live_runtime_control.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_navigation_api_purity.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_hub_guidance_test_geometry.py"
python "${ROOT_DIR}/tests/system_map/check_object_overlay.py"

echo "[DOCK-VERIFY] PASS"
