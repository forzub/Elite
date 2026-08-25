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
            raise AssertionError(f"{path}: missing trajectory invariant {token!r}")


def forbid(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{path}: forbidden trajectory dependency {token!r}")


try:
    require(
        "src/world/navigation/Trajectory.h",
        "struct TrajectorySample",
        "timeOffsetSeconds",
        "universeTimeSeconds",
        "pathProgressMeters",
        "sourcePathProgressMeters",
        "positionMeters",
        "velocityMps",
        "accelerationMps2",
        "orientation",
        "angularVelocityRadPerSecond",
        "struct Trajectory",
        "frameId",
    )
    require(
        "src/world/navigation/NavigationVehicleProfile.h",
        "collisionRadiusMeters",
        "maxSpeedMps",
        "maxForwardAccelerationMps2",
        "maxBrakingAccelerationMps2",
        "maxLateralAccelerationMps2",
    )

    # Stage 5C replaces local corner fillets with one global cubic B-spline
    # search. The coarse path selects topology; the smoother is allowed to
    # deviate/lengthen it, while canonical obstacle safety remains absolute.
    require(
        "src/world/navigation/SmoothPathOptimizer.h",
        "class SmoothPathOptimizer",
        "sourceProgressMeters",
        "maxSupportLevel",
        "maxCurvaturePerMeter",
        "curvatureVariation",
    )
    require(
        "src/world/navigation/SmoothPathOptimizer.cpp",
        "openUniformKnots",
        "evaluateSpline",
        "adaptiveSample",
        "segmentIntersectsNavigationObstacle",
        "Smoothness dominates distance deliberately",
        "selectedSupportLevel",
    )
    forbid(
        "src/world/navigation/TrajectoryGenerator.cpp",
        "buildSmoothedAnchors",
        "actualRadius = trim / tanHalf",
        "radiusMeters * tanHalf",
    )
    require(
        "src/world/navigation/TrajectoryGenerator.cpp",
        "SmoothPathOptimizer::optimize",
        "maxBrakingAccelerationMps2",
        "maxForwardAccelerationMps2",
        "maxLateralAccelerationMps2",
    )
    require(
        "src/world/navigation/TrajectoryGenerator.h",
        "double universeTimeScale = 1.0",
        "maxSmoothSupportLevel",
        "maxCurveChordErrorMeters",
    )

    for path in (
        "src/world/navigation/SmoothPathOptimizer.h",
        "src/world/navigation/SmoothPathOptimizer.cpp",
        "src/world/navigation/TrajectoryGenerator.h",
        "src/world/navigation/TrajectoryGenerator.cpp",
        "src/world/navigation/Trajectory.h",
        "src/world/navigation/NavigationVehicleProfile.h",
    ):
        forbid(
            path,
            "GameClient",
            "ClientWorldState",
            "GameServer",
            "GameSimulation",
            "SpaceState",
            "SystemMapRenderer",
            "GuidanceCorridor",
            "ProceduralCloud",
        )

    require(
        "src/game/navigation/NavigationVehicleProfileAdapters.h",
        "const ShipParams& params",
        "const VehicleGuidanceEnvelope& envelope",
        "const world::navigation::NavigationAgentProfile& agent",
        "NavigationVehicleProfile",
    )
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.h",
        "HubPredictionSource hubPredictionSource",
    )
    require(
        "src/game/SpaceState.cpp",
        "TrajectoryGenerator::generate",
        "preferredTurnRoom",
        "buys smoothness with distance",
        "maxSmoothSupportLevel",
        "pointSpeedConstraints",
        "speedLimitRanges",
        "terminalAllowedObstacleId",
        "[Trajectory] request=",
    )

    cmake = text("CMakeLists.txt")
    if cmake.count("src/world/navigation/SmoothPathOptimizer.cpp") < 2:
        raise AssertionError("SmoothPathOptimizer is not available to both client and server")
    if cmake.count("src/world/navigation/TrajectoryGenerator.cpp") < 2:
        raise AssertionError("TrajectoryGenerator is not available to both client and server")

    require(
        "tests/navigation_guidance/NavigationGuidanceTests.cpp",
        "testTrajectoryGeneratorParameterizesStraightPath",
        "testTrajectoryGeneratorSmoothsCornerWithoutCuttingObstacle",
        "testTrajectoryGeneratorHonorsHardDockingIngress",
        "testTrajectoryGeneratorRejectsImpossibleInitialBraking",
        "testTrajectoryGeneratorIsDeterministicAndInputPure",
        "testTrajectoryGeneratorUsesGlobalBSplineSmoothing",
    )

    print("[PASS] global smooth time-parameterized trajectory generator")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
