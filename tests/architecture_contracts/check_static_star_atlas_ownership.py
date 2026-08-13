#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] static StarAtlas ownership: {message}", file=sys.stderr)
    raise SystemExit(1)


forbidden_file = ROOT / "src/game/network/PresentationDataMessage.h"
if forbidden_file.exists():
    fail("obsolete PresentationDataMessage.h still exists")

client_catalog_h = read("src/game/client/ClientCatalogService.h")
client_catalog_cpp = read("src/game/client/ClientCatalogService.cpp")
client_cpp = read("src/game/client/GameClient.cpp")
server_cpp = read("src/game/server/GameServer.cpp")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
session_h = read("src/game/network/SessionMessage.h")
transport_h = read("src/game/network/ITransport.h")
server_transport_h = read("src/game/network/IServerTransport.h")
loopback_h = read("src/game/network/LocalLoopbackTransport.h")
runner_cpp = read("src/game/server/ServerRunner.cpp")
atlas_h = read("src/world/celestial/StarAtlasDatabase.h")
atlas_cpp = read("src/world/celestial/StarAtlasDatabase.cpp")

for text, label in (
    (transport_h, "ITransport"),
    (server_transport_h, "IServerTransport"),
    (loopback_h, "LocalLoopbackTransport"),
    (runner_cpp, "ServerRunner"),
    (client_catalog_h + client_catalog_cpp, "ClientCatalogService"),
    (server_cpp, "GameServer"),
):
    for forbidden in (
        "PresentationDataRequest",
        "PresentationDataResponse",
        "StarAtlasRequest",
        "StarAtlasResponse",
        "sendPresentationDataRequest",
        "receivePresentationDataRequest",
        "sendPresentationDataResponse",
        "receivePresentationDataResponse",
    ):
        if forbidden in text:
            fail(f"{label} still transports static StarAtlas data: {forbidden}")

for required in (
    "CatalogSchemaVersion",
    "loadFromRuntimeOrSource()",
    "contentFingerprint() const",
):
    if required not in atlas_h:
        fail(f"StarAtlas local-load/fingerprint contract missing: {required}")

for required in (
    "computeCatalogFingerprint",
    '"systems_details"',
    '"distant_systems_details"',
    '"objects_details"',
):
    if required not in atlas_cpp:
        fail(f"StarAtlas deterministic fingerprint implementation missing: {required}")

for required in (
    "m_starAtlas.loadFromRuntimeOrSource()",
    "validateServerStarAtlas(",
    "serverMetadata.schemaVersion != local.schemaVersion",
    "serverMetadata.contentFingerprint != local.contentFingerprint",
):
    if required not in client_catalog_cpp:
        fail(f"client local catalog compatibility check missing: {required}")

if "ITransport&" in client_catalog_h or "ITransport&" in client_catalog_cpp:
    fail("ClientCatalogService still owns a transport dependency")

for required in (
    "CatalogMetadata starAtlasCatalog;",
):
    if required not in session_h:
        fail(f"session catalog compatibility fence missing: {required}")

for required in (
    "welcome.starAtlasCatalog.schemaVersion",
    "welcome.starAtlasCatalog.contentFingerprint",
):
    if required not in runtime_cpp:
        fail(f"server does not publish static catalog compatibility metadata: {required}")

for required in (
    "m_catalogs.loadLocalStarAtlas()",
    "m_catalogs.validateServerStarAtlas(",
):
    if required not in client_cpp:
        fail(f"GameClient bootstrap does not enforce local StarAtlas ownership: {required}")

print("[PASS] StarAtlas is endpoint-local static data with session version/fingerprint fence")
