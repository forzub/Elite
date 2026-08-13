#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors: list[str] = []


def fail(path: Path, message: str) -> None:
    try:
        shown = path.relative_to(ROOT)
    except ValueError:
        shown = path
    errors.append(f"{shown}: {message}")


for path in (
    SRC / "game/client/ClientCatalogService.h",
    SRC / "game/client/ClientCatalogService.cpp",
    SRC / "game/client/GameClient.cpp",
    SRC / "game/server/GameServer.cpp",
):
    if not path.is_file():
        fail(path, "required source file is missing")
        continue

    text = path.read_text(encoding="utf-8", errors="replace")

    for forbidden in (
        "CelestialSnapshotRequest",
        "CelestialSnapshotResponse",
        "sendCelestialRequest",
        "m_celestialRefreshAccumulator",
    ):
        if forbidden in text:
            fail(path, f"obsolete celestial streaming path returned: {forbidden}")

registry_header = SRC / "world/celestial/CelestialRuntimeRegistry.h"
registry_cpp = SRC / "world/celestial/CelestialRuntimeRegistry.cpp"

for path in (registry_header, registry_cpp):
    if not path.is_file():
        fail(path, "shared demand-driven celestial resolver is missing")

if registry_header.is_file():
    text = registry_header.read_text(encoding="utf-8", errors="replace")

    for required in (
        "const CelestialSystemSnapshot* resolve(",
        "double universeTimeSeconds",
        "cachedSystemCount()",
        "const StarAtlasDatabase* m_atlas",
    ):
        if required not in text:
            fail(registry_header, f"resolver contract is incomplete: {required}")

    if "void update(double universeTimeSeconds)" in text:
        fail(registry_header, "registry must not update every cached system per tick")

catalog_cpp = SRC / "game/client/ClientCatalogService.cpp"
if catalog_cpp.is_file():
    text = catalog_cpp.read_text(encoding="utf-8", errors="replace")

    for required in (
        "m_celestialRuntimes.initialize(m_starAtlas)",
        "m_celestialRuntimes.resolve(",
    ):
        if required not in text:
            fail(catalog_cpp, f"client local celestial reconstruction is missing: {required}")

    for required in (
        "m_starAtlas.loadFromRuntimeOrSource()",
        "validateServerStarAtlas(",
        "contentFingerprint",
    ):
        if required not in text:
            fail(catalog_cpp, f"client static-catalog ownership is incomplete: {required}")

    for forbidden in (
        "ITransport",
        "StarAtlasRequest",
        "StarAtlasResponse",
        "sendPresentationDataRequest",
        "receivePresentationDataResponse",
    ):
        if forbidden in text:
            fail(catalog_cpp, f"client static catalog returned to transport ownership: {forbidden}")

client_cpp = SRC / "game/client/GameClient.cpp"
if client_cpp.is_file():
    text = client_cpp.read_text(encoding="utf-8", errors="replace")

    for required in (
        "m_serverClock.advance(",
        "m_serverClock.addSyncSample(",
        "m_universeTimeline.synchronize(",
        "renderUniverseTimeSeconds()",
        "m_catalogs.resolveCelestialSnapshot(",
    ):
        if required not in text:
            fail(client_cpp, f"client universe-time pipeline is incomplete: {required}")

server_cpp = SRC / "game/server/GameServer.cpp"
if server_cpp.is_file():
    text = server_cpp.read_text(encoding="utf-8", errors="replace")

    if "m_celestialRuntimes.update(" in text:
        fail(server_cpp, "server still evaluates every celestial runtime each tick")

    if "m_celestialRuntimes.resolve(" not in text:
        fail(server_cpp, "server does not use the shared demand-driven resolver")



# Stage 2: render-time and rotating-frame contracts.
map_resources_header = SRC / "game/system_map/MapCelestialRenderResources.h"
map_resources_cpp = SRC / "game/system_map/MapCelestialRenderResources.cpp"
client_bridge = SRC / "game/client/ClientCelestialMapBridge.h"
space_state_cpp = SRC / "game/SpaceState.cpp"
hub_frame_header = SRC / "game/navigation/HubNavigationFrame.h"
dynamic_motion_cpp = SRC / "game/navigation/DynamicMotionSystem.cpp"
ship_frame_header = SRC / "game/simulation/ShipReferenceFrameSnapshot.h"

