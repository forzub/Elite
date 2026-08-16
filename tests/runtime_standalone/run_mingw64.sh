#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"

if ! elite_fail_if_server_running "standalone runtime preflight"; then
    exit 1
fi

SERVER_DIR="${ELITE_SERVER_BUILD_DIR}"
CLIENT_DIR="${ELITE_CLIENT_BUILD_DIR}"
SERVER_EXE="${SERVER_DIR}/EliteServer.exe"
CLIENT_EXE="${CLIENT_DIR}/EliteGame.exe"

elite_cleanup_legacy_build_layout

if [[ "${ELITE_CANONICAL_BUILDS_READY:-0}" != "1" ]]; then
    echo "[SELFTEST] standalone-runtime stage=build-canonical-runtime"
    elite_build_canonical_runtime
fi

elite_require_canonical_runtime_binaries

# These are compiler-runtime canaries, not the complete dependency list. The
# build rule itself discovers/copies dependencies recursively.
for dir in "${SERVER_DIR}" "${CLIENT_DIR}"; do
    for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
        if [[ ! -f "${dir}/${dll}" ]]; then
            echo "Missing staged MinGW runtime: ${dir}/${dll}" >&2
            exit 1
        fi
    done
done

if ! command -v cygpath >/dev/null 2>&1; then
    echo "cygpath is required for clean Windows runtime acceptance." >&2
    exit 1
fi

WINDOWS_ROOT_WIN="${SYSTEMROOT:-${SystemRoot:-C:\\Windows}}"
WINDOWS_ROOT_POSIX="$(cygpath -u "${WINDOWS_ROOT_WIN}")"
CLEAN_WINDOWS_PATH="${WINDOWS_ROOT_POSIX}/System32:${WINDOWS_ROOT_POSIX}"

run_with_clean_windows_path() {
    local workdir="$1"
    local executable="$2"
    shift 2

    (
        cd "${workdir}"
        PATH="${CLEAN_WINDOWS_PATH}" "./${executable}" "$@"
    )
}

echo "[SELFTEST] standalone-runtime stage=headless-server-clean-path"
run_with_clean_windows_path "${SERVER_DIR}" EliteServer.exe --self-test

echo "[SELFTEST] standalone-runtime stage=client-clean-path"
run_with_clean_windows_path "${CLIENT_DIR}" EliteGame.exe --self-test-fast-universe

echo "[SELFTEST] standalone-runtime stage=headless-server-foreign-cwd"
(
    cd "${ROOT_DIR}"
    PATH="${CLEAN_WINDOWS_PATH}" "${SERVER_EXE}" --self-test
)

echo "[SELFTEST] standalone-runtime stage=client-foreign-cwd"
(
    cd "${ROOT_DIR}"
    PATH="${CLEAN_WINDOWS_PATH}" "${CLIENT_EXE}" --self-test-fast-universe
)

echo "[PASS] canonical EliteServer and EliteGame launch without MinGW/MSYS2 on PATH or cwd assumptions"
