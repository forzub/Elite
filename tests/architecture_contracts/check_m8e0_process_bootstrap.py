#!/usr/bin/env python3
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] M8E.0 process bootstrap: {message}", file=sys.stderr)
    raise SystemExit(1)


session_h = read("src/game/session/IGameSession.h")
remote_h = read("src/game/session/RemoteGameSession.h")
remote_cpp = read("src/game/session/RemoteGameSession.cpp")
app_h = read("src/core/Application.h")
app_cpp = read("src/core/Application.cpp")
main_cpp = read("src/main.cpp")
ui_server_h = read("src/ui/html/HtmlUiServer.h")
ui_server_cpp = read("src/ui/html/HtmlUiServer.cpp")
server_cpp = read("src/game/server/GameServer.cpp")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
loading_html = read("src/assets/webui/loading.html")
process_script = read("tests/network_process_acceptance/run_mingw64.sh")

for token in (
    "WaitingForServer",
    "retryIntervalSeconds",
    "connectOrWait()",
    "m_waitingForServer",
):
    if token not in session_h + remote_h + remote_cpp:
        fail(f"retryable client-before-server state missing: {token}")

for token in (
    "if (m_transport->connect(m_config.host, m_config.port))",
    "m_waitingForServer = true;",
    "std::max(0.10, m_config.retryIntervalSeconds)",
):
    if token not in remote_cpp:
        fail(f"RemoteGameSession does not retry initial TCP absence: {token}")

if "m_failed = true;\n        m_error = m_transport->lastError();" in remote_cpp.split("connectOrWait()", 1)[-1].split("updateSynchronization", 1)[0]:
    fail("initial TCP refusal still marks the remote session Failed")

for token in (
    "m_gameUiHttpPort = m_htmlUi.start(0, webUiRoot);",
    "makeGameUiHttpUrl(m_gameUiHttpPort",
    "std::uint16_t m_gameUiHttpPort = 0",
):
    if token not in app_cpp + app_h:
        fail(f"Application WebUI is not process-local/ephemeral: {token}")

for token in (
    "m_server.listen(port);",
    "m_server.get_local_endpoint(endpointError)",
    "m_localPort = endpoint.port();",
):
    if token not in ui_server_cpp:
        fail(f"HtmlUiServer does not expose its OS-assigned port: {token}")

if "start(std::uint16_t port" not in ui_server_h:
    fail("HtmlUiServer start signature does not accept an explicit ephemeral port request")

for token in (
    "ServerRuntime(worldParams, debugChannel, 2)",
    "ServerRuntime(worldParams, debugChannel, 1)",
    "BootstrapPlayerSpacingMeters = 50.0",
    "selectAvailablePlayerForAdmission()",
    "m_sessions.isConnectedPlayer",
):
    if token not in runtime_cpp + server_cpp:
        fail(f"dedicated two-slot player admission contract missing: {token}")

if "use an existing ordinary" in server_cpp:
    fail("production admission still documents arbitrary materialized/NPC takeover")

for token in (
    "loading.stage.waiting_server",
    "runPc286Typewriter",
    "runChineseIme",
    '["zheng zai", "正在"]',
    '["deng dai", "等待"]',
    '["fu wu qi", "服务器"]',
):
    if token not in loading_html + app_cpp:
        fail(f"localized waiting animation contract missing: {token}")

loading_json = json.loads(read("src/assets/localization/ui/common/loading.json"))
waiting = loading_json.get("strings", {}).get("loading.stage.waiting_server", {})
for locale in ("en", "ru", "zh-Hans", "es", "ja"):
    if not waiting.get(locale):
        fail(f"WAITING FOR SERVER localization missing for {locale}")

if waiting.get("zh-Hans") != "正在等待服务器":
    fail("Simplified Chinese waiting text drifted from the IME animation target")

for token in (
    "Deliberately start the client first",
    "Remote client exited instead of waiting",
    "./EliteServer.exe --listen",
):
    if token not in process_script:
        fail(f"process acceptance no longer proves client-first startup: {token}")

for token in (
    "serverWaitDeadline",
    "session.state() == game::session::GameSessionState::WaitingForServer",
    "const auto syncDeadline = Clock::now() + std::chrono::seconds(10);",
):
    if token not in main_cpp:
        fail(f"remote process self-test no longer separates server wait from synchronization: {token}")

print("[PASS] M8E.0 process-local UI, bootstrap admission and client-first waiting contracts")
