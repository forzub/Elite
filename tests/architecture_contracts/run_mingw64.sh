#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/architecture_contracts"
elite_require_build_toolchain


if ! elite_require_python; then
    echo "Python 3 is required for this test block." >&2
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"

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
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_reconnect_control_epoch.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_canonical_build_layout.py"
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
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_navigation_workspace_architecture.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_navigation_execution_asset.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_trajectory_predictor_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_navigation_guidance_layer.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_navigation_foundation_lock.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_geometric_path_planner.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_trajectory_generator.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_manual_guidance_tunnel.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_guidance_test_geometry.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_recent_navigation_regressions.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_model_asset_editor.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_ui_platform_fonts.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_ui_component_kit.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_account_social_shell.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_session_menu_shell.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_client_service_ui_polish.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_service_panel_shell.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_presentation_transition_atomicity.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_render_surface_ownership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_native_compositor_handoff.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_single_surface_session_presentation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_function_key_presentation_route.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_direct_navigation_no_crossfade.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_headless_server_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_headless_server_executable.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_transport_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_runtime_ownership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_session_foundation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_account_identity_handshake.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_authentication_admission.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_account_auth_polish.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_session_lifecycle_split.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_loading_ui_no_gl_swap.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_bootstrap_responsiveness.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_transport_fanout.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_client_local_identity.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multiplayer_session_navigation.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_replication_interest_retention.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_wire_protocol_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_wire_data_schema.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_tcp_wire_transport.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_network_process_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_single_instance.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_m8e0_process_bootstrap.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_session_failure_recovery.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_game_webview_embedding.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_glfw_win32_event_pump_contract.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_multi_process_focus_contract.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_case_sensitive_project_includes.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_runtime_trace_header.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_no_hardcoded_dev_paths.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_debug_session_boundary.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_server_worker_thread.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_runtime_policy_boundaries.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_motion_lab.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_interplanetary_transfer_lab.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_signal_reception_snapshot.py"

cmake \
    -S "${ROOT_DIR}/tests/architecture_contracts" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure

python3 tests/architecture_contracts/check_navigation_panel_functionality.py
