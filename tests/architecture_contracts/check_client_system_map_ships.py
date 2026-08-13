#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] client System-map ship migration: {message}")
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
map_service_h = (ROOT / "src/game/client/ClientMapService.h").read_text(encoding="utf-8")
map_service_cpp = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")
world_h = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
sampler_h = (ROOT / "src/game/client/ClientSystemMapShipSampler.h").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/game/client/ClientSystemMapShipBridge.h").read_text(encoding="utf-8")
game_client_cpp = (ROOT / "src/game/client/GameClient.cpp").read_text(encoding="utf-8")
protocol_h = (ROOT / "src/game/network/MapSnapshotMessage.h").read_text(encoding="utf-8")

server_system = function_body(server_cpp, "GameServer::buildSystemMapSnapshot(")
if server_system is None:
    fail("could not locate GameServer::buildSystemMapSnapshot")

for forbidden in (
    "for (const auto& [entityId, shipPtr] : m_simulation.ships())",
    "m_simulation.presentationShipTransform(entityId)",
    "mapShip.id = entityId",
    "mapShip.hasOrbit = false",
):
    if forbidden in server_system:
        fail(f"ordinary replicated ships returned to server map composition: {forbidden}")

# Explicit analytic diagnostics may remain in the map response while migration is
# incremental, but production ships must use the ordinary replication history.
if "diagnostic:hub_motion_lab_cube" not in server_system:
    fail("explicit Hub Motion Lab analytic map probe was accidentally removed")

for required in (
    "sampleSystemMapShipsAtServerTime(",
    "AwaitingNewerSnapshot",
    "TooOld",
    "resolveSnapshotPresentationWindow(",
    "canInterpolateSystemLocalState(",
    "world::coordinates::relativeMeters(",
    "world::coordinates::translated(",
):
    if required not in sampler_h:
        fail(f"exact-epoch replicated ship sampler is incomplete: {required}")

if "renderTransform" in sampler_h:
    fail("System-map ship sampling must use authoritative snapshot history, not render/prediction state")

if "class ClientWorldState;" not in map_service_h.split("namespace game::client", 1)[0]:
    fail("ClientWorldState forward declaration must name the canonical global type")

client_namespace = map_service_h.split("namespace game::client", 1)[1]
if "class ClientWorldState;" in client_namespace:
    fail("ClientMapService shadows the canonical ClientWorldState with a nested type")

for required in (
    "const ::ClientWorldState& world",
    "const ::ClientWorldState& m_world",
    "std::optional<game::network::SystemMapResponse>",
    "tryCompleteSystemResponse(",
):
    if required not in map_service_h:
        fail(f"ClientMapService lost the replication-history join seam: {required}")

for required in (
    "response.metadata.serverTimeSeconds",
    "m_world.sampleSystemMapShipsAtServerTime(",
    "SystemResponseResult::AwaitingSimulationHistory",
    "SystemResponseResult::RetryFreshResponse",
    "retrySystemRequestOrFail()",
    "m_deferredSystemResponse",
    "m_systemRequest.elapsedSeconds >= RequestTimeoutSeconds",
    "rebuildSystemMapShipLayer(",
):
    if required not in map_service_cpp:
        fail(f"System-map response does not wait for/sample ordinary replication: {required}")

for required in (
    "sampleSystemMapShipsAtServerTime(",
    "m_snapshotBuffer",
):
    if required not in world_h:
        fail(f"ClientWorldState does not expose retained authoritative history safely: {required}")

for required in (
    "SystemMapObjectKind::Ship",
    "EntityId localControlledEntityId",
    "isLocalPlayer",
    '"player"',
    '"entity:" + std::to_string(ship.id.value)',
    "fullMeters(ship.worldPosition)",
    "object.hasOrbit = false",
):
    if required not in bridge_h:
        fail(f"client ship-to-map composition lost established semantics: {required}")

if "m_maps(transport, m_catalogs, m_world)" not in game_client_cpp:
    fail("GameClient does not inject ordinary replicated world history into ClientMapService")

for required in (
    "ordinary replicated ships",
    "response metadata.serverTimeSeconds",
):
    if required not in protocol_h:
        fail(f"Stage 3B protocol rationale is not documented: {required}")

print("[PASS] System-map real ships are client-composed from exact-epoch replication history")
