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
            raise AssertionError(f"{path}: missing manual-guidance invariant {token!r}")


def forbid(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{path}: forbidden manual-guidance dependency {token!r}")


try:
    require(
        "src/world/navigation/GuidanceTunnel.h",
        "struct GuidanceTunnelGate",
        "distanceAlongTunnelMeters",
        "sourceTrajectoryProgressMeters",
        "passedTrajectoryProgressMeters",
        "gateSpacingMeters",
        "gateWidthMeters",
        "gateHeightMeters",
        "terminalAllowedObstacleId",
        "GuidanceTunnelBuilder",
    )
    require(
        "src/world/navigation/GuidanceTunnel.cpp",
        "nearestTrajectoryProgress",
        "buildDynamicCurve",
        "SmoothPathOptimizer::optimize",
        "travelForward",
        "currentVelocityMps",
        "terminalForward",
        "minimumTurnRadiusMeters",
        "minimumInteriorSupportDistance",
        "transitionBudget",
        "allowPolylineFallback = false",
        "rotationMinimizingOrientations",
        "terminalTwist",
        "appendGate(0.0)",
        "appendGate(total)",
        "passedTrajectoryProgressMeters",
    )
    for path in (
        "src/world/navigation/GuidanceTunnel.h",
        "src/world/navigation/GuidanceTunnel.cpp",
    ):
        forbid(
            path,
            "GameClient",
            "ClientWorldState",
            "SpaceState",
            "SystemMapRenderer",
            "GuidanceCorridorRenderer",
            "ProceduralCloud",
            "GameSimulation",
        )

    require(
        "src/game/SpaceState.cpp",
        "refreshActiveManualDockingGuidance",
        "fixedTunnel",
        "firstActiveGateIndex",
        "player.renderTransform.worldPosition",
        "targetAnchor.positionMeters",
        "liveObstacles",
        "spatialManualTunnel = true",
        "compatibility.openingWidthMeters",
        "compatibility.openingHeightMeters",
        "vehicleLength * 2.5",
        "minimumVisualTurnRadiusMeters",
    )

    space = text("src/game/SpaceState.cpp")
    refresh_start = space.index("bool SpaceState::refreshActiveManualDockingGuidance(bool forceRebuild)")
    refresh_end = space.index("// =====================================================================================\n// Update", refresh_start)
    refresh = space[refresh_start:refresh_end]
    for token in (
        "renderUniverseTimeSeconds()",
        "player.renderTransform.worldPosition",
        "player.renderTransform.orientation",
        "-targetAnchor.forward()",
        "targetAnchor.up()",
        "world::navigation::orientationForForwardUp",
        "presentationUniverseTimeSeconds",
        "fixedTunnel",
        "replanCheckIntervalSeconds",
        "predictedLookAheadSeconds",
        "courseChangeThresholdRadians",
    ):
        if token not in refresh:
            raise AssertionError(
                f"manual guidance refresh missing presentation-frame invariant {token!r}"
            )
    for token in (
        "m_client->universeTimeSeconds()",
        "player.transform.worldPosition",
        "glm::mat3(player.transform.orientation)",
    ):
        if token in refresh:
            raise AssertionError(
                f"manual guidance refresh mixes planning/authoritative frame {token!r}"
            )

    if "manualPlan.fixedTunnel.valid && !forceRebuild" not in refresh:
        raise AssertionError(
            "initial CALCULATE ROUTE no longer reuses the accepted trajectory backbone"
        )
    if "const bool reconnectCurrentPose = true;" in refresh:
        raise AssertionError(
            "initial manual tunnel was regressed into unconditional reconnect planning"
        )

    for token in (
        "forceRebuild",
        "manualPlan.fixedTunnel",
        "passedTunnelDistanceMeters",
        "firstActiveGateIndex",
        "outside",
        "replanCheckIntervalSeconds",
        "predictedLookAheadSeconds",
        "courseChangeThresholdRadians",
        "if (rebuild)",
        "GuidanceTunnelBuilder::build",
    ):
        if token not in refresh:
            raise AssertionError(
                f"manual guidance refresh missing fixed-corridor lifecycle {token!r}"
            )

    update_start = space.index("void SpaceState::updateDockingGuidance(float dt)")
    update_end = space.index("void SpaceState::", update_start + 32) if "void SpaceState::" in space[update_start + 32:] else len(space)
    update = space[update_start:update_end]
    if "refreshActiveManualDockingGuidance(false);" not in update:
        raise AssertionError("steady-state docking update does not use cheap fixed-corridor refresh")
    if "refreshActiveManualDockingGuidance(true)" not in update:
        raise AssertionError("new docking route does not force one tunnel build")

    require(
        "src/game/presentation/GuidanceHudPresentation.h",
        "const bool spatialTunnel = corridor->spatialManualTunnel",
        "opacity",
        "0.34 * (1.0 - smooth) + 0.07 * smooth",
        "Preserve the final dock gate",
    )
    require(
        "src/render/cockpit/GuidanceCorridorRenderer.cpp",
        "frame.opacity",
        "frameOpacity",
    )

    require(
        "src/game/navigation/GuidanceCorridor.h",
        "activePredictive",
        "activeSpatialManualTunnel",
    )
    require(
        "src/game/system_map/SystemMapRenderer.cpp",
        "guidance().activePredictive",
    )

    cmake = text("CMakeLists.txt")
    if cmake.count("src/world/navigation/GuidanceTunnel.cpp") < 2:
        raise AssertionError("GuidanceTunnelBuilder is not linked for both client and server")

    require(
        "src/world/navigation/NavigationObstacleFactory.cpp",
        "ObjectType::Station",
        "logical.width",
        "logical.height",
        "logical.length",
        "NavigationObstacleShape::Box",
    )

    require(
        "tests/navigation_guidance/NavigationGuidanceTests.cpp",
        "testManualGuidanceTunnelMorphsShipPoseToDockPose",
        "testManualGuidanceTunnelReactsToCurrentShipWithoutMovingDock",
        "testManualGuidanceTunnelDropsPassedGates",
        "testManualGuidanceTunnelUsesCurrentVelocityDirection",
        "testSmoothPathRejectsCurvatureViolationWithoutPolylineFallback",
        "manual tunnel changed frame size along its length",
        "terminal gate lost dock top/bottom orientation",
    )

    print("[PASS] rolling predictive manual docking corridor + hard curvature contract")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
