#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

ARCH_BUILD_DIR="${ROOT_DIR}/build/tests/architecture_contracts"

echo "[MODE-VERIFY] repo=${ROOT_DIR}"
echo "[MODE-VERIFY] head=$(git rev-parse --short HEAD)"

echo "[MODE-VERIFY] configure architecture-contract tests"
cmake -S "${ROOT_DIR}/tests/architecture_contracts" \
    -B "${ARCH_BUILD_DIR}" \
    -G Ninja

echo "[MODE-VERIFY] build focused native mode/flight contracts"
cmake --build "${ARCH_BUILD_DIR}" \
    --target local_flight_control_contract_tests client_preferences_store_contract_tests wire_protocol_contract_tests \
    -j 8

echo "[MODE-VERIFY] run focused native contracts"
ctest --test-dir "${ARCH_BUILD_DIR}" \
    -R "^(local_flight_control_contracts|client_preferences_store_contracts|wire_protocol_contracts)$" \
    --output-on-failure

echo "[MODE-VERIFY] run static architecture gates"
python "${ROOT_DIR}/tests/architecture_contracts/check_local_flight_control.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_client_mode_state.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_mode_state.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_localization_boundary.py"
python "${ROOT_DIR}/tests/architecture_contracts/check_wire_data_schema.py"

echo "[MODE-VERIFY] PASS"
