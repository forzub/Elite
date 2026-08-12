#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] client System-map celestial migration: {message}")
    sys.exit(1)


def function_body(text: str, marker: str) -> str | None:
    start = text.find(marker)
    if start < 0:
        return None
    brace = text.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for index in range(brace, len(text)):
        ch = text[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    return None


server_cpp = (ROOT / "src/game/server/GameServer.cpp").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/game/client/ClientCelestialMapBridge.h").read_text(encoding="utf-8")
map_service_h = (ROOT / "src/game/client/ClientMapService.h").read_text(encoding="utf-8")
map_service_cpp = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")
catalog_h = (ROOT / "src/game/client/ClientCatalogService.h").read_text(encoding="utf-8")
catalog_cpp = (ROOT / "src/game/client/ClientCatalogService.cpp").read_text(encoding="utf-8")
game_client_h = (ROOT / "src/game/client/GameClient.h").read_text(encoding="utf-8")
game_client_cpp = (ROOT / "src/game/client/GameClient.cpp").read_text(encoding="utf-8")
conversion_h = ROOT / "src/world/celestial/SystemMapConversion.h"

server_system = function_body(server_cpp, "GameServer::buildSystemMapSnapshot(")
if server_system is None:
    fail("could not locate GameServer::buildSystemMapSnapshot")

for forbidden in (
    "SystemMapBody item",
    "out.bodies.push_back",
    "runtimeStateById",
):
    if forbidden in server_system:
        fail(f"server still composes deterministic System-map bodies: {forbidden}")

for required in (
    "m_simulation.staticObjects()",
    "m_simulation.orbitalHubs()",
):
    if required not in server_system:
        fail(f"map-specific infrastructure layer was lost: {required}")

if "if (m_diagnostics.settings.systemMapMotionCsv)" not in server_system:
    fail("server celestial resolution for motion CSV is not explicitly diagnostic-gated")
if "celestialSnapshotForSystem(systemId)" not in server_system:
    fail("opt-in System-map motion diagnostic lost its celestial source")

for required in (
    "rebuildSystemMapCelestialLayer(",
    "CelestialSystemDefinition",
    "CelestialSystemSnapshot",
    "runtimeById",
    "celestial.simTimeSeconds != map.universeTimeSeconds",
    "item.positionAu = state.positionAu",
    "item.orbitCenterAu = parentIt->second->positionAu",
    "item.positionAu = body.staticPositionAu",
    "map.bodies = std::move(rebuiltBodies)",
    "toSystemMapRingVisualProfile",
    "toSystemMapRing",
):
    if required not in bridge_h:
        fail(f"client celestial composition contract is incomplete: {required}")

if not conversion_h.is_file():
    fail("shared celestial-to-map ring conversion helper is missing")

for required in (
    "const ClientCatalogService& catalogs",
    "const ClientCatalogService& m_catalogs",
):
    if required not in map_service_h:
        fail(f"ClientMapService lost local catalog dependency: {required}")

for required in (
    "m_catalogs.hasStarAtlas()",
    "m_catalogs.resolveCelestialSystem(",
    "rebuiltSnapshot.universeTimeSeconds",
    "rebuildSystemMapCelestialLayer(",
):
    if required not in map_service_cpp:
        fail(f"System-map response is not rebuilt locally at server epoch: {required}")

for required in (
    "resolveCelestialSystem(",
    "m_celestialRuntimes.resolve(",
):
    if required not in catalog_h + catalog_cpp:
        fail(f"catalog service cannot resolve arbitrary local system state: {required}")

catalog_member = game_client_h.find("ClientCatalogService m_catalogs")
map_member = game_client_h.find("ClientMapService m_maps")
if catalog_member < 0 or map_member < 0 or catalog_member > map_member:
    fail("GameClient must construct the catalog service before ClientMapService")

if "m_catalogs(transport)" not in game_client_cpp or \
   "m_maps(transport, m_catalogs, m_world)" not in game_client_cpp:
    fail("GameClient does not inject its local catalog/world into ClientMapService")

print("[PASS] System-map deterministic celestial layer is client-owned")