for path in (
    map_resources_header,
    map_resources_cpp,
    client_bridge,
    space_state_cpp,
    hub_frame_header,
    dynamic_motion_cpp,
    ship_frame_header,
):
    if not path.is_file():
        fail(path, "required Stage-2 coordinate/time contract file is missing")

if map_resources_header.is_file():
    text = map_resources_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "m_visualEffectTimeSeconds",
        "m_visualEffectLastSourceTimeSeconds",
        "m_visualEffectLastWallClockSeconds",
        "m_visualEffectTimeInitialized",
    ):
        if forbidden in text:
            fail(map_resources_header, f"independent render clock returned: {forbidden}")

if map_resources_cpp.is_file():
    text = map_resources_cpp.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "steady_clock",
        "sourceWentBackward",
        "correctionBlend",
        "m_visualEffectTimeSeconds",
    ):
        if forbidden in text:
            fail(map_resources_cpp, f"render layer still owns/corrects universe time: {forbidden}")

if client_bridge.is_file():
    text = client_bridge.read_text(encoding="utf-8", errors="replace")
    for required in (
        "applyClientCelestialPresentation(",
        "detail.planetRotationPhaseRad = body->rotationPhaseRad",
        "hub.parentPlanetRotationPhaseRad = body->rotationPhaseRad",
        "detail.universeTimeSeconds = celestial.simTimeSeconds",
        "hub.universeTimeSeconds = celestial.simTimeSeconds",
    ):
        if required not in text:
            fail(client_bridge, f"predictable client map bridge is incomplete: {required}")

if space_state_cpp.is_file():
    text = space_state_cpp.read_text(encoding="utf-8", errors="replace")
    if "applyClientCelestialPresentation(" not in text:
        fail(space_state_cpp, "local maps do not consume client-reconstructed celestial presentation")

if hub_frame_header.is_file():
    text = hub_frame_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "angularVelocityWorldRadPerSecond",
        "worldToLocalVector(",
        "localToWorldVector(",
        "worldToLocalVelocity(",
        "worldPositionMeters",
        "localToWorldVelocity(",
        "localPositionMeters",
        "glm::cross(",
    ):
        if required not in text:
            fail(hub_frame_header, f"rotating-frame velocity contract is incomplete: {required}")

if ship_frame_header.is_file():
    text = ship_frame_header.read_text(encoding="utf-8", errors="replace")
    if "angularVelocityWorldRadPerSecond" not in text:
        fail(ship_frame_header, "ship reference-frame snapshot lost angular velocity")

if dynamic_motion_cpp.is_file():
    text = dynamic_motion_cpp.read_text(encoding="utf-8", errors="replace")
    if "motion.gravityAccelerationMps2" in text:
        fail(
            dynamic_motion_cpp,
            "HubTactical again integrates absolute gravity in an orbiting local frame",
        )
    for required in (
        "frame.worldToLocalVector(",
        "frame.localToWorldVelocity(",
        "motion.localPositionMeters",
    ):
        if required not in text:
            fail(dynamic_motion_cpp, f"HubTactical coordinate contract is incomplete: {required}")

hub_map_bridge = SRC / "game/client/ClientHubMapBridge.h"
if hub_map_bridge.is_file():
    text = hub_map_bridge.read_text(encoding="utf-8", errors="replace")
    for required in (
        "source.motionMode == game::navigation::MotionMode::HubTactical",
        "ship.positionMeters = source.localPositionMeters",
        "ship.velocityMps = source.localVelocityMps",
    ):
        if required not in text:
            fail(hub_map_bridge, f"Hub Map lost authoritative HubTactical local state: {required}")



