from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

remote_h = (ROOT / "src/game/session/RemoteGameSession.h").read_text(encoding="utf-8")
remote_cpp = (ROOT / "src/game/session/RemoteGameSession.cpp").read_text(encoding="utf-8")
host_h = (ROOT / "src/game/server/NetworkServerHost.h").read_text(encoding="utf-8")
host_cpp = (ROOT / "src/game/server/NetworkServerHost.cpp").read_text(encoding="utf-8")
runtime_h = (ROOT / "src/game/server/ServerRuntime.h").read_text(encoding="utf-8")
runtime_cpp = (ROOT / "src/game/server/ServerRuntime.cpp").read_text(encoding="utf-8")
session_h = (ROOT / "src/game/network/SessionMessage.h").read_text(encoding="utf-8")
server_main = (ROOT / "src/server_main.cpp").read_text(encoding="utf-8")
client_main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
run_all = (ROOT / "tests/run_all_mingw64.sh").read_text(encoding="utf-8")

required_remote = [
    "TcpClientTransport",
    "GameClient",
    "serverFixedStepSeconds()",
]
for token in required_remote:
    if token not in remote_cpp:
        raise SystemExit(f"RemoteGameSession missing process-remote client seam: {token}")

for forbidden in [
    '#include "src/game/server/ServerRuntime',
    '#include "src/game/server/ServerWorker',
    '#include "src/game/host/LocalGameHost',
    '#include "src/game/server/GameServer',
    'std::unique_ptr<ServerRuntime>',
    'std::unique_ptr<GameServer>',
]:
    if forbidden in remote_h or forbidden in remote_cpp:
        raise SystemExit(
            f"RemoteGameSession must not own embedded authoritative runtime: {forbidden}"
        )

if "NetworkServerHost" not in host_h or "ServerRuntime" not in host_cpp:
    raise SystemExit("NetworkServerHost must own the dedicated authoritative runtime boundary")

if "attachPlayerSessionTransport(*transport)" not in host_cpp:
    raise SystemExit(
        "network admission must bind accepted transport without a client-selected EntityId"
    )

if "attachPlayerSessionTransport(*transport," in host_cpp:
    raise SystemExit(
        "network host must not pick authority by passing a connection-supplied EntityId"
    )

if "ServerRuntime(\n        const WorldParams& worldParams,\n        game::debug::IServerDebugChannel& debugChannel" not in runtime_h:
    raise SystemExit("dedicated ServerRuntime must support zero initial gameplay transports")

if "selectAvailablePlayerEntityForAdmission" not in runtime_cpp:
    raise SystemExit("server-owned admission selector is missing from runtime path")

if "double fixedStepSeconds" not in session_h:
    raise SystemExit("SessionWelcome must publish authoritative fixed-step cadence")

for token in ["--listen", "NetworkServerHost", "--self-test-one-client"]:
    if token not in server_main:
        raise SystemExit(f"EliteServer process lifecycle missing: {token}")

for token in ["--connect", "--self-test-remote-client", "configureRemoteServer"]:
    if token not in client_main:
        raise SystemExit(f"EliteGame remote process lifecycle missing: {token}")

if "NETWORK PROCESS ACCEPTANCE" not in run_all:
    raise SystemExit("ready suite must execute separate-process network acceptance")

platform_tokens = [
    '#include <windows.h>', '#include <winsock',
    '#include <sys/socket.h>', '#include <unistd.h>',
    '::recv(', '::send(', 'WSAStartup(',
]
for path, text in [
    ("RemoteGameSession", remote_h + remote_cpp),
    ("NetworkServerHost", host_h + host_cpp),
    ("ServerRuntime", runtime_h + runtime_cpp),
]:
    lower = text.lower()
    for token in platform_tokens:
        if token.lower() in lower:
            raise SystemExit(f"{path} leaked OS socket API/token: {token}")

print("[PASS] separate-process admission keeps TCP below session/runtime authority boundaries")
