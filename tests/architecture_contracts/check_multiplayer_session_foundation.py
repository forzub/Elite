#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8")

def fail(message):
    print(f"[FAIL] {message}", file=sys.stderr)
    raise SystemExit(1)

session_message = read("src/game/network/SessionMessage.h")
registry = read("src/game/server/ServerSessionRegistry.h")
server_h = read("src/game/server/GameServer.h")
server_cpp = read("src/game/server/GameServer.cpp")
runner_h = read("src/game/server/ServerRunner.h")
runner_cpp = read("src/game/server/ServerRunner.cpp")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
sim_h = read("src/game/simulation/GameSimulation.h")
sim_cpp = read("src/game/simulation/GameSimulation.cpp")

for token in (
    "struct ServerSessionId",
    "ServerSessionId sessionId {}",
    "PlayerId playerId {}",
    "ShipInstanceId controlledShipInstanceId = 0",
    "EntityId controlledEntityId {0}",
):
    if token not in session_message:
        fail(f"session protocol foundation missing: {token}")

for token in (
    "class ServerSessionRegistry",
    "create(PlayerId playerId)",
    "player(",
    "disconnect(",
    "reconnect(",
    "isConnectedPlayer(PlayerId playerId)",
):
    if token not in registry:
        fail(f"server session registry missing: {token}")

for forbidden in (
    "#include <windows.h>",
    "#include <winsock2.h>",
    "#include <unistd.h>",
    "#include <sys/socket.h>",
    "HWND ",
    "HANDLE ",
):
    if forbidden.lower() in registry.lower():
        fail(f"platform transport detail leaked into session authority: {forbidden}")

for token in (
    "createPlayerSession(",
    "disconnectPlayerSession(",
    "controlledEntityForSession(",
    "game::server::ServerSessionRegistry m_sessions",
):
    if token not in server_h:
        fail(f"GameServer session authority API missing: {token}")

for token in (
    "controlledEntityForSession(sessionId)",
    "m_queueDiagnostics.rejectedSessionMessages",
    "submitCommand(controlledEntityId, payload)",
    "m_pendingClientShipCommands[controlledEntityId.value]",
):
    if token not in server_cpp:
        fail(f"session-to-entity command routing missing: {token}")

if "receiveClientMessage(\n    EntityId playerId" in server_cpp:
    fail("GameServer still accepts a caller-selected player EntityId for client messages")

for token in (
    "struct ServerTransportBinding",
    "game::network::ServerSessionId sessionId",
    "std::vector<ServerTransportBinding> m_transports",
):
    if token not in runner_h:
        fail(f"ServerRunner is not connection-session bound: {token}")

for token in (
    "m_server.receiveClientMessage(",
    "binding.sessionId",
):
    if token not in runner_cpp:
        fail(f"ServerRunner does not route through authoritative session identity: {token}")

if "m_server.playerId(),\n            clientMessage" in runner_cpp:
    fail("ServerRunner still maps every connection to the legacy singleton player")

for token in (
    "createPlayerSession(m_server->primaryPlayerIdentity())",
    "publishSessionBootstrap(transport, m_primarySessionId)",
    "welcome.sessionId = sessionId",
    "welcome.playerId = playerId",
    "welcome.controlledShipInstanceId = controlledShipInstanceId",
    "welcome.controlledEntityId = controlledEntityId",
):
    if token not in runtime_cpp:
        fail(f"runtime bootstrap is not session-registry owned: {token}")

for token in (
    "setPlayerControlled(EntityId id, bool controlled)",
    "isPlayerControlled(EntityId id) const noexcept",
    "m_playerControlledShipIds",
):
    if token not in sim_h:
        fail(f"simulation multi-player ownership set missing: {token}")

for token in (
    "isPlayerControlled(id)",
    "m_playerControlledShipIds",
):
    if token not in sim_cpp:
        fail(f"simulation still depends on one player identity: {token}")

# Legacy primary-player compatibility may still exist in isolated debug/local
# APIs, but multiplayer authority and navigation must not depend on it.
for fragment in (
    "Ship& ship = *shipPtr;\n            if (id == m_playerId)",
    "if (!(id == m_playerId))\n        {\n            broadphaseQuery",
    "id == m_playerId,\n                anchors",
):
    if fragment in sim_cpp:
        fail("NPC/activation authority still special-cases only m_playerId")

if "m_playerNavigation" in server_h or "m_playerNavigation" in server_cpp:
    fail("GameServer still owns singleton player navigation instead of session-derived navigation")

print("[PASS] multiplayer server-session authority foundation")
