#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

errors = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")


def text(path: Path) -> str:
    if not path.is_file():
        fail(path, "required architecture-contract file is missing")
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


protocol = SRC / "game/network/ProtocolMetadata.h"
server_cpp = SRC / "game/server/GameServer.cpp"
server_h = SRC / "game/server/GameServer.h"
simulation_cpp = SRC / "game/simulation/GameSimulation.cpp"
simulation_h = SRC / "game/simulation/GameSimulation.h"
dynamic_motion = SRC / "game/navigation/DynamicMotionState.h"
diagnostic_session = SRC / "game/simulation/UniverseDiagnosticTrajectorySession.h"
client_cpp = SRC / "game/client/GameClient.cpp"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
client_map_cpp = SRC / "game/client/ClientMapService.cpp"
transition_cpp = SRC / "game/client/MapTransitionController.cpp"
interpolator_h = SRC / "game/system_map/AuthoritativeMapInterpolator.h"
space_cpp = SRC / "game/SpaceState.cpp"
space_h = SRC / "game/SpaceState.h"
cloud_policy = SRC / "render/celestial/CloudMotionPolicy.h"
cloud_resources = SRC / "game/system_map/MapCelestialRenderResources.cpp"
status_doc = SRC / "game/ARCHITECTURE_STATUS.md"
debug_control_html = SRC / "assets/webui/debug_control.html"
main_cpp = SRC / "main.cpp"

protocol_text = text(protocol)
if "std::uint64_t universeTimelineRevision" not in protocol_text:
    fail(protocol, "snapshot metadata lost universe-timeline revision fence")

server_text = text(server_cpp)
server_h_text = text(server_h)
if re.search(
    r"(?m)^\s*world::time::UniverseClock&\s+universeClock\(\)",
    server_h_text,
):
    fail(server_h, "mutable UniverseClock accessor bypasses timeline revision/session bookkeeping")
for required in (
    "metadata.universeTimelineRevision = m_universeTimelineRevision",
    "snapshot.session.universeTimelineRevision =",
    "++m_universeTimelineRevision",
    "beginUniverseTrajectoryDiagnostic(",
):
    if required not in server_text and required not in server_h_text:
        fail(server_cpp, f"server timeline/diagnostic contract is incomplete: {required}")

failure_entry = function_body(server_text, "void GameServer::update(double dt)")
if failure_entry is None:
    fail(server_cpp, "could not locate GameServer::update")
else:
    if "setDebugUniverseTimeSimulation(" not in failure_entry:
        fail(server_cpp, "failed diagnostic entry can rewind without publishing a revision")
    if "ship.setControlState(cmd);" not in failure_entry:
        fail(server_cpp, "normal control-state application is missing")
    if "if (!time.universeTimeSimulation)" not in failure_entry:
        fail(server_cpp, "diagnostic branch no longer protects production controls")
    if "ship.setControlState(neutralControl)" in failure_entry:
        fail(server_cpp, "diagnostic branch overwrites production control state")

motion_text = text(dynamic_motion)
for forbidden in (
    "resumeModeAfterPassiveTrajectory",
    "passiveTrajectoryParentBodyId",
    "passiveTrajectoryRelativePositionMeters",
    "passiveTrajectoryRelativeVelocityMps",
    "passiveTrajectoryEpochUniverseTimeSeconds",
):
    if forbidden in motion_text:
        fail(dynamic_motion, f"debug-only trajectory state leaked into production motion: {forbidden}")

session_text = text(diagnostic_session)
for required in (
    "class UniverseDiagnosticTrajectorySession",
    "void begin(",
    "void discard()",
    "m_states.clear()",
):
    if required not in session_text:
        fail(diagnostic_session, f"transactional diagnostic session is incomplete: {required}")

simulation_text = text(simulation_cpp)
begin_diag = function_body(
    simulation_text,
    "bool GameSimulation::beginUniverseTrajectoryDiagnostic(")
end_diag = function_body(
    simulation_text,
    "void GameSimulation::endUniverseTrajectoryDiagnostic()")
update_sim = function_body(
    simulation_text,
    "void GameSimulation::update(")

if begin_diag is None:
    fail(simulation_cpp, "could not locate diagnostic branch entry")
