#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        raise AssertionError(f"missing {path}")
    return p.read_text(encoding="utf-8", errors="replace")


def require(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing contract token {token!r}")


def forbid(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{path}: forbidden coupling token {token!r}")


try:
    require(
        "src/game/navigation/NavigationModuleState.h",
        "TrajectoryPrediction",
        "SafetyEvaluation",
        "RoutePlanning",
        "LocalGuidance",
        "ServerGuidance",
        "HudTargetMarkers",
        "HudRouteMarkers",
        "HudGuidanceCorridor",
        "HudGalacticCompass",
        "HudFlightVector",
        "setAllHudLayers",
    )
    require(
        "src/game/navigation/GuidanceCorridor.h",
        "GuidanceSource",
        "StationTrafficControl",
        "GuidancePurpose",
        "ObstacleBypass",
        "GuidanceFrame",
        "hasTerminalTarget",
        "terminalTargetMeters",
        "NavigationGuidanceState",
    )
    require(
        "src/game/navigation/NavigationPlanningSnapshot.h",
        "NavigationLane",
        "KnownTrafficIntent",
        "NavigationObstacle",
        "RestrictedNavigationVolume",
        "NavigationPlanningSnapshotBuilder",
        "Radar",
    )
    require(
        "src/game/navigation/TrajectorySafetyEvaluator.cpp",
        "closestLinearApproach",
        "timingUncertaintySeconds",
        "trafficStart",
        "trafficEnd",
    )
    require(
        "src/game/navigation/LocalGuidancePlanner.cpp",
        "TrajectoryPredictor::predict",
        "TrajectorySafetyEvaluator::evaluate",
        "evaluateDockingTerminalState",
        "LocalGuidanceStatus::NoTerminalSolution",
        "GuidanceSource::LocalPlanner",
        "LocalGuidanceStatus::Blocked",
    )
    forbid(
        "src/game/navigation/LocalGuidancePlanner.cpp",
        "last.centerMeters = dockingTerminalPointAt",
    )
    require(
        "src/game/navigation/HubCoMovingFrame.h",
        "HubCoMovingFrameSeed",
        "makeHubCoMovingFrameSeed",
        "predictHubCoMovingFrameAt",
    )
    require(
        "src/game/navigation/HubSemanticAnchor.h",
        "HubSemanticAnchorKind",
        "DockingPort",
        "AttackPoint",
        "DockOrientationPolicy",
        "Upright",
        "resolveHubSemanticAnchor",
    )
    require(
        "src/game/navigation/DockingCompatibility.h",
        "ShipDockingEnvelope",
        "DockingPortRuntimeState",
        "evaluateDockingCompatibility",
        "result.routeAvailable",
        "result.geometryFits",
        "result.free",
        "result.accessAllowed",
        "result.operational",
    )
    require(
        "src/game/navigation/DockingRouteRequest.h",
        "DockingRouteRequestState",
        "NavigationRouteAnchorKind::SemanticAnchor",
    )
    require(
        "src/game/client/ClientHubMapBridge.h",
        "source.hubAttachment.moduleId",
        "currentLocalRotationDeg",
        "localAngularVelocityDegPerSecond",
        "universeTimeSeconds",
    )
    require(
        "src/game/system_map/LocalMapPresentationBuilder.cpp",
        'item.objectId = "hub-module:" + object.stableId',
        "MapObjectInfoKind::Infrastructure",
        "item.pickPriority = 20",
        "item.pointerInteractive = false",
    )
    require(
        "src/game/system_map/MapObjectOverlay.h",
        "primaryPressStarted",
        "if (m_activeObjectId == objectId)",
    )
    require(
        "src/game/system_map/SystemMapRenderer.cpp",
        "decorateHubDockingOverlay",
        "MapObjectInfoKind::DockingPort",
        'calculate.key = "calculate_docking_route"',
        "calculate.enabled = compatibility.routeAvailable",
        "item.pickPriority = 200",
        "pickHubInfrastructureBody",
        "hubInfrastructureBodyHitDepth",
        "actual assembly",
        "rememberInfrastructure",
        "rememberSemanticAnchor",
        "dockingRouteRequests().request",
        "cancelDockingTaskForClosedCard",
        "dockingRouteRequests().clear",
        "decorateActiveGuidanceTrajectory",
        "predictHubCoMovingFrameAt",
        "futureFrame.worldToLocalPosition",
        "anchorsForModule(module.stableId)",
        "item.hitPolygonPx",
    )
    require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "drawProjectedTrajectory",
        "MapTrajectoryKind::Planned",
        "GL_LINE_STRIP",
        "continuous and time-invariant",
    )
    forbid(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "cubicBezierPoint",
    )
    require(
        "src/assets/data/navigation/hub_docking_runtime_test.json",
        '"occupancy": "occupied"',
        '"access": "denied"',
        '"access": "allowed"',
    )
    require(
        "src/assets/models/hub/guidance_test/guidance_dock_cube.obj",
        "Front lower docking apron",
        "Rear lower docking apron",
    )
    require(
        "src/assets/models/hub/guidance_test/guidance_dock_cylinder.obj",
        "Front lower docking apron",
        "Rear lower docking apron",
    )
    require(
        "src/game/navigation/GalacticReferenceFrame.cpp",
        "galactic_center_dir",
        "galactic_north_dir",
        "galacticAnglesForDirection",
    )
    require(
        "src/game/SpaceState.cpp",
        "buildGuidanceCorridorHudPresentation",
        "buildGalacticCompassPresentation",
        "StrategicTrajectoryPlanner::plan",
        "ClientNavigationPlanningSnapshotFactory",
        "planningUniverseTime",
        "m_lastStrategicDockingRequestSerial",
        "GuidanceSource::RouteSolver",
        "toggleNavigationModule",
    )
    require(
        "src/game/navigation/StrategicTrajectoryPlanner.h",
        "startVelocityMps",
        "visibilityPath",
        "approachPointMeters",
        "terminalPointMeters",
        "segmentClear",
    )
    require(
        "src/game/SpaceState.h",
        "setNavigationModuleEnabled",
        "setAllNavigationHudLayersEnabled",
        "GuidanceCorridorRenderer",
        "GalacticCompassRenderer",
    )
    require(
        "src/game/geometry/MeshLibrary.cpp",
        "assets/models/hub/guidance_test/guidance_dock_cube.obj",
        "assets/models/hub/guidance_test/guidance_dock_cylinder.obj",
    )
    require(
        "src/assets/data/navigation/hub_semantic_anchors.json",
        "guidance_dock_cube_a",
        "guidance_dock_cylinder_b",
        "docking_port",
    )

    require(
        "src/game/navigation/NavigationPlanningEpoch.h",
        "NavigationPlanningEpoch",
        "serverTimeSeconds",
        "universeTimeSeconds",
        "universeTimelineRevision",
    )
    require(
        "src/game/navigation/ReplicatedHubFrame.h",
        "makeReplicatedHubKinematicFrame",
        "X = normal, Y = radial, Z = -prograde",
        "X = prograde, Y = radial, Z = normal",
    )
    require(
        "src/game/navigation/NavigationWorldPredictor.h",
        "NavigationWorldPredictor",
        "predictHubFrameAt",
        "resolveHubAttachmentAt",
        "predictHubLocalConstantVelocity",
    )
    require(
        "src/game/navigation/HubKinematicEvaluator.h",
        "evaluateOrbitalHubKinematicFrameAt",
        "Server simulation and client-side planning prediction",
        "X = prograde, Y = radial, Z = normal",
    )
    require(
        "src/game/navigation/HubKinematicEvaluator.cpp",
        "computeOrbitPositionMeters",
        "computeOrbitVelocityMetersPerSecond",
        "angularVelocityWorldRadPerSecond",
    )
    require(
        "src/game/shared/SpatialComputationPlacement.h",
        "SpatialComputationPlacement",
        "ClientLocal",
        "ServerShared",
        "humanParticipantCount > 1",
        "server-resolved consistency domain",
    )
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "sampleHubMapRuntimeAtServerTime",
        "NavigationWorldPredictor::predictHubFrameAt",
        "sourceEpoch",
        "planningServerTimeSeconds",
        "CoordinateRoundTripToleranceMeters",
    )
    forbid(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "renderTransform",
        "renderWorldPosition",
        "renderOrientation",
        "universeTimeSeconds()",
        "renderServerTimeSeconds()",
    )

    simulation = text("src/game/simulation/GameSimulation.cpp")
    rebuild_begin = simulation.index(
        "void GameSimulation::rebuildHubNavigationFrames"
    )
    rebuild_end = simulation.index(
        "void GameSimulation::prepareReferenceFramesForSpawn",
        rebuild_begin,
    )
    rebuild_hubs = simulation[rebuild_begin:rebuild_end]
    if "evaluateOrbitalHubKinematicFrameAt" not in rebuild_hubs:
        raise AssertionError(
            "authoritative Hub runtime no longer uses the shared kinematic evaluator"
        )
    for duplicate in (
        "computeOrbitPositionMeters(",
        "computeOrbitVelocityMetersPerSecond(",
    ):
        if duplicate in rebuild_hubs:
            raise AssertionError(
                "GameSimulation::rebuildHubNavigationFrames reintroduced duplicate Hub orbit math: "
                + repr(duplicate)
            )

    spawn_begin = simulation.index(
        "void GameSimulation::prepareReferenceFramesForSpawn"
    )
    spawn_end = simulation.find("\nvoid GameSimulation::", spawn_begin + 1)
    if spawn_end < 0:
        spawn_end = len(simulation)
    spawn_frames = simulation[spawn_begin:spawn_end]
    if "rebuildHubNavigationFrames(0.0)" not in spawn_frames:
        raise AssertionError(
            "spawn preparation no longer reuses the authoritative Hub evaluator path"
        )
    for duplicate in (
        "computeOrbitPositionMeters(",
        "computeOrbitVelocityMetersPerSecond(",
    ):
        if duplicate in spawn_frames:
            raise AssertionError(
                "prepareReferenceFramesForSpawn reintroduced bootstrap-only Hub orbit math: "
                + repr(duplicate)
            )

    space_state = text("src/game/SpaceState.cpp")
    guidance_begin = space_state.index("void SpaceState::updateDockingGuidance")
    guidance_end = space_state.find("\nvoid SpaceState::", guidance_begin + 1)
    if guidance_end < 0:
        guidance_end = len(space_state)
    docking_guidance = space_state[guidance_begin:guidance_end]
    for forbidden in (
        "renderTransform",
        "renderWorldPosition",
        "celestialSnapshot()",
        "world().hubs()",
        "m_client->universeTimeSeconds()",
        "hubVisualLocalToWorldPosition(",
        "hubAttachedVisualOrientation(",
    ):
        if forbidden in docking_guidance:
            raise AssertionError(
                "SpaceState::updateDockingGuidance: forbidden planning source "
                + repr(forbidden)
            )

    # Core physics/planning components must remain reusable on server/headless.
    for path in (
        "src/game/navigation/TrajectoryPredictor.cpp",
        "src/game/navigation/TrajectorySafetyEvaluator.cpp",
        "src/game/navigation/LocalGuidancePlanner.cpp",
        "src/game/navigation/NavigationWorldPredictor.cpp",
        "src/game/navigation/HubKinematicEvaluator.cpp",
    ):
        forbid(
            path,
            "GameClient",
            "GameServer",
            "SystemMapRenderer",
            "GuidanceCorridorRenderer",
            "RadarWidget",
            "websocket",
        )

    print("[PASS] modular navigation guidance + 4D safety + HUD layer boundaries")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
