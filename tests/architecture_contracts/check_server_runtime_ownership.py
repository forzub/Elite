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
worker_h = read("src/game/server/ServerWorker.h")
worker_cpp = read("src/game/server/ServerWorker.cpp")
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
    "std::unique_ptr<server::ServerWorker> m_worker",
    "std::make_unique<game::debug::LocalDebugSessionControl>()",
    "std::make_unique<server::ServerWorker>(",
    "*m_debugControl",
    "m_worker->advance(",
    "m_worker->fixedStepSeconds()",
    "return *m_debugControl;",
):
    if required not in host_h and required not in host_cpp:
        fail(f"LocalGameHost no longer composes through ServerWorker: {required}")

for forbidden in (
    "std::unique_ptr<server::ServerRuntime> m_runtime",
    "std::make_unique<server::ServerRuntime>(",
    "m_runtime->",
):
    if forbidden in host_h or forbidden in host_cpp:
        fail(f"LocalGameHost regained direct ServerRuntime ownership/access: {forbidden}")

# Exactly the server-side runtime owns GameServer during local production play.
for required in (
    "class ServerRuntime final",
    "std::unique_ptr<GameServer> m_server",
    "m_server->world() = worldParams",
    "game::debug::IServerDebugChannel& debugChannel",
    "transport.receiveSessionHello(hello)",
    "attachPlayerSessionTransport(transport, hello)",
    "transport.publishSnapshotImmediately(bootstrapSnapshot)",
    "m_debugChannel.publishSnapshot(m_server->snapshot())",
):
    if required not in runtime_h and required not in runtime_cpp:
        fail(f"ServerRuntime ownership/bootstrap contract is incomplete: {required}")

if not re.search(r"std::make_unique<GameServer>\s*\(\s*bootstrapPlayerSlotCount\s*\)", runtime_cpp):
    fail("ServerRuntime must remain the sole GameServer owner while passing explicit bootstrap-slot capacity")

if "public game::debug::IDebugSessionControl" in runtime_h:
    fail("ServerRuntime regained the application-side debug facade")

if "return *m_worker;" in host_cpp:
    fail("LocalGameHost exposes ServerWorker itself as debug control")

for required in (
    "ServerRuntime runtime(worldParams, transport, debugChannel)",
    "runtime.advance(elapsedSeconds)",
):
    if required not in worker_cpp:
        fail(f"ServerWorker no longer owns ServerRuntime execution: {required}")

# The client presents only an opaque bearer token; server-assigned gameplay
# identity is returned separately in SessionWelcome and never selected by the
# client command stream.
for required in (
    "struct SessionHello",
    "AuthToken authToken {};",
    "struct SessionWelcome",
    "ServerSessionId sessionId {};",
    "PlayerId playerId {};",
    "ShipInstanceId controlledShipInstanceId = 0;",
    "EntityId controlledEntityId {0};",
    "CatalogMetadata starAtlasCatalog;",
):
    if required not in session_message_h:
        fail(f"one-time session identity contract is incomplete: {required}")

if "controlledEntityId" in client_session_snapshot_h:
    fail("stable controlled-entity identity leaked into recurring simulation snapshots")

for required in (
    "resolveOrBindAccount",
    "m_accounts.resolve",
    "m_accounts.bind",
    "m_server->createPlayerSession(playerId)",
    "welcome.sessionId = sessionId",
    "welcome.playerId = playerId",
    "welcome.controlledShipInstanceId = controlledShipInstanceId",
    "welcome.controlledEntityId = controlledEntityId",
    "m_server->copyHydratedSnapshotForSession(",
    "welcome.starAtlasCatalog.schemaVersion",
    "welcome.starAtlasCatalog.contentFingerprint",
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
    "welcome.sessionId",
    "m_serverSessionId = welcome.sessionId",
    "m_catalogs.validateServerStarAtlas(",
    "welcome.starAtlasCatalog",
    "m_playerIdentityId = welcome.playerId",
    "m_controlledShipInstanceId = welcome.controlledShipInstanceId",
    "m_playerId = welcome.controlledEntityId",
    "m_hasPlayerIdentity = true",
):
    if required not in client_cpp:
        fail(f"GameClient no longer derives identity from server welcome: {required}")

if "m_host->playerId()" in session_cpp:
    fail("LocalGameSession still asks the authoritative host for player identity")

if "return m_client->playerId();" not in session_cpp:
    fail("LocalGameSession player identity is not sourced from synchronized client state")

if "sendSessionHello(" not in client_transport_h or "receiveSessionWelcome(" not in client_transport_h:
    fail("client transport lost the account hello / session welcome control plane")

if "receiveSessionHello(" not in server_transport_h or "publishSessionWelcomeImmediately(" not in server_transport_h:
    fail("server transport lost the account hello / session welcome control plane")

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
    "transport.receiveClientMessage(clientMessage)",
    "binding.sessionId",
    "m_server.receiveClientMessage(",
):
    if required not in runner_cpp:
        fail(f"server-side local-session identity binding is incomplete: {required}")

print("[PASS] server runtime ownership + account-authenticated server-assigned client identity")
