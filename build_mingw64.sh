#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"

if ! elite_require_ready_toolchain; then
    exit 2
fi

if ! elite_fail_if_server_running "canonical build preflight"; then
    exit 1
fi

elite_cleanup_legacy_build_layout
elite_build_canonical_runtime
elite_require_canonical_runtime_binaries

echo "[BUILD-LAYOUT] canonical client: build/EliteGame.exe"
echo "[BUILD-LAYOUT] canonical server: build/headless_server/EliteServer.exe"
