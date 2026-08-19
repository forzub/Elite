#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/tests/helpers/toolchain.sh"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"

RUN_TESTS=0
FETCH_FONTS=0

usage() {
    cat <<'USAGE'
Usage: bash rebuild_all_mingw64.sh [--tests] [--fetch-fonts]

Performs a true clean rebuild of the two canonical runtime binaries:
  build/EliteGame.exe
  build/headless_server/EliteServer.exe

Options:
  --tests        Run the full tests/run_all_mingw64.sh ready gate afterwards.
  --fetch-fonts  Refresh/download the pinned Noto UI font set before rebuilding.
  -h, --help     Show this help.

The rebuild always requires all declared bundled UI fonts.
USAGE
}

while (($#)); do
    case "$1" in
        --tests)
            RUN_TESTS=1
            ;;
        --fetch-fonts)
            FETCH_FONTS=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[REBUILD][FAIL] unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if ! elite_require_ready_toolchain; then
    exit 2
fi

if ! elite_fail_if_server_running "full clean rebuild preflight"; then
    exit 1
fi

if (( FETCH_FONTS )); then
    echo "[REBUILD] refreshing pinned UI fonts"
    bash "${ROOT_DIR}/tools/fetch_ui_fonts_mingw64.sh"
fi

FONT_LOCK="${ROOT_DIR}/third_party/fonts/noto/font-lock.json"
if [[ ! -f "${FONT_LOCK}" ]]; then
    echo "[REBUILD][FAIL] missing ${FONT_LOCK#${ROOT_DIR}/}" >&2
    echo "[REBUILD][FAIL] run: bash tools/fetch_ui_fonts_mingw64.sh" >&2
    echo "[REBUILD][FAIL] or rerun this script with --fetch-fonts" >&2
    exit 1
fi

printf '\n[REBUILD] removing generated build tree: %s\n' "${ROOT_DIR}/build"
rm -rf -- "${ROOT_DIR}/build"

# Recreate canonical scratch/log directories and keep the canonical layout
# contract in one place.
elite_cleanup_legacy_build_layout

printf '\n================================================================\n'
printf 'FULL REBUILD: GRAPHICAL CLIENT\n'
printf '================================================================\n'
cmake -S "${ROOT_DIR}" -B "${ELITE_CLIENT_BUILD_DIR}" -G Ninja \
    -DELITE_BUILD_CLIENT=ON \
    -DELITE_BUILD_SERVER=OFF \
    -DELITE_REQUIRE_BUNDLED_UI_FONTS=ON
cmake --build "${ELITE_CLIENT_BUILD_DIR}" --target EliteGame

printf '\n================================================================\n'
printf 'FULL REBUILD: HEADLESS SERVER\n'
printf '================================================================\n'
cmake -S "${ROOT_DIR}" -B "${ELITE_SERVER_BUILD_DIR}" -G Ninja \
    -DELITE_BUILD_CLIENT=OFF \
    -DELITE_BUILD_SERVER=ON
cmake --build "${ELITE_SERVER_BUILD_DIR}" --target EliteServer

elite_require_canonical_runtime_binaries

printf '\n================================================================\n'
printf 'FULL REBUILD: SMOKE TESTS\n'
printf '================================================================\n'
(
    cd "${ELITE_SERVER_BUILD_DIR}"
    ./EliteServer.exe --self-test
)
(
    cd "${ELITE_CLIENT_BUILD_DIR}"
    ./EliteGame.exe --self-test-fast-universe
)

if (( RUN_TESTS )); then
    printf '\n================================================================\n'
    printf 'FULL REBUILD: READY GATE\n'
    printf '================================================================\n'
    bash "${ROOT_DIR}/tests/run_all_mingw64.sh"
fi

printf '\n================================================================\n'
printf 'FULL CLEAN REBUILD PASSED\n'
printf '================================================================\n'
printf 'Client: %s\n' "${ELITE_CLIENT_BUILD_DIR}/EliteGame.exe"
printf 'Server: %s\n' "${ELITE_SERVER_BUILD_DIR}/EliteServer.exe"
if (( RUN_TESTS )); then
    printf 'Ready gate: PASSED\n'
else
    printf 'Ready gate: not requested (use --tests)\n'
fi
