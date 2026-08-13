#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"Server-transport architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


client_transport_h = read("src/game/network/ITransport.h")
server_transport_h = read("src/game/network/IServerTransport.h")
loopback_h = read("src/game/network/LocalLoopbackTransport.h")
loopback_cpp = read("src/game/network/LocalLoopbackTransport.cpp")
runner_h = read("src/game/server/ServerRunner.h")
runner_cpp = read("src/game/server/ServerRunner.cpp")
host_cpp = read("src/game/host/LocalGameHost.cpp")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
worker_cpp = read("src/game/server/ServerWorker.cpp")

# Client-facing transport must not own the server loop/pump anymore.
if "virtual void update(float dt)" in client_transport_h:
    fail("client ITransport still exposes the server-side transport pump")

for required in (
    "class IServerTransport",
    "publishSessionWelcomeImmediately(",
    "receiveClientMessage(",
    "receiveMapRequest(",
    "receiveTimeSyncRequest(",
    "publishSnapshot(",
    "publishSnapshotImmediately(",
    "sendMapResponse(",
    "sendTimeSyncResponse(",
):
    if required not in server_transport_h:
        fail(f"server transport endpoint is incomplete: {required}")

# Static StarAtlas data must never reopen a generic presentation payload
# channel. Map requests are authoritative/dynamic queries; local catalogs are
# loaded independently and compatibility-fenced during session bootstrap.
for text, label in (
    (client_transport_h, "ITransport.h"),
    (server_transport_h, "IServerTransport.h"),
    (loopback_h, "LocalLoopbackTransport.h"),
    (loopback_cpp, "LocalLoopbackTransport.cpp"),
    (runner_cpp, "ServerRunner.cpp"),
):
    for forbidden in (
        "PresentationData",
        "StarAtlasRequest",
        "StarAtlasResponse",
        "presentationRequests",
        "presentationResponses",
    ):
        if forbidden in text:
            fail(f"{label} regained static presentation-data transport: {forbidden}")

for text, label in (
    (loopback_h, "LocalLoopbackTransport.h"),
    (loopback_cpp, "LocalLoopbackTransport.cpp"),
):
    for forbidden in (
        "GameServer&",
        "GameServer *",
        "GameServer*",
        "m_server;",
        "m_server.",
        "src/game/server/GameServer.h",
    ):
        if forbidden in text:
            fail(f"{label} directly owns/calls GameServer again: {forbidden}")

for required in (
    "public ITransport, public IServerTransport",
    "receiveSessionWelcome(",
    "publishSessionWelcomeImmediately(",
    "m_sessionWelcome",
    "m_clientMessages",
    "m_mapRequests",
    "m_serverTimeSyncRequests",
):
    if required not in loopback_h:
        fail(f"loopback message seam is incomplete: {required}")

for required in (
    "IServerTransport& transport",
    "struct ServerTransportBinding",
    "IServerTransport* transport = nullptr",
    "game::network::ServerSessionId sessionId",
    "std::vector<ServerTransportBinding> m_transports",
    "receiveInboundMessages(ServerTransportBinding& binding)",
    "publishOutboundMessages()",
):
    if required not in runner_h:
        fail(f"ServerRunner does not own pure server session endpoints: {required}")

for forbidden in (
    "ITransport& transport",
    "ITransport& m_transport",
    "src/game/network/ITransport.h",
):
    if forbidden in runner_h or forbidden in runner_cpp:
        fail(f"ServerRunner still depends on the client transport interface: {forbidden}")

for required in (
    "for (auto& binding : m_transports)",
    "transport.receiveClientMessage(clientMessage)",
    "binding.sessionId",
    "m_server.receiveClientMessage(",
    "transport.receiveMapRequest(mapRequest)",
    "m_server.enqueueMapRequest(",
    "transport.receiveTimeSyncRequest(timeSyncRequest)",
    "m_server.serverTimeSeconds()",
    "transport.sendTimeSyncResponse(",
    "m_server.copySnapshotForSession(",
    "m_server.popMapResponse(responseSessionId, mapResponse)",
):
    if required not in runner_cpp:
        fail(f"ServerRunner multi-endpoint message exchange is incomplete: {required}")

if "std::make_unique<LocalLoopbackTransport>(*m_server)" in host_cpp:
    fail("LocalGameHost still injects GameServer into the loopback transport")

if "std::make_unique<LocalLoopbackTransport>()" not in host_cpp:
    fail("LocalGameHost no longer creates the local transport link")

for required in (
    "std::make_unique<server::ServerWorker>(",
    "transport.publishSnapshotImmediately(bootstrapSnapshot)",
):
    if required not in host_cpp and required not in runtime_cpp and required not in worker_cpp:
        fail(f"local-server bootstrap no longer uses the endpoint seam: {required}")

print("[PASS] server/client transport ownership boundary")
