#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
server = (ROOT / "src/game/server/GameServer.cpp").read_text(encoding="utf-8")
client = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")
bridge = (ROOT / "src/game/client/ClientGalaxyMapBridge.h").read_text(encoding="utf-8")
protocol = (ROOT / "src/game/network/MapSnapshotMessage.h").read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] {message}", file=sys.stderr)
    raise SystemExit(1)


def function_body(text: str, marker: str) -> str:
    start = text.find(marker)
    if start < 0:
        fail(f"missing function: {marker}")
    brace = text.find("{", start)
    if brace < 0:
        fail(f"missing body for: {marker}")
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[brace:i + 1]
    fail(f"unterminated body for: {marker}")


server_builder = function_body(
    server,
    "world::celestial::GalaxyMapSnapshot GameServer::buildGalaxyMapSnapshot() const",
)

# The authoritative server may publish world-state overlays, but must not
# reconstruct static StarAtlas presentation fields for every Galaxy request.
for forbidden in (
    "m_starAtlas.systems()",
    "m_starAtlas.objects()",
    "item.name = s.name",
    "item.starType = s.starType",
    "item.positionLy = s.positionLy",
    "source.description",
    "source.tags",
):
    if forbidden in server_builder:
        fail(f"Galaxy server builder still reconstructs client-local catalog data: {forbidden}")

for required in (
    "m_systemJurisdictions",
    "overlay.id = systemId",
    "overlay.jurisdiction = jurisdiction",
    "m_universeClock.timeSeconds()",
    "m_universeClock.dateTimeString()",
):
    if required not in server_builder:
        fail(f"Galaxy server builder lost authoritative overlay/epoch contract: {required}")

for required in (
    'ClientGalaxyMapBridge.h',
    "m_catalogs.starAtlas()",
    "rebuildGalaxyMapCatalogLayer",
    "m_catalogs.hasStarAtlas()",
):
    if required not in client:
        fail(f"ClientMapService no longer owns Galaxy catalog composition: {required}")

for required in (
    "atlas.systems()",
    "atlas.objects()",
    "const std::vector<world::celestial::StarSystemSummary>& systems",
    "const std::vector<world::celestial::GalaxyObjectDefinition>& objects",
    "source.positionLy",
    "source.starType",
    "source.description",
    "source.tags",
    "jurisdictionBySystem",
    '"Unregistered"',
):
    if required not in bridge:
        fail(f"Galaxy client bridge lost catalog/overlay composition: {required}")

if "Stage 3C protocol seam" not in protocol:
    fail("Galaxy response protocol no longer documents the client-catalog/server-overlay seam")

print("[PASS] Galaxy-map static catalog is client-owned; server publishes world overlays only")