else:
    for required in (
        "motion.travelFrame.localToWorldPosition(",
        "motion.travelFrame.localToWorldVelocity(",
        "seedFromLocalTravelFrame",
        "m_universeDiagnosticTrajectories.add(",
        "GravityFieldSystem::sample(",
        "seededShipCount == eligibleShipCount",
        "m_universeDiagnosticTrajectories.discard()",
    ):
        if required not in begin_diag:
            fail(simulation_cpp, f"diagnostic entry is not all-or-nothing/canonical: {required}")
    for forbidden in (
        "motion.mode = game::navigation::MotionMode::PassiveTrajectory",
        "tr.setWorldPositionMeters(",
        "motion.worldVelocityMps =",
    ):
        if forbidden in begin_diag:
            fail(simulation_cpp, f"diagnostic entry mutates production ship state: {forbidden}")

    # Every ship emitted in an accelerated snapshot must belong to the same
    # universe-timeline revision. Diagnostic motion probes are visible ships,
    # so exempting them would recreate the frozen/accelerated mixed epoch.
    if "isHubMotionLabShip(" in begin_diag:
        fail(simulation_cpp, "diagnostic branch can leave Hub Motion Lab ships on a frozen mixed epoch")

if end_diag is None:
    fail(simulation_cpp, "could not locate diagnostic branch exit")
else:
    if "m_universeDiagnosticTrajectories.discard()" not in end_diag:
        fail(simulation_cpp, "diagnostic exit does not discard alternate branch")
    for forbidden in (
        "setWorldPositionMeters(",
        "worldVelocityMps =",
        "localPositionMeters =",
        "localVelocityMps =",
    ):
        if forbidden in end_diag:
            fail(simulation_cpp, f"diagnostic exit commits future state: {forbidden}")

if update_sim is None:
    fail(simulation_cpp, "could not locate GameSimulation::update")
else:
    for required in (
        "if (trajectoryDebugMode)",
        "advanceUniverseTrajectoryDiagnostic(",
        "if (!trajectoryDebugMode)",
        "endUniverseTrajectoryDiagnostic()",
        "obj.linearVelocity =",
        "computeOrbitVelocityMetersPerSecond(",
        "angularVelocityWorldRadPerSecond",
    ):
        if required not in update_sim:
            fail(simulation_cpp, f"diagnostic/derived-kinematic lifecycle is incomplete: {required}")

client_text = text(client_cpp)
for required in (
    "snapshot.metadata.universeTimelineRevision !=",
    "snapshot.session.universeTimelineRevision",
    "m_maps.setUniverseTimelineRevision(",
    "void GameClient::prepareGameplayFrame(",
):
    if required not in client_text:
        fail(client_cpp, f"client revision/frame-boundary contract is incomplete: {required}")

client_world_text = text(client_world_cpp)
for required in (
    "timelineRevisionChanged",
    "m_snapshotBuffer.clear()",
    "m_snapshotTimelineRevision = incomingTimelineRevision",
):
    if required not in client_world_text:
        fail(client_world_cpp, f"world snapshot revision fence is incomplete: {required}")

client_map_text = text(client_map_cpp)
for required in (
    "setUniverseTimelineRevision(",
    "acceptsTimeline(",
    "resetPendingRequests()",
    "metadata.universeTimelineRevision",
):
    if required not in client_map_text:
        fail(client_map_cpp, f"map cache revision fence is incomplete: {required}")

transition_text = text(transition_cpp)
if "mapMetadata.universeTimelineRevision !=" not in transition_text:
    fail(transition_cpp, "map transition can bridge different timeline revisions")

interpolator_text = text(interpolator_h)
for required in (
    "std::uint64_t universeTimelineRevision",
    "TimedDetailSnapshot",
    "TimedHubSnapshot",
):
    if required not in interpolator_text:
        fail(interpolator_h, f"local-map interpolator revision fence is incomplete: {required}")

interpolator_cpp = SRC / "game/system_map/AuthoritativeMapInterpolator.cpp"
interpolator_cpp_text = text(interpolator_cpp)
for required in (
    "m_detailHistory.back().universeTimelineRevision",
    "m_hubHistory.back().universeTimelineRevision",
    "m_detailHistory.clear()",
    "m_hubHistory.clear()",
):
    if required not in interpolator_cpp_text:
        fail(interpolator_cpp, f"local-map timeline fence is incomplete: {required}")

