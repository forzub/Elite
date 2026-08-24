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
            raise AssertionError(f"{path}: missing geometric-routing invariant {token!r}")


def forbid(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{path}: forbidden duplicate routing token {token!r}")


try:
    # One canonical obstacle geometry is shared by repair-drone and ship paths.
    require(
        "src/world/navigation/NavigationObstacle.h",
        "NavigationObstacleShape",
        "Sphere",
        "Box",
        "Capsule",
        "glm::dvec3 centerMeters",
        "glm::dmat3 localToWorldBasis",
        "conservativeRadiusMeters",
    )
    require(
        "src/world/navigation/NavigationObstacleGeometry.cpp",
        "segmentIntersectsAabb",
        "segmentSegmentDistanceSquared",
        "NavigationObstacleShape::Box",
        "NavigationObstacleShape::Capsule",
    )
    require(
        "src/world/navigation/GeometricPathPlanner.cpp",
        "GeometricPathPlanner::plan",
        "addBoxSupportNodes",
        "addSphereSupportNodes",
        "addCapsuleSupportNodes",
        "priority_queue",
        "simplifyPath",
    )

    # Docking semantics compose the generic planner; they do not own a second
    # visibility graph or force the geometric path to follow current velocity.
    require(
        "src/game/navigation/DockingPathPlanner.cpp",
        "GeometricPathPlanner::plan",
        "targetObstacleId",
        "docking ingress is blocked by non-target obstacle",
    )
    forbid(
        "src/game/navigation/DockingPathPlanner.h",
        "startVelocityMps",
        "startLeadSeconds",
        "minimumStartLeadMeters",
    )

    # Client planning builds real OBB/capsule geometry once, at the frozen
    # planning epoch, instead of SpaceState inventing magic obstacle radii.
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "makeNavigationObstacleForObject",
        "navigationObstacles",
        "targetNavigationObstacleId",
    )
    forbid(
        "src/game/SpaceState.cpp",
        "StrategicTrajectoryObstacle",
        "GuidanceDockCylinder ? 650.0",
        "GuidanceDockCube ? 520.0",
    )

    # Repair drone and docking ship must execute the same path-search engine.
    require(
        "src/world/modules/ObjectRepairJobRuntime.cpp",
        "GeometricPathPlanner::plan",
        "buildRepairDroneGeometricPath",
        "segmentClearOfNavigationObstacles",
    )
    for legacy in (
        "src/world/navigation/ObstacleAvoidance.cpp",
        "src/world/navigation/ObstacleAvoidance.h",
        "src/world/navigation/ObstaclePathPlanner.cpp",
        "src/world/navigation/ObstaclePathPlanner.h",
        "src/game/navigation/StrategicTrajectoryPlanner.h",
    ):
        if (ROOT / legacy).exists():
            raise AssertionError(f"legacy parallel route engine still exists: {legacy}")


    # The identical deterministic backend is compiled into both execution
    # owners. SpatialComputationPlacement may later choose client-local or
    # server-shared without changing route geometry.
    cmake = text("CMakeLists.txt")
    if cmake.count("src/world/navigation/GeometricPathPlanner.cpp") < 2:
        raise AssertionError("GeometricPathPlanner is not available to both client and server")
    if cmake.count("src/game/navigation/DockingPathPlanner.cpp") < 2:
        raise AssertionError("DockingPathPlanner is not available to both client and server")

    # The dynamic safety snapshot wraps the same canonical physical geometry
    # instead of defining another NavigationObstacle shape/size model.
    require(
        "src/game/navigation/NavigationPlanningSnapshot.h",
        "struct NavigationObstacleState",
        "world::navigation::NavigationObstacle geometry",
    )
    forbid(
        "src/game/navigation/NavigationPlanningSnapshot.h",
        "struct NavigationObstacle\n",
        "double physicalRadiusMeters = 0.0;\n    double requiredClearanceMeters",
    )

    require(
        "tests/navigation_guidance/NavigationGuidanceTests.cpp",
        "testGeometricPlannerKeepsClearDirectPath",
        "testGeometricPlannerDetoursRotatedObb",
        "testGeometricPlannerUsesSphereBoxAndCapsuleKernel",
        "testDockingPathPreservesPhysicalEndpointsAndIngress",
        "testGeometricPlannerIsDeterministicAndInputPure",
    )

    print("[PASS] canonical obstacle geometry + shared geometric path planner")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
