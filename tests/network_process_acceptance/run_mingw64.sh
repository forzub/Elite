#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"

if ! elite_fail_if_server_running "network process acceptance preflight"; then
    exit 1
fi

SERVER_DIR="${ELITE_TEST_SERVER_DIR:-${ROOT_DIR}/build/network_process_server}"
CLIENT_DIR="${ELITE_TEST_CLIENT_DIR:-${ROOT_DIR}/build}"
SERVER_EXE="${SERVER_DIR}/EliteServer.exe"
CLIENT_EXE="${CLIENT_DIR}/EliteGame.exe"
SERVER_LOG="${ROOT_DIR}/build/network_process_server.log"
CLIENT_LOG="${ROOT_DIR}/build/network_process_client.log"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo "Python is required for network process acceptance." >&2
    exit 1
fi

# Standalone invocation owns fresh build prerequisites. The ready harness may
# pass already-validated build directories through ELITE_TEST_*_DIR so this
# block does not rebuild the same targets twice. Never accept a merely existing
# binary as proof that it matches the current source tree.
if [[ -z "${ELITE_TEST_SERVER_DIR:-}" ]]; then
    echo "[SELFTEST] network-process stage=build-server"
    cmake -S "${ROOT_DIR}" -B "${SERVER_DIR}" -G Ninja \
        -DELITE_BUILD_CLIENT=OFF \
        -DELITE_BUILD_SERVER=ON
    cmake --build "${SERVER_DIR}" --target EliteServer
fi

if [[ -z "${ELITE_TEST_CLIENT_DIR:-}" ]]; then
    echo "[SELFTEST] network-process stage=build-client"
    cmake -S "${ROOT_DIR}" -B "${CLIENT_DIR}" -G Ninja
    cmake --build "${CLIENT_DIR}" --target EliteGame
fi

if [[ ! -x "${SERVER_EXE}" || ! -x "${CLIENT_EXE}" ]]; then
    echo "Network process acceptance requires current EliteServer.exe and EliteGame.exe builds." >&2
    exit 1
fi

PORT="$(${PYTHON_BIN} - <<'PY'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
)"
ENDPOINT="127.0.0.1:${PORT}"

rm -f "${SERVER_LOG}" "${CLIENT_LOG}"

echo "[SELFTEST] network-process stage=client-first endpoint=${ENDPOINT}"

# Deliberately start the client first. Initial connection refusal must enter
# WaitingForServer rather than terminating the process/session.
(
    cd "${CLIENT_DIR}"
    ./EliteGame.exe --self-test-remote-client "${ENDPOINT}"
) >"${CLIENT_LOG}" 2>&1 &
CLIENT_PID=$!
SERVER_PID=""

cleanup() {
    if [[ -n "${CLIENT_PID:-}" ]] && kill -0 "${CLIENT_PID}" >/dev/null 2>&1; then
        kill "${CLIENT_PID}" >/dev/null 2>&1 || true
        wait "${CLIENT_PID}" >/dev/null 2>&1 || true
    fi
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
        kill "${SERVER_PID}" >/dev/null 2>&1 || true
        wait "${SERVER_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

sleep 0.35
if ! kill -0 "${CLIENT_PID}" >/dev/null 2>&1; then
    echo "Remote client exited instead of waiting for a server that was not started yet." >&2
    cat "${CLIENT_LOG}" >&2 || true
    exit 1
fi

(
    cd "${SERVER_DIR}"
    ./EliteServer.exe --listen "${ENDPOINT}" --self-test-one-client
) >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

echo "[SELFTEST] network-process stage=server-boot"

ready=0
for _ in $(seq 1 100); do
    if grep -q "\[EliteServer\] listening endpoint=" "${SERVER_LOG}" 2>/dev/null; then
        ready=1
        break
    fi
    if ! kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
        break
    fi
    sleep 0.05
done

if [[ "${ready}" != "1" ]]; then
    echo "Network server did not reach listening state." >&2
    cat "${SERVER_LOG}" >&2 || true
    cat "${CLIENT_LOG}" >&2 || true
    exit 1
fi

# This launch intentionally bypasses the shell preflight: the first server is
# ours, and this second process exists solely to prove that EliteServer itself
# atomically rejects duplicate ownership.
set +e
SECOND_SERVER_OUTPUT="$(
    cd "${SERVER_DIR}"
    ./EliteServer.exe --self-test 2>&1
)"
SECOND_SERVER_STATUS=$?
set -e

if [[ ${SECOND_SERVER_STATUS} -eq 0 ]] ||
   [[ "${SECOND_SERVER_OUTPUT}" != *"another EliteServer instance is already running"* ]]; then
    echo "Second EliteServer instance was not rejected by the process-level singleton guard." >&2
    echo "${SECOND_SERVER_OUTPUT}" >&2
    exit 1
fi

echo "[PASS] second EliteServer process rejected by singleton guard"
echo "[SELFTEST] network-process stage=handshake"

if ! wait "${CLIENT_PID}"; then
    CLIENT_PID=""
    echo "Remote GameClient process self-test failed after waiting for server startup." >&2
    cat "${CLIENT_LOG}" >&2 || true
    cat "${SERVER_LOG}" >&2 || true
    exit 1
fi
CLIENT_PID=""

if ! wait "${SERVER_PID}"; then
    SERVER_PID=""
    echo "Network server process self-test failed." >&2
    cat "${SERVER_LOG}" >&2 || true
    cat "${CLIENT_LOG}" >&2 || true
    exit 1
fi
SERVER_PID=""
trap - EXIT

cat "${CLIENT_LOG}"
cat "${SERVER_LOG}"
echo "[PASS] client-first startup waited, then separate EliteGame/EliteServer processes exchanged authoritative TCP gameplay"
