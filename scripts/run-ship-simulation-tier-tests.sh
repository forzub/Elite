#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-contracts}"
JOBS="${JOBS:-8}"

SOURCE_SHA="$(git rev-parse HEAD)"
SOURCE_SHORT_SHA="$(git rev-parse --short=12 HEAD)"
if [[ -n "$(git status --porcelain)" ]]; then
  SOURCE_DIRTY=1
else
  SOURCE_DIRTY=0
fi

echo "[ship-tier-test] source_sha=${SOURCE_SHA} dirty=${SOURCE_DIRTY} mode=${MODE}"

build_game() {
  echo "[ship-tier-test] building EliteGame from ${SOURCE_SHORT_SHA}"
  cmake --build build --target EliteGame -j "${JOBS}"
}

case "${MODE}" in
  contracts)
    build_game

    cmake --build build/tests/navigation_runtime \
      --target ship_state_tests \
               ship_mass_properties_tests \
               ship_simulation_tier_tests \
               ship_simulation_detail_policy_tests \
               npc_formation_goal_tests \
               ship_physical_control_models_tests \
               ship_control_execution_gateway_tests \
      -j "${JOBS}"

    ctest --test-dir build/tests/navigation_runtime \
      -R "^(ship_state|ship_mass_properties|ship_simulation_tier|ship_simulation_detail_policy|npc_formation_goal|ship_physical_control_models|ship_control_execution_gateway)$" \
      --output-on-failure

    cmake --build build/tests/architecture_contracts \
      --target system_map_ship_sampling_contract_tests \
               detail_map_runtime_sampling_contract_tests \
               remote_ship_presentation_contract_tests \
               replication_interest_contract_tests \
               wire_data_plane_contract_tests \
               client_observation_interest_contract_tests \
      -j "${JOBS}"

    ctest --test-dir build/tests/architecture_contracts \
      -R "^(system_map_ship_sampling_contracts|detail_map_runtime_sampling_contracts|remote_ship_presentation_contracts|replication_interest_contracts|wire_data_plane_contracts|client_observation_interest_contracts)$" \
      --output-on-failure
    ;;

  shell500)
    build_game
    echo "[ship-tier-test] BASELINE: 500 RouteOnly TrafficShell; automatic observer LOD disabled"
    exec env \
      ELITE_500_SHIPS_TEST=1 \
      ELITE_DISABLE_AUTO_SHIP_DETAIL=1 \
      ./build/EliteGame.exe
    ;;

  budget500)
    build_game
    echo "[ship-tier-test] REAL BUDGET: 500 starting shells; observer-driven MotionFidelity enabled"
    exec env ELITE_500_SHIPS_TEST=1 ./build/EliteGame.exe
    ;;

  mixed500)
    build_game
    echo "[ship-tier-test] COMPATIBILITY: start 24 materialized + 476 shells; automatic LOD may rebalance them"
    exec env ELITE_500_MATERIALIZED_SHIPS_TEST=1 ./build/EliteGame.exe
    ;;

  tier)
    build_game
    echo "[ship-tier-test] LIFECYCLE ONLY: shell -> materialized -> derelict -> wreck shell"
    exec env \
      ELITE_SHIP_TIER_TRANSITION_TEST=1 \
      ELITE_DISABLE_AUTO_SHIP_DETAIL=1 \
      ./build/EliteGame.exe
    ;;

  fullstress)
    build_game
    echo "[ship-tier-test] HARD CEILING: 500 FullControl materialized ships."
    echo "[ship-tier-test] This is intentionally NOT the normal population path."
    exec env ELITE_500_FULL_MATERIALIZED_STRESS=1 ./build/EliteGame.exe
    ;;

  *)
    echo "Usage: $0 {contracts|shell500|budget500|mixed500|tier|fullstress}" >&2
    exit 2
    ;;
esac
