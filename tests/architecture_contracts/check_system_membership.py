#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")


def read(path: Path) -> str:
    if not path.is_file():
        fail(path, "required system-membership contract file is missing")
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def function_body(source: str, signature: str):
    start = source.find(signature)
    if start < 0:
        return None
    brace = source.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for index in range(brace, len(source)):
        ch = source[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return None


def require_text(path: Path, source: str, required, label: str) -> None:
    for token in required:
        if token not in source:
            fail(path, f"{label}: missing {token}")


motion_h = SRC / "game/navigation/DynamicMotionState.h"
hub_frame_h = SRC / "game/navigation/HubNavigationFrame.h"
reference_h = SRC / "game/navigation/ReferenceFrame.h"
ship_frame_h = SRC / "game/simulation/ShipReferenceFrameSnapshot.h"
diagnostic_h = SRC / "game/simulation/UniverseDiagnosticTrajectorySession.h"
static_h = SRC / "world/objects/StaticObject.h"
object_snapshot_h = SRC / "game/simulation/ObjectSnapshot.h"
client_world_h = SRC / "game/client/ClientWorldState.h"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
simulation_h = SRC / "game/simulation/GameSimulation.h"
simulation_cpp = SRC / "game/simulation/GameSimulation.cpp"
server_h = SRC / "game/server/GameServer.h"
server_cpp = SRC / "game/server/GameServer.cpp"
scene_cpp = SRC / "game/scene/GameSceneSetup.cpp"
promo_cpp = SRC / "game/promo/PromoFlybyScenario.cpp"
spatial_resolver_h = SRC / "game/navigation/PlayerSpatialDomainResolver.h"
spatial_resolver_cpp = SRC / "game/navigation/PlayerSpatialDomainResolver.cpp"
navigation_config_h = SRC / "game/navigation/GalaxyNavigationConfig.h"

motion_text = read(motion_h)
if not re.search(r"\bint\s+systemId\s*=\s*-1\s*;", motion_text):
    fail(motion_h, "DynamicMotionState has no first-class systemId authority")

hub_frame_text = read(hub_frame_h)
if not re.search(r"\bint\s+systemId\s*=\s*-1\s*;", hub_frame_text):
    fail(hub_frame_h, "HubNavigationFrame has no system membership")

reference_text = read(reference_h)
if len(re.findall(r"\bint\s+systemId\s*=\s*-1\s*;", reference_text)) < 2:
    fail(reference_h, "ReferenceFrame and ResolvedFrameState must both carry systemId")

ship_frame_text = read(ship_frame_h)
if not re.search(r"\bint\s+systemId\s*=\s*-1\s*;", ship_frame_text):
    fail(ship_frame_h, "ship reference-frame snapshot lost systemId")

diagnostic_text = read(diagnostic_h)
state_start = diagnostic_text.find("struct UniverseDiagnosticTrajectoryState")
if state_start < 0 or "int systemId = -1;" not in diagnostic_text[state_start:state_start + 1200]:
    fail(diagnostic_h, "diagnostic trajectory state does not preserve ship system membership")

static_text = read(static_h)
require_text(
    static_h,
    static_text,
    ("int systemId = -1;", "bool systemMapVisible = false;"),
    "static-object spatial/presentation split is incomplete",
)
if "mapSystemId" in static_text:
    fail(static_h, "mapSystemId still conflates spatial membership with map visibility")

object_snapshot_text = read(object_snapshot_h)
if "int systemId = -1;" not in object_snapshot_text:
    fail(object_snapshot_h, "ObjectSnapshot does not carry static-object membership")

client_world_h_text = read(client_world_h)
if "int                                             systemId = -1;" not in client_world_h_text and \
   not re.search(r"\bint\s+systemId\s*=\s*-1\s*;", client_world_h_text):
    fail(client_world_h, "ClientObjectState does not retain ObjectSnapshot systemId")

client_world_text = read(client_world_cpp)
require_text(
    client_world_cpp,
    client_world_text,
    (
        "a.systemId == b.systemId",
        "transform.motion.systemId = frame.systemId",
        "state.systemId = o.systemId",
    ),
    "client membership propagation is incomplete",
)

simulation_h_text = read(simulation_h)
spawn_decl = re.search(
    r"EntityId\s+spawnShip\s*\(\s*ShipRole\s+role\s*,\s*int\s+systemId\s*,",
    simulation_h_text,
    re.S,
)
if not spawn_decl:
    fail(simulation_h, "spawnShip does not require explicit systemId")

station_decl = re.search(
    r"EntityId\s+spawnStation\s*\(\s*ObjectType\s+type\s*,\s*int\s+systemId\s*,",
    simulation_h_text,
    re.S,
)
if not station_decl:
    fail(simulation_h, "spawnStation does not require explicit systemId")

simulation_text = read(simulation_cpp)
spawn = function_body(simulation_text, "EntityId GameSimulation::spawnShip(")
if spawn is None:
    fail(simulation_cpp, "could not locate spawnShip")
else:
    require_text(
        simulation_cpp,
        spawn,
        ("int systemId", "motion.systemId = systemId"),
        "spawnShip membership contract is incomplete",
    )

spawn_station = function_body(simulation_text, "EntityId GameSimulation::spawnStation(")
if spawn_station is None:
    fail(simulation_cpp, "could not locate spawnStation")
else:
    require_text(
        simulation_cpp,
        spawn_station,
        ("int systemId", "obj.systemId = systemId"),
        "spawnStation membership contract is incomplete",
    )

rebuild_frames = function_body(simulation_text, "void GameSimulation::rebuildHubNavigationFrames(")
if rebuild_frames is None or "frame.systemId = hub.systemId" not in rebuild_frames:
    fail(simulation_cpp, "HubNavigationFrame does not inherit OrbitalHubRuntime systemId")

resolve_frame = function_body(simulation_text, "game::navigation::ResolvedFrameState GameSimulation::resolveReferenceFrame(")
if resolve_frame is None:
    fail(simulation_cpp, "could not locate resolveReferenceFrame")
else:
    require_text(
        simulation_cpp,
        resolve_frame,
        (
            "frame.systemId >= 0",
            "frame.systemId != hubFrame->systemId",
            "result.systemId = hubFrame->systemId",
        ),
        "reference-frame membership validation is incomplete",
    )

attach_static = function_body(simulation_text, "bool GameSimulation::attachStaticObjectToHub(")
if attach_static is None:
    fail(simulation_cpp, "could not locate attachStaticObjectToHub")
else:
    require_text(
        simulation_cpp,
        attach_static,
        (
            "hubSystemId < 0",
            "obj.systemId != hubSystemId",
        ),
        "hub-attached static object does not validate pre-existing system membership",
    )

begin_diag = function_body(simulation_text, "bool GameSimulation::beginUniverseTrajectoryDiagnostic(")
apply_diag = function_body(simulation_text, "bool GameSimulation::applyDiagnosticTrajectoryTransform(")
if begin_diag is None:
    fail(simulation_cpp, "could not locate diagnostic branch entry for membership check")
else:
    for required in (
        "motion.systemId < 0",
        "sourceHubFrame->systemId != motion.systemId",
        "state.systemId = motion.systemId",
    ):
        if required not in begin_diag:
            fail(simulation_cpp, f"diagnostic branch membership fence is incomplete: {required}")
if apply_diag is None:
    fail(simulation_cpp, "could not locate diagnostic presentation transform")
else:
    for required in (
        "motion.systemId = state->systemId",
        "frame->systemId == state->systemId",
    ):
        if required not in apply_diag:
            fail(simulation_cpp, f"diagnostic presentation membership fence is incomplete: {required}")

update_sim = function_body(simulation_text, "void GameSimulation::update(")
if update_sim is None:
    fail(simulation_cpp, "could not locate GameSimulation::update")
else:
    promo_pos = update_sim.find("m_promoFlybyScenario.update(*this, fdt);")
    if promo_pos >= 0:
        guard_pos = update_sim.rfind("if (!trajectoryDebugMode)", 0, promo_pos)
        if guard_pos < 0 or promo_pos - guard_pos > 300:
            fail(simulation_cpp, "PromoFlybyScenario can mutate production state during diagnostic branch")

scene_text = read(scene_cpp)
promo_text = read(promo_cpp)
# Active spawn sites must make membership explicit. Commented historical code is ignored.
for path, source in ((scene_cpp, scene_text), (promo_cpp, promo_text)):
    uncommented = "\n".join(
        line for line in source.splitlines()
        if not line.lstrip().startswith("//")
    )
    for match in re.finditer(r"spawnShip\s*\(\s*ShipRole::(?:Player|NPC)\s*,", uncommented, re.S):
        tail = uncommented[match.end():match.end() + 160]
        system_arg = re.match(r"\s*([^,]+)\s*,", tail)
        if not system_arg or not system_arg.group(1).strip():
            fail(path, "active spawnShip call does not pass an explicit system id")
            break

scene_uncommented = "\n".join(
    line for line in scene_text.splitlines()
    if not line.lstrip().startswith("//")
)
for match in re.finditer(r"spawnStation\s*\(\s*ObjectType::[A-Za-z0-9_]+\s*,", scene_uncommented, re.S):
    tail = scene_uncommented[match.end():match.end() + 160]
    system_arg = re.match(r"\s*([^,]+)\s*,", tail)
    if not system_arg or not system_arg.group(1).strip():
        fail(scene_cpp, "literal spawnStation call does not pass an explicit system id")
        break
# Data-driven hub modules pass their authored hub.systemId instead of a magic zero.
if not re.search(
    r"sim\.spawnStation\s*\(\s*objectType\s*,\s*hub\.systemId\s*,",
    scene_text,
    re.S,
):
    fail(scene_cpp, "hub-module spawn does not inherit the authored hub system id")

server_text = read(server_cpp)
server_h_text = read(server_h)
if "void synchronizePlayerSystemMembership();" not in server_h_text:
    fail(server_h, "server has no boundary that derives navigation system from player entity authority")

sync_player = function_body(server_text, "void GameServer::synchronizePlayerSystemMembership()")
if sync_player is None:
    fail(server_cpp, "could not locate synchronizePlayerSystemMembership")
else:
    require_text(
        server_cpp,
        sync_player,
        (
            "player->core().transform().motion.systemId",
            "m_playerNavigation.currentSystemId = shipSystemId",
        ),
        "player-navigation membership derivation is incomplete",
    )

server_update = function_body(server_text, "void GameServer::update(double dt)")
if server_update is None or "synchronizePlayerSystemMembership();" not in server_update:
    fail(server_cpp, "active celestial context is not synchronized from player membership each tick")

resolver_h_text = read(spatial_resolver_h)
resolver_cpp_text = read(spatial_resolver_cpp)
navigation_config_text = read(navigation_config_h)

require_text(
    spatial_resolver_h,
    resolver_h_text,
    (
        "currentSystemId = -1",
        "galacticPositionLy",
        "systemMembershipRadiusAu",
    ),
    "interstellar navigation-domain contract is incomplete",
)

require_text(
    spatial_resolver_cpp,
    resolver_cpp_text,
    (
        "sourceSystem->positionLy",
        "world::coordinates::toGalacticLy",
        "nearestDistanceLy <= membershipRadiusLy",
        "result.currentSystemId = -1",
        "result.currentSystemId = nearestSystem->id",
    ),
    "interstellar navigation-domain resolver is incomplete",
)

if "systemMembershipRadiusAu" not in navigation_config_text:
    fail(navigation_config_h, "system-membership radius is not data-driven")

if server_update is not None:
    require_text(
        server_cpp,
        server_update,
        (
            "resolvePlayerSpatialDomain(",
            "m_systemMembershipRadiusAu",
            "m_playerNavigation.currentSystemId =",
            "spatialDomain.currentSystemId",
            "m_playerNavigation.worldPosition =",
            "spatialDomain.worldPosition",
        ),
        "server does not publish resolved interstellar navigation state",
    )

system_snapshot = function_body(server_text, "GameServer::buildSystemMapSnapshot(")
if system_snapshot is None:
    fail(server_cpp, "could not locate buildSystemMapSnapshot")
else:
    for forbidden in (
        "m_simulation.staticObjects()",
        "m_simulation.orbitalHubs()",
    ):
        if forbidden in system_snapshot:
            fail(server_cpp, f"System-map infrastructure returned to server composition: {forbidden}")

client_infrastructure_sampler = ROOT / "src/game/client/ClientSystemMapInfrastructureSampler.h"
client_infrastructure_sampler_text = client_infrastructure_sampler.read_text(encoding="utf-8")
require_text(
    client_infrastructure_sampler,
    client_infrastructure_sampler_text,
    (
        "object.systemId != requestedSystemId",
        "hub.systemId != requestedSystemId",
        "olderObject.systemId",
        "newerObject->systemId",
        "canInterpolateSystemLocalState(",
        "olderHub.systemId",
        "newerHub->systemId",
    ),
    "client System-map infrastructure sampling can mix system-local coordinate domains",
)

client_ship_sampler = ROOT / "src/game/client/ClientSystemMapShipSampler.h"
client_ship_sampler_text = client_ship_sampler.read_text(encoding="utf-8")
require_text(
    client_ship_sampler,
    client_ship_sampler_text,
    (
        "olderSystemId",
        "newerSystemId",
        "canInterpolateSystemLocalState(",
        "olderSystemId != requestedSystemId",
    ),
    "client System-map ship sampling can mix system-local coordinate domains",
)

client_detail_sampler = ROOT / "src/game/client/ClientDetailMapRuntimeSampler.h"
client_detail_sampler_text = client_detail_sampler.read_text(encoding="utf-8")
require_text(
    client_detail_sampler,
    client_detail_sampler_text,
    (
        "ship.transform.motion.systemId != requestedSystemId",
        "object.systemId != requestedSystemId",
        "hub.systemId != requestedSystemId",
        "oldShip.transform.motion.systemId",
        "newShip->transform.motion.systemId",
        "oldObject.systemId",
        "newObject->systemId",
        "oldHub.systemId",
        "newHub->systemId",
        "canInterpolateSystemLocalState(",
    ),
    "client Detail sampling can mix system-local coordinate domains",
)

build_hub = function_body(server_text, "GameServer::buildHubMapSnapshot(")
if build_hub is None:
    fail(server_cpp, "could not locate buildHubMapSnapshot")
else:
    require_text(
        server_cpp,
        build_hub,
        (
            "hub.systemId != systemId",
            "frame->systemId != systemId",
        ),
        "Hub map request can bind a hub/reference frame from another system",
    )

refresh_hub = function_body(server_text, "void GameServer::refreshHubMapDynamicState(")
if refresh_hub is None:
    fail(server_cpp, "could not locate refreshHubMapDynamicState")
else:
    for required in (
        "object.systemId != snapshot.systemId",
        "transform.motion.systemId != snapshot.systemId",
    ):
        if required not in refresh_hub:
            fail(server_cpp, f"Hub map dynamic refresh can cross system membership: {required}")

if errors:
    print("System-membership architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("System-membership architecture check passed.")
