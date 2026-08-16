#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_CLIENT_BUILD_DIR}"

if ! elite_require_python; then
    echo "Python 3 is required for this test block." >&2
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"

"${PYTHON_BIN}" "${ROOT_DIR}/tests/client_acceptance/check_architecture.py"

elite_cleanup_legacy_build_layout
elite_build_canonical_client
elite_assert_unique_runtime_binaries

(
    cd "${BUILD_DIR}"
    ./EliteGame.exe --self-test-client-acceptance
)
