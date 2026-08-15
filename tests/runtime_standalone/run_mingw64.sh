#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"

if ! elite_fail_if_server_running "standalone runtime preflight"; then
    exit 1
fi

SERVER_DIR="${ELITE_TEST_SERVER_DIR:-${ROOT_DIR}/build/headless_server}"
CLIENT_DIR="${ELITE_TEST_CLIENT_DIR:-${ROOT_DIR}/build}"
SERVER_EXE="${SERVER_DIR}/EliteServer.exe"
CLIENT_EXE="${CLIENT_DIR}/EliteGame.exe"

for exe in "${SERVER_EXE}" "${CLIENT_EXE}"; do
    if [[ ! -x "${exe}" ]]; then
        echo "Standalone runtime acceptance requires built EliteServer.exe and EliteGame.exe." >&2
        exit 1
    fi
done

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

    # Invoke the native PE executable directly from MSYS2 while deliberately
    # removing every MSYS2/MinGW directory from PATH. MSYS2 converts this
    # POSIX path list to a native Windows PATH for the child process. The
    # Windows loader may therefore use only the executable directory plus the
    # normal Windows system locations for DLL resolution.
    (
        cd "${workdir}"
        PATH="${CLEAN_WINDOWS_PATH}" "./${executable}" "$@"
    )
}

echo "[SELFTEST] standalone-runtime stage=headless-server-clean-path"
run_with_clean_windows_path "${SERVER_DIR}" EliteServer.exe --self-test

echo "[SELFTEST] standalone-runtime stage=client-clean-path"
run_with_clean_windows_path "${CLIENT_DIR}" EliteGame.exe --self-test-fast-universe

# Runtime assets are staged beside each executable. Launch once from the
# repository root as well so the acceptance contract catches accidental
# dependence on the shell's current working directory.
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

echo "[PASS] EliteServer and EliteGame launch without MinGW/MSYS2 on PATH or cwd assumptions"
