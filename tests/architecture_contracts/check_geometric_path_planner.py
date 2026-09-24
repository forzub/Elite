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

    # Current manual docking advisory composes the generic planner; it does
    # not own a second visibility graph or a separate obstacle geometry model.
    require(
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "GeometricPathPlanner::plan",
        "segmentClearOfNavigationObstacles",
        "gateSpacingMeters",
        "dock alignment blocked",
    )
    forbid(
        "src/game/navigation/DockingAdvisoryPlanner.h",
        "startLeadSeconds",
        "minimumStartLeadMeters",
        "targetObstacleId",
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

    # Repair drone and docking advisory must execute the same path-search engine.
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


    # The deterministic backend is compiled once into the shared
    # EliteNavigationGeometry library. Client/server runtime layers reuse that
    # target; duplicating the .cpp in multiple executables is no longer the
    # ownership contract.
    cmake = text("CMakeLists.txt")
    if cmake.count("src/world/navigation/GeometricPathPlanner.cpp") != 1:
        raise AssertionError(
            "GeometricPathPlanner must be compiled once by EliteNavigationGeometry"
        )
    if "add_library(EliteNavigationGeometry STATIC" not in cmake:
        raise AssertionError("shared EliteNavigationGeometry target is missing")
    if "PUBLIC EliteNavigationGeometry" not in cmake:
        raise AssertionError(
            "Navigation world runtime no longer reuses EliteNavigationGeometry"
        )

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
        "tests/navigation_runtime/GeometricPathPlannerTests.cpp",
        "testClearDirectPath",
        "testRotatedObbDetour",
        "testSphereBoxCapsuleKernel",
        "testDeterministicInputPure",
    )
    require(
        "tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp",
        "r.gateSpacingMeters=500.0",
        "far dock failed:",
        "corridor entry/exit semantics failed",
    )

    print("[PASS] canonical obstacle geometry + shared geometric path planner")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
