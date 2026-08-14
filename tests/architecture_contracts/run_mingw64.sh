#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/architecture_contract_tests"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo "Python is required for the architecture contract check." >&2
    exit 1
fi

"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_architecture.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_detail_revision.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_system_membership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_coordinate_frames.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_kinematic_frames.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_travel_frame_ownership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_local_flight_control.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_active_system_context.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_spatial_domain.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_frame_presentation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_tactical_prediction.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_local_predicted_presentation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_presentation_pipeline.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_prediction_reconciliation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_replication_snapshot_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_replication_static_definitions.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_system_map_celestial.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_system_map_ships.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_system_map_infrastructure.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_detail_map_migration.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_hub_map_migration.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_galaxy_map_catalog.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_static_star_atlas_ownership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_sky_culture_catalogs.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_localization_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_headless_server_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_headless_server_executable.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_transport_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_runtime_ownership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_session_foundation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_transport_fanout.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_client_local_identity.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_session_navigation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_replication_interest_retention.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_debug_session_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_worker_thread.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_runtime_policy_boundaries.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_motion_lab.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_signal_reception_snapshot.py"

cmake \
    -S "${ROOT_DIR}/tests/architecture_contracts" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
