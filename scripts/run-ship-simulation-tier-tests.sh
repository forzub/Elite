#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-contracts}"
JOBS="${JOBS:-8}"

case "${MODE}" in
  contracts)
    cmake --build build --target EliteGame -j "${JOBS}"
    cmake --build build/tests/navigation_runtime \
      --target ship_state_tests \
               ship_mass_properties_tests \
               ship_simulation_tier_tests \
               npc_formation_goal_tests \
               ship_physical_control_models_tests \
               ship_control_execution_gateway_tests \
      -j "${JOBS}"

    ctest --test-dir build/tests/navigation_runtime \
      -R "^(ship_state|ship_mass_properties|ship_simulation_tier|npc_formation_goal|ship_physical_control_models|ship_control_execution_gateway)$" \
      --output-on-failure
    ;;

  shell500)
    echo "[ship-tier-test] 500 total: 500 TrafficShell, visible formation"
    exec env ELITE_500_SHIPS_TEST=1 ./build/EliteGame.exe
    ;;

  mixed500)
    echo "[ship-tier-test] 500 total: 24 materialized + 476 TrafficShell"
    exec env ELITE_500_MATERIALIZED_SHIPS_TEST=1 ./build/EliteGame.exe
    ;;

  tier)
    echo "[ship-tier-test] standalone lifecycle: shell -> materialized -> derelict -> wreck shell"
    exec env ELITE_SHIP_TIER_TRANSITION_TEST=1 ./build/EliteGame.exe
    ;;

  combined)
    echo "[ship-tier-test] physical state flight + 500 shells + standalone lifecycle"
    exec env \
      ELITE_SHIP_STATE_FLIGHT_TEST=1 \
      ELITE_500_SHIPS_TEST=1 \
      ELITE_SHIP_TIER_TRANSITION_TEST=1 \
      ./build/EliteGame.exe
    ;;

  fullstress)
    echo "[ship-tier-test] WARNING: intentional 500 FULL materialized torture test."
    echo "[ship-tier-test] This may be very slow and is NOT the normal scalability path."
    exec env ELITE_500_FULL_MATERIALIZED_STRESS=1 ./build/EliteGame.exe
    ;;

  *)
    echo "Usage: $0 {contracts|shell500|mixed500|tier|combined|fullstress}" >&2
    exit 2
    ;;
esac
