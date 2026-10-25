#!/usr/bin/env bash
# Native MSYS2 MinGW64: one command to reproduce all focused Newtonian
# physics/HUD checks and save build, test and quantitative reports to D:/tmp.
# Does not pull, reset, delete build trees, start servers or edit sources.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -d /d ]]; then
    LOGDIR=/d/tmp
else
    LOGDIR="${TMPDIR:-/tmp}/elite_newtonian"
fi
mkdir -p "$LOGDIR"

echo "[NEWTONIAN] Repo: $ROOT"
echo "[NEWTONIAN] All reports: $LOGDIR"
echo "[NEWTONIAN] Build focused real-plant tests..."

cmake --build build/tests/navigation_runtime \
    --target \
        newtonian_physics_diagnostics_tests \
        ship_power_bus_execution_tests \
        newtonian_rcs_physical_tests \
        ship_physical_control_models_tests \
        newtonian_execution_authority_tests \
        ship_control_execution_gateway_tests \
        engine_thrust_hud_presentation_tests \
        ship_control_behavior_oracle_tests \
    -j 6 2>&1 | tee "$LOGDIR/elite_newtonian_build.log"

echo "[NEWTONIAN] Run reproducible physical and presentation checks..."
ctest --test-dir build/tests/navigation_runtime \
    -R '^(ship_power_bus_execution|newtonian_physics_diagnostics|newtonian_rcs_physical|ship_physical_control_models|newtonian_execution_authority|ship_control_execution_gateway|engine_thrust_hud_presentation|ship_control_behavior_oracle)$' \
    --output-on-failure 2>&1 | tee "$LOGDIR/elite_newtonian_ctest.log"

echo "[NEWTONIAN] PASS. Review:"
echo "  $LOGDIR/elite_newtonian_summary.txt"
echo "  $LOGDIR/elite_newtonian_matrix.csv"
echo "  $LOGDIR/elite_newtonian_nozzles.csv"
echo "  $LOGDIR/elite_newtonian_build.log"
echo "  $LOGDIR/elite_newtonian_ctest.log"
echo "[NEWTONIAN] For ACTUAL in-game engine data, build and launch"
echo "            the server/client, select Newtonian I/Y/X,"
echo "            and then send D:/tmp/elite_newtonian_runtime.csv."
