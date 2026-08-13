#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] client Details migration: {message}")
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
object_h = (ROOT / "src/game/simulation/ObjectSnapshot.h").read_text(encoding="utf-8")
hub_h = (ROOT / "src/game/simulation/OrbitalHubSnapshot.h").read_text(encoding="utf-8")
world_h = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
sampler_h = (ROOT / "src/game/client/ClientDetailMapRuntimeSampler.h").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/game/client/ClientDetailMapBridge.h").read_text(encoding="utf-8")
map_service_cpp = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")

for forbidden in (
    "buildDetailMapSnapshot",
    "buildCelestialBodyDetailSnapshot",
    "buildLocalObjectDetailSnapshot",
    "appendLocalDetailObjects",
    "refreshDetailMapDynamicState",
):
    if forbidden in server_h or forbidden in server_cpp:
        fail(f"server still owns Details presentation builder: {forbidden}")

response_start = protocol_h.find("struct DetailMapResponse")
response_end = protocol_h.find("struct HubMapResponse", response_start)
if response_start < 0 or response_end < 0:
    fail("could not locate DetailMapResponse protocol DTO")
detail_response = protocol_h[response_start:response_end]

for required in (
    "SnapshotMetadata metadata",
    "DetailTarget target",
    "Stage 3F protocol seam",
    "ordinary SimulationSnapshot",
):
    if required not in detail_response:
        fail(f"Detail response lost authoritative epoch/target seam: {required}")

if "DetailMapSnapshot" in detail_response or "snapshot;" in detail_response:
    fail("Detail response again carries a server-built presentation snapshot")

request_pump = function_body(server_cpp, "void GameServer::processPendingMapRequests()")
if request_pump is None:
    fail("could not locate server map request pump")
detail_branch_start = request_pump.find(
    "std::is_same_v<RequestT, game::network::DetailMapRequest>"
)
detail_branch_end = request_pump.find(
    "std::is_same_v<RequestT, game::network::HubMapRequest>",
    detail_branch_start,
)
if detail_branch_start < 0 or detail_branch_end < 0:
    fail("could not isolate server Detail request branch")
detail_branch = request_pump[detail_branch_start:detail_branch_end]
for required in (
    "response.metadata = metadata",
    "response.target = typedRequest.target",
):
    if required not in detail_branch:
        fail(f"server Detail acknowledgement lost: {required}")
for forbidden in (
    "m_simulation.",
    "m_starAtlas",
    "celestialSnapshotForSystem",
    "response.snapshot",
):
    if forbidden in detail_branch:
        fail(f"Detail request branch still composes world presentation: {forbidden}")

for required in (
    "glm::dvec3 linearVelocityMps",
):
    if required not in object_h:
        fail(f"static-object replication lacks Details kinematic fact: {required}")
for required in (
    "glm::dvec3 worldVelocityMps",
):
    if required not in hub_h:
        fail(f"hub replication lacks Details kinematic fact: {required}")
for required in (
    "o.linearVelocityMps = glm::dvec3(obj.linearVelocity)",
    "h.worldVelocityMps = hubVelocityIt->second",
):
    if required not in simulation_cpp:
        fail(f"replication builder does not publish Details kinematics: {required}")

for required in (
    "sampleDetailMapRuntimeAtServerTime(",
    "resolveSnapshotPresentationWindow(",
    "DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot",
    "DetailMapRuntimeSampleStatus::TooOld",
    "canInterpolateSystemLocalState(",
    "object.systemId != requestedSystemId",
    "hub.systemId != requestedSystemId",
    "ship.transform.motion.systemId != requestedSystemId",
    "linearVelocityMps",
    "worldVelocityMps",
    "Do not filter on navigationVisible here",
):
    if required not in sampler_h:
        fail(f"exact-epoch Details runtime sampler incomplete: {required}")

if "if (!object.navigationVisible" in sampler_h:
    fail("Details sampler incorrectly inherited System-map marker filtering")

for required in (
    "rebuildDetailMapFromClientState(",
    "buildClientCelestialBodyDetail(",
    "DetailSceneKind::CelestialBody",
    "DetailSceneKind::LocalObject",
    "DetailSceneKind::SpatialVolume",
    "toSystemMapRingVisualProfile",
    "toSystemMapRing",
    "runtime.hubs",
    "runtime.objects",
    "runtime.ships",
    "out.playerOrbits.clear()",
    "evaluateHubMotionLabCube(serverTimeSeconds)",
):
    if required not in bridge_h:
        fail(f"client Details composition bridge incomplete: {required}")

for required in (
    "sampleDetailMapRuntimeAtServerTime(",
    "m_snapshotBuffer",
):
    if required not in world_h:
        fail(f"ClientWorldState lost Details replication-history seam: {required}")

for required in (
    "m_world.sampleDetailMapRuntimeAtServerTime(",
    "response.metadata.serverTimeSeconds",
    "response.metadata.universeTimeSeconds",
    "m_catalogs.resolveCelestialSystem(",
    "rebuildDetailMapFromClientState(",
    "DetailResponseResult::AwaitingSimulationHistory",
    "DetailResponseResult::RetryFreshResponse",
    "m_deferredDetailResponse",
):
    if required not in map_service_cpp:
        fail(f"ClientMapService does not compose Details at one authoritative epoch: {required}")

print("[PASS] Details is client-composed from local celestial state + exact-epoch replication")
