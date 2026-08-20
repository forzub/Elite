#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def fail(message: str) -> None:
    print("[FAIL] System-map ships are composed from ordinary replication at accepted snapshot epoch: " + message)
    raise SystemExit(1)

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

protocol = read("src/game/network/MapSnapshotMessage.h")
service = read("src/game/client/ClientMapService.cpp")
world = read("src/game/client/ClientWorldState.h")
bridge = read("src/game/client/ClientSystemMapShipBridge.h")
for token in ("SystemMapRequest", "SystemMapResponse", "DetailMapRequest", "DetailMapResponse", "HubMapRequest", "HubMapResponse"):
    if token in protocol:
        fail(f"obsolete map RPC DTO survived: {{token}}")
if "std::variant<GalaxyMapRequest>" not in protocol or "std::variant<GalaxyMapResponse>" not in protocol:
    fail("map transport is not Galaxy-only")
for token in ("sampleSystemMapShipsAtServerTime(", "sourceMetadata.serverTimeSeconds", "rebuildSystemMapShipLayer("):
    if token not in service:
        fail(f"System ship composition missing: {token}")
if "sampleSystemMapShipsAtServerTime(" not in world:
    fail("ClientWorldState lost retained System-map ship history sampler")
for token in ("SystemMapObjectKind::Ship", "localControlledEntityId", "fullMeters(ship.worldPosition)"):
    if token not in bridge:
        fail(f"ship bridge semantics missing: {token}")
for token in ("m_deferredSystemResponse", "retrySystemRequestOrFail", "simulationHasReached"):
    if token in service:
        fail(f"obsolete System response waiting survived: {token}")

print("[PASS] System-map ships are composed from ordinary replication at accepted snapshot epoch")