# Stage 3: one global server-time estimate, one universe timeline and one
# buffered presentation timeline. Frame dt may advance the presentation
# playhead, but snapshot history constrains it; frame dt may never independently
# integrate authoritative world/universe state.
server_clock_header = SRC / "game/client/ClientServerClock.h"
presentation_clock_header = SRC / "game/client/ClientPresentationClock.h"
universe_timeline_header = SRC / "game/client/ClientUniverseTimeline.h"
old_client_clock_header = SRC / "game/client/ClientUniverseClock.h"
time_sync_header = SRC / "game/network/TimeSyncMessage.h"
transport_header = SRC / "game/network/ITransport.h"
loopback_header = SRC / "game/network/LocalLoopbackTransport.h"
loopback_cpp = SRC / "game/network/LocalLoopbackTransport.cpp"
universe_clock_header = SRC / "world/time/UniverseClock.h"
client_world_header = SRC / "game/client/ClientWorldState.h"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
session_header = SRC / "game/simulation/ClientSessionSnapshot.h"
local_session_cpp = SRC / "game/host/LocalGameSession.cpp"
server_timeline_clock_header = SRC / "game/server/ServerTimelineClock.h"
server_time_context_header = SRC / "game/server/ServerTimeContext.h"
simulation_header = SRC / "game/simulation/GameSimulation.h"
simulation_cpp = SRC / "game/simulation/GameSimulation.cpp"

for path in (
    server_clock_header,
    presentation_clock_header,
    universe_timeline_header,
    time_sync_header,
    transport_header,
    loopback_header,
    loopback_cpp,
    universe_clock_header,
    client_world_header,
    client_world_cpp,
    session_header,
    local_session_cpp,
    server_timeline_clock_header,
    server_time_context_header,
    simulation_header,
    simulation_cpp,
):
    if not path.is_file():
        fail(path, "required Stage-3 time-contract file is missing")

if old_client_clock_header.exists():
    fail(old_client_clock_header, "obsolete frame-integrated client universe clock returned")

if server_clock_header.is_file():
    text = server_clock_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class ClientServerClock",
        "addSyncSample(",
        "modelRate()",
        "effectiveRate()",
        "phaseCorrectionWindowSeconds",
        "maxPhaseCorrectionRate",
        "rttSlackSeconds",
    ):
        if required not in text:
            fail(server_clock_header, f"server-clock estimator contract is incomplete: {required}")

    for forbidden in (
        "maxPhaseCorrectionRate = 0.35",
        "1.35",
        "0.65",
    ):
        if forbidden in text:
            fail(server_clock_header, f"large old clock-rate correction returned: {forbidden}")

if presentation_clock_header.is_file():
    text = presentation_clock_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class ClientPresentationClock",
        "minimumSnapshotLeadSeconds",
        "recoverySnapshotLeadSeconds",
        "hardRebaseThresholdSeconds",
        "hardRebaseCount()",
        "starvationCount()",
        "newestSnapshotServerTimeSeconds",
    ):
        if required not in text:
            fail(presentation_clock_header, f"presentation-clock contract is incomplete: {required}")

if universe_timeline_header.is_file():
    text = universe_timeline_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class ClientUniverseTimeline",
        "timeAtServerTime(",
        "std::uint64_t revision",
        "m_anchorServerTimeSeconds",
        "m_anchorUniverseTimeSeconds",
    ):
        if required not in text:
            fail(universe_timeline_header, f"universe timeline contract is incomplete: {required}")

    if "advance(" in text:
        fail(universe_timeline_header, "universe timeline must not integrate frame delta")

if time_sync_header.is_file():
    text = time_sync_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "struct TimeSyncRequest",
        "clientSendTimeSeconds",
        "struct TimeSyncResponse",
        "serverReceiveTimeSeconds",
    ):
        if required not in text:
            fail(time_sync_header, f"time-sync protocol is incomplete: {required}")

if transport_header.is_file():
    text = transport_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "sendTimeSyncRequest(",
        "receiveTimeSyncResponse(",
    ):
        if required not in text:
            fail(transport_header, f"transport does not expose clock sync: {required}")

if loopback_cpp.is_file():
    text = loopback_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_timeSyncRequestBuffer",
        "m_timeSyncResponseBuffer",
        "m_serverTimeSyncRequests",
        "sendTimeSyncRequest(",
        "receiveTimeSyncRequest(",
        "sendTimeSyncResponse(",
        "receiveTimeSyncResponse(",
    ):
        if required not in text:
            fail(loopback_cpp, f"loopback time-sync path is incomplete: {required}")

    for forbidden in (
        "GameServer",
        "m_server.",
        "serverTimeSeconds()",
    ):
        if forbidden in text:
            fail(loopback_cpp, f"loopback transport regained server ownership: {forbidden}")

