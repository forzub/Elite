#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] multiplayer transport fan-out: {message}", file=sys.stderr)
    raise SystemExit(1)


runner_h = read("src/game/server/ServerRunner.h")
runner_cpp = read("src/game/server/ServerRunner.cpp")
runtime_h = read("src/game/server/ServerRuntime.h")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
server_h = read("src/game/server/GameServer.h")
server_cpp = read("src/game/server/GameServer.cpp")
headless = read("src/game/server/HeadlessServerEndpoints.h")
server_main = read("src/server_main.cpp")

for token in (
    "struct ServerTransportBinding",
    "std::vector<ServerTransportBinding> m_transports",
    "bool attachTransport(",
    "bool detachTransport(",
    "std::size_t transportCount() const noexcept",
):
    if token not in runner_h:
        fail(f"ServerRunner lacks multi-endpoint binding seam: {token}")

for token in (
    "for (auto& binding : m_transports)",
    "receiveInboundMessages(binding)",
    "binding.sessionId",
    "m_server.receiveClientMessage(",
    "m_server.copySparseSnapshotForSession(",
    "m_server.popMapResponse(responseSessionId, mapResponse)",
    "findBinding(responseSessionId)",
):
    if token not in runner_cpp:
        fail(f"ServerRunner fan-out/fan-in routing incomplete: {token}")

# One authoritative fixed step must serve every connection. Never regress to
# one GameServer::update() per transport/session.
if runner_cpp.count("m_server.update(m_policy.fixedStepSeconds)") != 1:
    fail("ServerRunner no longer has exactly one authoritative update call")

run_step = runner_cpp.find("void ServerRunner::runFixedStep()")
if run_step < 0:
    fail("could not locate ServerRunner::runFixedStep")
run_step_text = runner_cpp[run_step:]
update_pos = run_step_text.find("m_server.update(m_policy.fixedStepSeconds)")
receive_pos = run_step_text.find("receiveInboundMessages(binding)")
publish_pos = run_step_text.find("publishOutboundMessages()")
if min(update_pos, receive_pos, publish_pos) < 0 or not (receive_pos < update_pos < publish_pos):
    fail("multi-session fixed-step ordering is not inbound -> one simulation -> outbound")

for token in (
    "attachPlayerSessionTransport(",
    "detachPlayerSessionTransport(",
    "connectedPlayerSessionCount() const noexcept",
):
    if token not in runtime_h:
        fail(f"ServerRuntime admission/lifecycle API missing: {token}")

for token in (
    "m_server->createPlayerSession(playerId)",
    "m_runner->attachTransport(transport, sessionId)",
    "publishSessionBootstrap(transport, sessionId)",
    "m_runner->detachTransport(sessionId)",
    "m_server->disconnectPlayerSession(sessionId)",
):
    if token not in runtime_cpp:
        fail(f"ServerRuntime does not bind transport lifecycle to session authority: {token}")

for token in (
    "copySnapshotForSession(",
    "PendingSessionMapRequest",
    "CompletedSessionMapResponse",
    "game::network::ServerSessionId sessionId",
    "navigationStateForEntity(",
    "navigationStateForSession(",
):
    if token not in server_h:
        fail(f"GameServer per-session replication/map envelope missing: {token}")

for token in (
    "controlledEntityForSession(sessionId)",
    "outSnapshot.session.playerNavigation = sessionNavigation",
    "navigationStateForSession(sessionId, sessionNavigation)",
    "pending.sessionId = sessionId",
    "completed.sessionId = sessionId",
    "queueMapResponse(sessionId",
):
    if token not in server_cpp:
        fail(f"GameServer does not preserve session ownership through responses: {token}")

if re.search(r"std::deque\s*<\s*game::network::MapResponse\s*>", server_h):
    fail("map responses lost destination session identity")

for forbidden in (
    "#include <windows.h>",
    "#include <winsock2.h>",
    "#include <sys/socket.h>",
    "SOCKET ",
    "HANDLE ",
):
    for text, label in (
        (runner_h, "ServerRunner.h"),
        (runner_cpp, "ServerRunner.cpp"),
        (runtime_h, "ServerRuntime.h"),
        (runtime_cpp, "ServerRuntime.cpp"),
    ):
        if forbidden in text:
            fail(f"platform socket/process detail leaked into {label}: {forbidden}")

for token in (
    "enqueueClientMessage(",
    "enqueueMapRequest(",
    "enqueueTimeSyncRequest(",
    "latestMapResponse()",
    "latestTimeSyncResponse()",
):
    if token not in headless:
        fail(f"headless self-test endpoint cannot drive multi-session protocol: {token}")

for token in (
    "game::server::HeadlessServerTransport transportA",
    "game::server::HeadlessServerTransport transportB",
    "runtime.attachPlayerSessionTransport(",
    "runtime.connectedPlayerSessionCount() != 2",
    "controlA.controlTick = 101",
    "controlB.controlTick = 202",
    "acknowledgedControlTick != 101",
    "acknowledgedControlTick != 202",
    "mapResponseRequestId(transportA.latestMapResponse()) != 1001",
    "mapResponseRequestId(transportB.latestMapResponse()) != 2002",
    "runtime.detachPlayerSessionTransport(sessionB)",
):
    if token not in server_main:
        fail(f"headless executable lost two-session authoritative smoke: {token}")

print("[PASS] two transport sessions share one authoritative runtime with isolated routing")