space_text = text(space_cpp)
prepare = function_body(space_text, "void SpaceState::prepareFrame(float dt)")
if prepare is None:
    fail(space_cpp, "could not locate SpaceState::prepareFrame")
else:
    sync_pos = prepare.find("prepareGameplayFrame(")
    map_pos = prepare.find("updateLiveMapSnapshots(")
    presentation_pos = prepare.find("updateLocalMapPresentationSnapshots(")
    if min(sync_pos, map_pos, presentation_pos) < 0 or not (sync_pos < map_pos < presentation_pos):
        fail(space_cpp, "authoritative synchronization must precede map preparation and input")
    if "m_authoritativeMapInterpolator = {};" not in prepare:
        fail(space_cpp, "SpaceState does not clear local-map interpolation at revision fence")

load_defaults = function_body(space_text, "void SpaceState::loadDebugControlDefaults()")
save_defaults = function_body(space_text, "bool SpaceState::saveDebugControlDefaults(")
for body, name in ((load_defaults, "load"), (save_defaults, "save")):
    if body is None:
        fail(space_cpp, f"could not locate Debug Control defaults {name}")
    else:
        for forbidden_key in (
            'erase("debugUniverseTimeSimulation")',
            'erase("debugFastUniverseTime")',
        ):
            if forbidden_key not in body:
                fail(space_cpp, f"Debug Control defaults {name} can persist active diagnostic state")

# Fast-universe is a user-visible debug control, so protect the complete
# command path rather than only testing UniverseClock in isolation.
debug_control_text = text(debug_control_html)
for required in (
    'id="debugUniverseTimeSimulation"',
    'id="debugUniverseTimeScaleInput"',
    'debugUniverseTimeSimulation: document.getElementById',
    'debugUniverseTimeScale: numberValue',
):
    if required not in debug_control_text:
        fail(debug_control_html, f"fast-universe UI control path is incomplete: {required}")

apply_debug = function_body(space_text, "void SpaceState::applyDebugControlPayload(")
if apply_debug is None:
    fail(space_cpp, "could not locate Debug Control payload application")
else:
    for required in (
        '"debugUniverseTimeSimulation"',
        '"debugUniverseTimeScale"',
        "m_debugSession->setUniverseTimeSimulation(",
    ):
        if required not in apply_debug:
            fail(space_cpp, f"fast-universe Debug Control command is not wired: {required}")

main_text = text(main_cpp)
for required in (
    "--self-test-fast-universe",
    "runFastUniverseSmokeTest()",
    "std::make_unique<GameServer>()",
    "server->setDebugUniverseTimeSimulation(true, TestScale);",
    "server->update(StepSeconds);",
    "server->debugUniverseTimeSimulation()",
):
    if required not in main_text:
        fail(main_cpp, f"real-scene fast-universe smoke contract missing: {required}")

if "GameServer server;" in main_text:
    fail(
        main_cpp,
        "real-scene fast-universe smoke must heap-allocate GameServer like LocalGameHost",
    )

space_h_text = text(space_h)
for forbidden in (
    "GameSimulation",
    "SimulationSnapshot",
):
    if forbidden in space_h_text:
        fail(space_h, f"SpaceState regained server/simulation ownership: {forbidden}")

cloud_policy_text = text(cloud_policy)
for required in (
    "input.baseHeightKm * 1000.0",
    "authoredVisualUvPerSecond",
    "debugSpeedMultiplier",
    "morphologySpeedMultiplier",
):
    if required not in cloud_policy_text:
        fail(cloud_policy, f"cloud motion behavior contract is incomplete: {required}")

cloud_resources_text = text(cloud_resources)
if "resolveCloudMotionPolicy(" not in cloud_resources_text:
    fail(cloud_resources, "cloud renderer bypasses the tested motion policy")

if not status_doc.is_file():
    fail(status_doc, "architecture/migration status document is missing")

if errors:
    print("Architecture contract check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Architecture contract check passed.")