if universe_clock_header.is_file():
    text = universe_clock_header.read_text(encoding="utf-8", errors="replace")
    if "duration_cast<seconds>" in text:
        fail(universe_clock_header, "server universe clock is quantized to whole seconds")
    if "duration<double>(now).count()" not in text:
        fail(universe_clock_header, "server universe anchor is not high-resolution")
    if "m_timeSeconds +=" not in text or "safeDt * timeScale()" not in text:
        fail(universe_clock_header, "universe timeline is not advanced from authoritative server dt")

if session_header.is_file():
    text = session_header.read_text(encoding="utf-8", errors="replace")
    if "universeTimelineRevision" not in text:
        fail(session_header, "session snapshot does not version universe timeline changes")

if client_cpp.is_file():
    text = client_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "estimatedServerTimeSeconds()",
        "renderServerTimeSeconds()",
        "renderUniverseTimeSeconds()",
        "RenderInterpolationDelaySeconds",
        "m_presentationClock.update(",
        "m_presentationClock.renderTimeSeconds()",
        "m_transport.sendTimeSyncRequest(request)",
        "m_transport.receiveTimeSyncResponse(response)",
    ):
        if required not in text:
            fail(client_cpp, f"global client time pipeline is incomplete: {required}")

    for forbidden in (
        "m_universeClock",
        "m_clientTime",
        "m_renderDelay",
    ):
        if forbidden in text:
            fail(client_cpp, f"obsolete independent client time source returned: {forbidden}")

if client_world_header.is_file():
    text = client_world_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in ("m_clientTime", "m_renderDelay"):
        if forbidden in text:
            fail(client_world_header, f"ClientWorldState owns a second render clock: {forbidden}")
    if "double renderServerTimeSeconds" not in text:
        fail(client_world_header, "ClientWorldState does not consume global render server time")

if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")

    # ClientWorldState must consume the one presentation playhead selected by
    # GameClient. Stage 2e deliberately moved snapshot-pair selection and alpha
    # calculation behind SnapshotPresentationWindow, so requiring the obsolete
    # direct assignment ``double renderTime = renderServerTimeSeconds`` would
    # reject the stronger single-window contract.
    required_presentation_window_tokens = (
        "resolveSnapshotPresentationWindow(",
        "m_snapshotBuffer,",
        "renderServerTimeSeconds,",
        "presentationWindow.renderTimeSeconds",
        "presentationWindow.interpolationAlpha",
    )
    for required in required_presentation_window_tokens:
        if required not in text:
            fail(
                client_world_cpp,
                f"snapshot interpolation is not derived from the global presentation playhead: {required}",
            )

    # Do not allow a second local render clock or a second hand-written alpha
    # path to creep back into ClientWorldState. Sampling mechanics belong to
    # SnapshotPresentationWindow.
    for forbidden in (
        "double renderTime = renderServerTimeSeconds",
        "(renderTime - olderTime) /",
        "(renderServerTimeSeconds - olderTime) /",
    ):
        if forbidden in text:
            fail(
                client_world_cpp,
                f"obsolete independent snapshot presentation path returned: {forbidden}",
            )

if space_state_cpp.is_file():
    text = space_state_cpp.read_text(encoding="utf-8", errors="replace")
    if "static_cast<double>(std::max(0.0f, dt))" not in text:
        fail(space_state_cpp, "raw wall delta is not separated from clamped prediction delta")

if server_cpp.is_file():
    text = server_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_universeTimelineRevision",
        "snapshot.session.universeTimelineRevision",
        "time.serverDeltaSeconds = std::max(0.0, dt)",
        "time.gameplayDeltaSeconds =",
        "time.universeTimeSimulation",
    ):
        if required not in text:
            fail(server_cpp, f"server universe timeline revision is incomplete: {required}")

if server_timeline_clock_header.is_file():
    text = server_timeline_clock_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class ServerTimelineClock",
        "void advance(double serverDeltaSeconds)",
        "m_timeSeconds += serverDeltaSeconds",
    ):
        if required not in text:
            fail(server_timeline_clock_header, f"server timeline clock contract is incomplete: {required}")

if server_time_context_header.is_file():
    text = server_time_context_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "double serverDeltaSeconds",
        "double gameplayDeltaSeconds",
        "double universeDeltaSeconds",
    ):
        if required not in text:
            fail(server_time_context_header, f"server time domains are not explicit: {required}")

