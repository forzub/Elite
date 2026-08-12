#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"Server-runtime ownership architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


host_h = read("src/game/host/LocalGameHost.h")
host_cpp = read("src/game/host/LocalGameHost.cpp")
runtime_h = read("src/game/server/ServerRuntime.h")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
session_cpp = read("src/game/host/LocalGameSession.cpp")
client_h = read("src/game/client/GameClient.h")
client_cpp = read("src/game/client/GameClient.cpp")
session_message_h = read("src/game/network/SessionMessage.h")
client_session_snapshot_h = read("src/game/simulation/ClientSessionSnapshot.h")
client_transport_h = read("src/game/network/ITransport.h")
server_transport_h = read("src/game/network/IServerTransport.h")
loopback_h = read("src/game/network/LocalLoopbackTransport.h")
runner_cpp = read("src/game/server/ServerRunner.cpp")

# Composition/client-facing host must not own or include authoritative memory.
for forbidden in (
    '#include "src/game/server/GameServer.h"',
    "std::unique_ptr<GameServer>",
    "GameServer&",
    "m_server->",
):
    if forbidden in host_h or forbidden in host_cpp:
        fail(f"LocalGameHost regained direct GameServer access: {forbidden}")

for required in (
    "std::unique_ptr<game::debug::LocalDebugSessionControl> m_debugControl",
    "std::unique_ptr<server::ServerRuntime> m_runtime",
    "std::make_unique<game::debug::LocalDebugSessionControl>()",
    "std::make_unique<server::ServerRuntime>(",
    "*m_debugControl",
    "m_runtime->advance(",
    "m_runtime->fixedStepSeconds()",
    "return *m_debugControl;",
):
    if required not in host_h and required not in host_cpp:
        fail(f"LocalGameHost no longer composes through ServerRuntime: {required}")

# Exactly the server-side runtime owns GameServer during local production play.
for required in (
    "class ServerRuntime final",
    "std::unique_ptr<GameServer> m_server",
    "std::make_unique<GameServer>()",
    "m_server->world() = worldParams",
    "game::debug::IServerDebugChannel& debugChannel",
    "transport.publishSnapshotImmediately(m_server->snapshot())",
    "m_debugChannel.publishSnapshot(m_server->snapshot())",
):
    if required not in runtime_h and required not in runtime_cpp:
        fail(f"ServerRuntime ownership/bootstrap contract is incomplete: {required}")

if "public game::debug::IDebugSessionControl" in runtime_h:
    fail("ServerRuntime regained the application-side debug facade")

if "return *m_runtime;" in host_cpp:
    fail("LocalGameHost exposes ServerRuntime itself as debug control")

# Stable session identity is server-assigned one-time bootstrap metadata, not a
# field repeated in every simulation snapshot and not a value selected in each
# client command packet.
for required in (
    "struct SessionWelcome",
    "EntityId controlledEntityId {0};",
):
    if required not in session_message_h:
        fail(f"one-time session identity contract is incomplete: {required}")

if "controlledEntityId" in client_session_snapshot_h:
    fail("stable controlled-entity identity leaked into recurring simulation snapshots")

for required in (
    "welcome.controlledEntityId = m_server->playerId()",
    "transport.publishSessionWelcomeImmediately(welcome)",
):
    if required not in runtime_cpp:
        fail(f"ServerRuntime does not publish authoritative session identity: {required}")

if "explicit GameClient(ITransport& transport);" not in client_h:
    fail("GameClient constructor regained an externally supplied player EntityId")

if "sendClientMessage(m_playerId" in client_cpp:
    fail("GameClient resumed attaching client-selected EntityId to command packets")

for required in (
    "m_transport.receiveSessionWelcome(welcome)",
    "m_playerId = welcome.controlledEntityId",
    "m_hasPlayerIdentity = true",
):
    if required not in client_cpp:
        fail(f"GameClient no longer derives identity from server welcome: {required}")

if "m_host->playerId()" in session_cpp:
    fail("LocalGameSession still asks the authoritative host for player identity")

if "return m_client->playerId();" not in session_cpp:
    fail("LocalGameSession player identity is not sourced from synchronized client state")

if "receiveSessionWelcome(" not in client_transport_h:
    fail("client transport lost the one-time session welcome channel")

if "publishSessionWelcomeImmediately(" not in server_transport_h:
    fail("server transport lost the one-time session welcome channel")

if "std::queue<std::pair<EntityId" in loopback_h:
    fail("loopback command queue regained client-selected EntityId ownership")

for text, label in (
    (client_transport_h, "ITransport"),
    (server_transport_h, "IServerTransport"),
    (loopback_h, "LocalLoopbackTransport"),
):
    if re.search(r"sendClientMessage\s*\(\s*EntityId", text, re.S):
        fail(f"{label} lets the client select command authority identity")
    if re.search(r"receiveClientMessage\s*\(\s*EntityId&", text, re.S):
        fail(f"{label} transports client-selected command authority identity")

for required in (
    "m_transport.receiveClientMessage(clientMessage)",
    "m_server.playerId()",
    "m_server.receiveClientMessage(",
):
    if required not in runner_cpp:
        fail(f"server-side local-session identity binding is incomplete: {required}")

print("[PASS] server runtime ownership + server-assigned client identity")
