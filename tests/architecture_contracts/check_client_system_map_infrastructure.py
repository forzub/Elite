#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def fail(message: str) -> None:
    print("[FAIL] System-map infrastructure is composed from ordinary replication at accepted snapshot epoch: " + message)
    raise SystemExit(1)

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

protocol = read("src/game/network/MapSnapshotMessage.h")
service = read("src/game/client/ClientMapService.cpp")
world = read("src/game/client/ClientWorldState.h")
bridge = read("src/game/client/ClientSystemMapInfrastructureBridge.h")
server = read("src/game/server/GameServer.cpp")
for token in ("SystemMapRequest", "SystemMapResponse", "DetailMapRequest", "DetailMapResponse", "HubMapRequest", "HubMapResponse"):
    if token in protocol:
        fail(f"obsolete map RPC DTO survived: {{token}}")
if "std::variant<GalaxyMapRequest>" not in protocol or "std::variant<GalaxyMapResponse>" not in protocol:
    fail("map transport is not Galaxy-only")
if "GameServer::buildSystemMapSnapshot(" in server:
    fail("server System-map production builder survived")
for token in ("sampleSystemMapInfrastructureAtServerTime(", "sourceMetadata.serverTimeSeconds", "rebuildSystemMapInfrastructureLayer("):
    if token not in service:
        fail(f"System infrastructure composition missing: {token}")
if "sampleSystemMapInfrastructureAtServerTime(" not in world:
    fail("ClientWorldState lost retained infrastructure sampler")
if "SystemMapObjectKind::Hub" not in bridge and "SystemMapObjectKind::Infrastructure" not in bridge:
    fail("infrastructure bridge no longer maps authoritative rows into System objects")

print("[PASS] System-map infrastructure is composed from ordinary replication at accepted snapshot epoch")
