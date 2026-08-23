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
        "GuidanceSource::LocalPlanner",
        "LocalGuidanceStatus::Blocked",
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
        "toggleNavigationModule",
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

    # Core physics/planning components must remain reusable on server/headless.
    for path in (
        "src/game/navigation/TrajectoryPredictor.cpp",
        "src/game/navigation/TrajectorySafetyEvaluator.cpp",
        "src/game/navigation/LocalGuidancePlanner.cpp",
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
