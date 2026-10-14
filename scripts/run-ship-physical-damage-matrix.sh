#!/usr/bin/env bash
set -euo pipefail

JOBS="${JOBS:-8}"
BUILD_DIR="build/tests/navigation_runtime"
LOG_DIR="${ELITE_SHIP_PHYSICAL_DAMAGE_LOG_DIR:-D:/tmp/elite/ship_physical_damage}"

echo "[ship-physical-damage] configure"
cmake -S tests/navigation_runtime -B "${BUILD_DIR}" -G Ninja

echo "[ship-physical-damage] build"
cmake --build "${BUILD_DIR}" \
  --target ship_control_execution_gateway_tests \
           ship_physical_control_models_tests \
           ship_physical_actuator_plant_tests \
           ship_physical_damage_matrix_tests \
  -j "${JOBS}"

echo "[ship-physical-damage] run"
ELITE_SHIP_PHYSICAL_DAMAGE_LOG_DIR="${LOG_DIR}" \
ctest --test-dir "${BUILD_DIR}" \
  -R "^(ship_control_execution_gateway|ship_physical_control_models|ship_physical_actuator_plant|ship_physical_damage_matrix)$" \
  --output-on-failure

echo "[ship-physical-damage] logs"
echo "  ${LOG_DIR}/ship_physical_damage_matrix.csv"
echo "  ${LOG_DIR}/ship_physical_damage_summary.log"
