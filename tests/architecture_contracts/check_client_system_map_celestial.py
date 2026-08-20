#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def fail(message: str) -> None:
    print("[FAIL] System-map deterministic celestial layer is client-owned and uses accepted snapshot epoch: " + message)
    raise SystemExit(1)

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

protocol = read("src/game/network/MapSnapshotMessage.h")
service_h = read("src/game/client/ClientMapService.h")
service = read("src/game/client/ClientMapService.cpp")
client = read("src/game/client/GameClient.cpp")
server = read("src/game/server/GameServer.cpp")
for token in ("SystemMapRequest", "SystemMapResponse", "DetailMapRequest", "DetailMapResponse", "HubMapRequest", "HubMapResponse"):
    if token in protocol:
        fail(f"obsolete map RPC DTO survived: {{token}}")
if "std::variant<GalaxyMapRequest>" not in protocol or "std::variant<GalaxyMapResponse>" not in protocol:
    fail("map transport is not Galaxy-only")
if "GameServer::buildSystemMapSnapshot(" in server:
    fail("server System-map builder survived local-composition migration")
for token in ("composeSystem(", "sourceMetadata.serverTimeSeconds", "sourceMetadata.universeTimeSeconds", "rebuildSystemMapCelestialLayer("):
    if token not in service_h + service:
        fail(f"local System composition seam missing: {token}")
if "m_lastSimulationMetadata" not in client or "m_maps.composeSystem(" not in client:
    fail("GameClient does not compose System map from last accepted SimulationSnapshot metadata")

print("[PASS] System-map deterministic celestial layer is client-owned and uses accepted snapshot epoch")
