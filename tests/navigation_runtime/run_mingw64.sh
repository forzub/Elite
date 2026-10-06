#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_runtime"

echo "[NAV-RUNTIME] configure current navigation-runtime suite"
cmake     -S "${ROOT_DIR}/tests/navigation_runtime"     -B "${BUILD_DIR}"     -G Ninja     -DCMAKE_BUILD_TYPE=Release

echo "[NAV-RUNTIME] build current production/V2 tests"
# Legacy comparison labs are EXCLUDE_FROM_ALL in CMake and therefore cannot
# break the production acceptance build merely because a retired API changes.
cmake --build "${BUILD_DIR}"

echo "[NAV-RUNTIME] run current production/V2 tests"
ctest     --test-dir "${BUILD_DIR}"     -LE legacy_navigation_lab     --output-on-failure

echo "[NAV-RUNTIME] V2 tunnel diagnostic"
ctest     --test-dir "${BUILD_DIR}"     -R "^navigation_v2_tunnel_proving_ground$"     -V

if [[ "${ELITE_RUN_LEGACY_NAVIGATION_LABS:-0}" == "1" ]]; then
    echo "[NAV-RUNTIME] build explicitly requested legacy comparison labs"
    cmake --build "${BUILD_DIR}"         --target navigation_runtime_planner_tests                  maneuver_phase_gate_tests                  maneuver_tracking_controller_tests                  maneuver_corner_family_matrix_tests                  maneuver_rigid_body_corridor_tests                  maneuver_corridor_matrix_tests                  maneuver_fly_through_3d_tests                  maneuver_speed_doctrine_matrix_tests                  maneuver_chained_limit_matrix_tests                  navigation_composite_proving_ground_tests                  maneuver_program_execution_lab_tests                  corridor_capture_guidance_tests

    echo "[NAV-RUNTIME] run explicitly requested legacy comparison labs"
    ctest         --test-dir "${BUILD_DIR}"         -L legacy_navigation_lab         --output-on-failure
fi

echo "[RESULT] navigation_runtime PASS"
