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

echo "[DOCK-VERIFY] build docking + accepted-program + live-control native gates"
cmake --build "${TEST_BUILD_DIR}" \
    --target docking_advisory_tests \
             accepted_maneuver_program_builder_tests \
             navigation_runtime_control_tests \
             maneuver_tracking_controller_tests \
    -j 8

echo "[DOCK-VERIFY] run native docking + execution gates"
ctest --test-dir "${TEST_BUILD_DIR}" \
    -R "^(docking_advisory|accepted_maneuver_program_builder|navigation_runtime_control|maneuver_tracking_controller)$" \
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
python "${ROOT_DIR}/tests/architecture_contracts/check_navigation_live_runtime_control.py"

echo "[DOCK-VERIFY] PASS"
