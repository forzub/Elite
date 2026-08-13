#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] client System-map infrastructure migration: {message}")
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
simulation_cpp = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
simulation_h = (ROOT / "src/game/simulation/SimulationSnapshot.h").read_text(encoding="utf-8")
object_h = (ROOT / "src/game/simulation/ObjectSnapshot.h").read_text(encoding="utf-8")
hub_h = (ROOT / "src/game/simulation/OrbitalHubSnapshot.h").read_text(encoding="utf-8")
world_h = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
sampler_h = (ROOT / "src/game/client/ClientSystemMapInfrastructureSampler.h").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/game/client/ClientSystemMapInfrastructureBridge.h").read_text(encoding="utf-8")
map_service_cpp = (ROOT / "src/game/client/ClientMapService.cpp").read_text(encoding="utf-8")
protocol_h = (ROOT / "src/game/network/MapSnapshotMessage.h").read_text(encoding="utf-8")

server_system = function_body(server_cpp, "GameServer::buildSystemMapSnapshot(")
if server_system is None:
    fail("could not locate GameServer::buildSystemMapSnapshot")

for forbidden in (
    "m_simulation.staticObjects()",
    "m_simulation.orbitalHubs()",
    "SystemMapObjectKind::Station",
    "SystemMapObjectKind::Hub",
    "mapHub.owner",
    "mapObj.owner",
):
    if forbidden in server_system:
        fail(f"server still composes production System-map infrastructure: {forbidden}")

if "diagnostic:hub_motion_lab_cube" not in server_system:
    fail("explicit analytic diagnostic probe was accidentally removed")

for required in (
    "navigationVisible",
    "navigationParentBodyId",
    "displayName",
    "ownerName",
    "OrbitalMotion orbitalMotion",
):
    if required not in object_h:
        fail(f"ObjectSnapshot lost authoritative infrastructure fact: {required}")

for required in (
    "struct OrbitalHubSnapshot",
    "std::string id",
    "std::string owner",
    "int systemId",
    "WorldPosition worldPosition",
    "OrbitalMotion motion",
):
    if required not in hub_h:
        fail(f"hub replication DTO incomplete: {required}")

if "std::vector<game::simulation::OrbitalHubSnapshot> hubs" not in simulation_h:
    fail("SimulationSnapshot does not carry ordinary replicated hub state")

for required in (
    "o.navigationVisible = obj.systemMapVisible",
    "o.navigationParentBodyId = obj.mapParentBodyId",
    "o.orbitalMotion = obj.orbitalMotion",
    "snapshot.hubs.reserve(m_orbitalHubs.size())",
    "h.worldPosition = hub.worldPosition",
    "h.motion = hub.motion",
):
    if required not in simulation_cpp:
        fail(f"replication builder does not publish infrastructure fact: {required}")

for required in (
    "sampleSystemMapInfrastructureAtServerTime(",
    "resolveSnapshotPresentationWindow(",
    "relativeMeters(",
    "translated(",
    "SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot",
    "SystemMapInfrastructureSampleStatus::TooOld",
):
    if required not in sampler_h:
        fail(f"exact-epoch infrastructure sampler incomplete: {required}")

for required in (
    "rebuildSystemMapInfrastructureLayer(",
    "SystemMapObjectKind::Station",
    "SystemMapObjectKind::Hub",
    '"entity:" + std::to_string(objectState.id.value)',
    "applySystemMapOrbit(object, hubIt->second->motion)",
    "fullMeters(hubState.worldPosition)",
):
    if required not in bridge_h:
        fail(f"client infrastructure presentation bridge incomplete: {required}")

for required in (
    "sampleSystemMapInfrastructureAtServerTime(",
    "m_snapshotBuffer",
    "const std::unordered_map<std::string, ClientHubState>& hubs()",
):
    if required not in world_h:
        fail(f"ClientWorldState lost ordinary infrastructure state/history seam: {required}")

for required in (
    "m_world.sampleSystemMapInfrastructureAtServerTime(",
    "SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot",
    "SystemMapInfrastructureSampleStatus::TooOld",
    "rebuildSystemMapInfrastructureLayer(",
):
    if required not in map_service_cpp:
        fail(f"ClientMapService does not join replicated infrastructure at map epoch: {required}")

for required in (
    "Stage 3E protocol seam",
    "production hubs/static",
    "ordinary SimulationSnapshot history",
):
    if required not in protocol_h:
        fail(f"Stage 3E protocol rationale missing: {required}")

print("[PASS] System-map infrastructure/hubs are client-composed from exact-epoch replication")