if simulation_header.is_file():
    text = simulation_header.read_text(encoding="utf-8", errors="replace")
    if "ServerTimelineClock" not in text or "m_serverTimelineClock" not in text:
        fail(simulation_header, "GameSimulation lost the dedicated monotonic server clock")
    if re.search(r"\bm_serverTime\b", text):
        fail(simulation_header, "raw server-time accumulator returned to GameSimulation")

cloud_resources_cpp = SRC / "game/system_map/MapCelestialRenderResources.cpp"
if cloud_resources_cpp.is_file():
    text = cloud_resources_cpp.read_text(encoding="utf-8", errors="replace")
    start = text.find("double cloudWindTimeScale()")
    end = text.find("std::string normalizeCloudToken", start)
    if start < 0 or end < 0:
        fail(cloud_resources_cpp, "could not locate cloud wind time-scale contract")
    else:
        cloud_scale = text[start:end]
        if '"default_debug"' in cloud_scale:
            fail(
                cloud_resources_cpp,
                "production cloud wind still uses the debug time multiplier",
            )
        if '"physical"' not in cloud_scale:
            fail(
                cloud_resources_cpp,
                "production cloud wind does not select the physical time scale",
            )

if simulation_cpp.is_file():
    text = simulation_cpp.read_text(encoding="utf-8", errors="replace")
    if "m_serverTimelineClock.advance(time.serverDeltaSeconds);" not in text:
        fail(
            simulation_cpp,
            "server timeline is not advanced from serverDeltaSeconds independently of gameplay freeze",
        )
    for forbidden in (
        "m_serverTime += dt",
        "m_serverTime += time.gameplayDeltaSeconds",
    ):
        if forbidden in text:
            fail(simulation_cpp, f"server clock can freeze with gameplay again: {forbidden}")

    diagnostic_match = re.search(
        r"bool GameSimulation::beginUniverseTrajectoryDiagnostic\([\s\S]*?\n}\r?\n\r?\nvoid GameSimulation::endUniverseTrajectoryDiagnostic",
        text,
    )
    if not diagnostic_match:
        fail(simulation_cpp, "could not locate transactional universe diagnostic entry contract")
    else:
        diagnostic_entry = diagnostic_match.group(0)
        for required in (
            "seedFromLocalTravelFrame",
            "motion.travelFrame.valid",
            "motion.travelFrame.systemId == motion.systemId",
            "motion.travelFrame.localToWorldPosition(",
            "motion.travelFrame.localToWorldVelocity(",
            "motion.localPositionMeters",
            "motion.localVelocityMps",
            "m_universeDiagnosticTrajectories.add(",
            "seededShipCount == eligibleShipCount",
            "m_universeDiagnosticTrajectories.discard()",
        ):
            if required not in diagnostic_entry:
                fail(
                    simulation_cpp,
                    f"transactional diagnostic entry is incomplete: {required}",
                )

        for forbidden in (
            "motion.mode = game::navigation::MotionMode::PassiveTrajectory",
            "tr.setWorldPositionMeters(",
            "motion.worldVelocityMps =",
            "sourceHubFrame->localToWorldPosition(",
            "sourceHubFrame->localToWorldVelocity(",
        ):
            if forbidden in diagnostic_entry:
                fail(
                    simulation_cpp,
                    f"diagnostic entry mutates production ship state: {forbidden}",
                )

    exit_match = re.search(
        r"void GameSimulation::endUniverseTrajectoryDiagnostic\(\)[\s\S]*?\n}\r?\n\r?\nvoid GameSimulation::advanceUniverseTrajectoryDiagnostic",
        text,
    )
    if not exit_match:
        fail(simulation_cpp, "could not locate transactional universe diagnostic exit contract")
    else:
        diagnostic_exit = exit_match.group(0)
        if "m_universeDiagnosticTrajectories.discard()" not in diagnostic_exit:
            fail(simulation_cpp, "diagnostic exit does not discard the alternate branch")
        for forbidden in (
            "setWorldPositionMeters(",
            "worldVelocityMps =",
            "localPositionMeters =",
            "localVelocityMps =",
        ):
            if forbidden in diagnostic_exit:
                fail(simulation_cpp, f"diagnostic exit commits future state: {forbidden}")


if errors:
    print("World runtime architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("World runtime architecture check passed.")
