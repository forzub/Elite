#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def fail(message: str) -> None:
    print("[FAIL] Hub map is locally composed from ordinary replication and accepted snapshot metadata: " + message)
    raise SystemExit(1)

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

protocol = read("src/game/network/MapSnapshotMessage.h")
service = read("src/game/client/ClientMapService.cpp")
client = read("src/game/client/GameClient.cpp")
world = read("src/game/client/ClientWorldState.h")
server = read("src/game/server/GameServer.cpp")
for token in ("SystemMapRequest", "SystemMapResponse", "DetailMapRequest", "DetailMapResponse", "HubMapRequest", "HubMapResponse"):
    if token in protocol:
        fail(f"obsolete map RPC DTO survived: {{token}}")
if "std::variant<GalaxyMapRequest>" not in protocol or "std::variant<GalaxyMapResponse>" not in protocol:
    fail("map transport is not Galaxy-only")
for token in ("composeHub(", "sampleHubMapRuntimeAtServerTime(", "sourceMetadata.serverTimeSeconds", "sourceMetadata.universeTimeSeconds", "rebuildHubMapFromClientState("):
    if token not in service:
        fail(f"local Hub composition missing: {token}")
if "m_maps.composeHub(systemId, hubId, m_lastSimulationMetadata)" not in client:
    fail("GameClient does not anchor Hub composition to last accepted SimulationSnapshot")
if "sampleHubMapRuntimeAtServerTime(" not in world:
    fail("ClientWorldState lost retained Hub runtime sampler")
for token in ("m_deferredHubResponse", "HubMapRequest", "HubMapResponse"):
    if token in service + server:
        fail(f"obsolete Hub RPC/wait state survived: {token}")

print("[PASS] Hub map is locally composed from ordinary replication and accepted snapshot metadata")
