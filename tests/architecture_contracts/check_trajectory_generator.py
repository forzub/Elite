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

    # Stage 5C's custom global B-spline smoother was retired by NAV-RUCKIG-1
    # (9df31a2a). Its symbol remains only so stale callers fail closed; the
    # TrajectoryGenerator must not route through it (no second motion backend).
    require(
        "src/world/navigation/SmoothPathOptimizer.cpp",
        "#ifndef ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT",
        "out.valid = false",
    )
    forbid(
        "src/world/navigation/TrajectoryGenerator.cpp",
        "SmoothPathOptimizer::optimize",
        "buildSmoothedAnchors",
        "actualRadius = trim / tanHalf",
    )
    require(
        "src/world/navigation/TrajectoryGenerator.cpp",
        "maxBrakingAccelerationMps2",
        "maxForwardAccelerationMps2",
        "maxLateralAccelerationMps2",
    )
    require(
        "src/world/navigation/TrajectoryGenerator.h",
        "double universeTimeScale = 1.0",
        "struct TrajectoryGenerationPolicy",
        "Ruckig state-to-state solve policy",
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
    # The client route autopilot (RouteExecutionCore) is the live consumer.
    require(
        "src/game/navigation/autopilot/RouteExecutionCore.cpp",
        "TrajectoryGenerator::generate",
        "pointSpeedConstraints",
    )

    cmake = text("CMakeLists.txt")
    if cmake.count("src/world/navigation/TrajectoryGenerator.cpp") < 2:
        raise AssertionError("TrajectoryGenerator is not available to both client and server")

    # tests/navigation_guidance was retired (91f24d13); the generator's
    # behavioural regressions live in the navigation_ruckig suite.
    require(
        "tests/navigation_ruckig/TrajectoryGeneratorAngularTests.cpp",
        "testAuthoredPathGeometryIsNotRedrawn",
        "testAuthoredArcStaysInsideAccelerationEnvelope",
        "testPlanningReserveStaysBelowHardLateralEnvelope",
        "TRAJECTORY GENERATOR ANGULAR TESTS: PASS",
    )
    require(
        "tests/navigation_ruckig/CMakeLists.txt",
        "TrajectoryGeneratorAngularTests.cpp",
    )

    print("[PASS] Ruckig-based time-parameterized trajectory generator; retired smoother fails closed")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
