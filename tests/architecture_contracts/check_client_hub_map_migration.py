#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] client Hub Map migration: {message}")
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

server_h = (ROOT / "src/game/server/GameServer.h").read_text(encoding="utf-8")
server_cpp = (ROOT / "src/game/server/GameServer.cpp").read_text(encoding="utf-8")
protocol_h = (ROOT / "src/game/network/MapSnapshotMessage.h").read_text(encoding="utf-8")
simulation_cpp = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
hub_snapshot_h = (ROOT / "src/game/simulation/OrbitalHubSnapshot.h").read_text(encoding="utf-8")
sampler_h = (ROOT / "src/game/client/ClientDetailMapRuntimeSampler.h").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/game/client/ClientHubMapBridge.h").read_text(encoding="utf-8")
world_h = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
map_service_h = (ROOT / "src/game/client/ClientMapService.h").read_text(encoding="utf-8")
map_service_cpp = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")

for forbidden in (
    "buildHubMapSnapshot",
    "refreshHubMapDynamicState",
    "axesToHubLocal",
    "assemblySizeMetersForType",
):
    if forbidden in server_h or forbidden in server_cpp:
        fail(f"server still owns Hub Map presentation builder: {forbidden}")

response_start = protocol_h.find("struct HubMapResponse")
response_end = protocol_h.find("using MapResponse", response_start)
if response_start < 0 or response_end < 0:
    fail("could not locate HubMapResponse protocol DTO")
hub_response = protocol_h[response_start:response_end]

for required in (
    "SnapshotMetadata metadata",
    "int systemId",
    "std::string hubId",
    "Stage 3G protocol seam",
    "ordinary",
    "SimulationSnapshot",
):
    if required not in hub_response:
        fail(f"Hub response lost authoritative epoch/identity seam: {required}")

if "HubMapSnapshot" in hub_response or "snapshot;" in hub_response:
    fail("Hub response again carries a server-built presentation snapshot")

request_pump = function_body(server_cpp, "void GameServer::processPendingMapRequests()")
if request_pump is None:
    fail("could not locate server map request pump")
hub_branch_start = request_pump.find(
    "std::is_same_v<RequestT, game::network::HubMapRequest>"
)
if hub_branch_start < 0:
    fail("could not isolate server Hub request branch")
hub_branch = request_pump[hub_branch_start:]
for required in (
    "response.metadata = metadata",
    "response.systemId = typedRequest.systemId",
    "response.hubId = typedRequest.hubId",
):
    if required not in hub_branch:
        fail(f"server Hub acknowledgement lost: {required}")
for forbidden in (
    "m_simulation.",
    "m_starAtlas",
    "response.snapshot",
):
    if forbidden in hub_branch:
        fail(f"Hub request branch still composes world presentation: {forbidden}")

for required in (
    "angularVelocityWorldRadPerSecond",
    "primeModuleId",
):
    if required not in hub_snapshot_h:
        fail(f"ordinary hub replication lacks Hub Map frame/topology fact: {required}")

for required in (
    "h.angularVelocityWorldRadPerSecond",
    "frame->angularVelocityWorldRadPerSecond",
    "h.primeModuleId = frame->primeModuleId",
):
    if required not in simulation_cpp:
        fail(f"replication builder does not publish Hub Map frame fact: {required}")

for required in (
    "motionMode",
    "localPositionMeters",
    "localVelocityMps",
    "angularVelocityWorldRadPerSecond",
    "primeModuleId",
    "resolveSnapshotPresentationWindow(",
    "canInterpolateSystemLocalState(",
):
    if required not in sampler_h:
        fail(f"exact-epoch local-map runtime sampler incomplete for Hub Map: {required}")

for required in (
    "rebuildHubMapFromClientState(",
    "makeClientHubMapFrame(",
    "worldToLocalVelocity(",
    "X=normal, Y=radial, Z=-prograde",
    "X=prograde, Y=radial, Z=normal",
    "source.hubAttachment.hubId != hubId",
    "source.hubAttachment.localOffsetMeters",
    "hubVisualLocalToWorldPosition(",
    "hubAttachedVisualOrientation(",
    "source.motionMode == game::navigation::MotionMode::HubTactical",
    "source.localPositionMeters",
    "source.localVelocityMps",
    "source.motionMode == game::navigation::MotionMode::Docked",
    "evaluateHubMotionLabCube(serverTimeSeconds)",
    "parent->rotationPhaseRad",
    "hubMapAssemblySizeMeters",
):
    if required not in bridge_h:
        fail(f"client Hub Map composition bridge incomplete: {required}")

for required in (
    "sampleHubMapRuntimeAtServerTime(",
    "m_snapshotBuffer",
):
    if required not in world_h:
        fail(f"ClientWorldState lost Hub Map replication-history seam: {required}")

for required in (
    "HubResponseResult",
    "m_deferredHubResponse",
):
    if required not in map_service_h:
        fail(f"ClientMapService Hub exact-epoch state machine incomplete: {required}")

for required in (
    "m_world.sampleHubMapRuntimeAtServerTime(",
    "response.metadata.serverTimeSeconds",
    "response.metadata.universeTimeSeconds",
    "m_catalogs.resolveCelestialSystem(",
    "rebuildHubMapFromClientState(",
    "HubResponseResult::AwaitingSimulationHistory",
    "HubResponseResult::RetryFreshResponse",
    "m_deferredHubResponse",
):
    if required not in map_service_cpp:
        fail(f"ClientMapService does not compose Hub Map at one authoritative epoch: {required}")

print("[PASS] Hub Map is client-composed from local celestial state + exact-epoch replication")
