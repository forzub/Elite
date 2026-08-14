#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SERVER_EXE="${ROOT_DIR}/build/headless_server/EliteServer.exe"
CLIENT_EXE="${ROOT_DIR}/build/EliteGame.exe"
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

if [[ ! -x "${SERVER_EXE}" || ! -x "${CLIENT_EXE}" ]]; then
    echo "EliteServer.exe and EliteGame.exe must be built before process acceptance." >&2
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

(
    cd "${ROOT_DIR}/build/headless_server"
    ./EliteServer.exe --listen "${ENDPOINT}" --self-test-one-client
) >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

cleanup() {
    if kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
        kill "${SERVER_PID}" >/dev/null 2>&1 || true
        wait "${SERVER_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

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
    exit 1
fi

if ! (
    cd "${ROOT_DIR}/build"
    ./EliteGame.exe --self-test-remote-client "${ENDPOINT}"
) >"${CLIENT_LOG}" 2>&1; then
    echo "Remote GameClient process self-test failed." >&2
    cat "${CLIENT_LOG}" >&2 || true
    cat "${SERVER_LOG}" >&2 || true
    exit 1
fi

if ! wait "${SERVER_PID}"; then
    echo "Network server process self-test failed." >&2
    cat "${SERVER_LOG}" >&2 || true
    cat "${CLIENT_LOG}" >&2 || true
    exit 1
fi
trap - EXIT

cat "${CLIENT_LOG}"
cat "${SERVER_LOG}"
echo "[PASS] separate EliteGame/EliteServer processes exchanged authoritative TCP gameplay"
