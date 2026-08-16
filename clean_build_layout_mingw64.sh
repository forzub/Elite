#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"

if ! elite_fail_if_server_running "build-layout cleanup preflight"; then
    exit 1
fi

elite_cleanup_legacy_build_layout
elite_assert_unique_runtime_binaries

echo "[BUILD-LAYOUT] obsolete generated build directories removed"
echo "[BUILD-LAYOUT] runtime paths remain build/EliteGame.exe and build/headless_server/EliteServer.exe"
