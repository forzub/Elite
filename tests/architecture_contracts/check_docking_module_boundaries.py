#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        raise AssertionError(f"missing required architecture file: {path}")
    return p.read_text(encoding="utf-8")


def forbid(path: str, *tokens: str) -> None:
    text = read(path)
    for token in tokens:
        if token in text:
            raise AssertionError(f"{path} leaks forbidden dependency/token: {token}")


def require(path: str, *tokens: str) -> None:
    text = read(path)
    for token in tokens:
        if token not in text:
            raise AssertionError(f"{path} missing architecture token: {token}")


def main() -> None:
    planner_api = "src/game/navigation/planner/RoutePlannerApi.h"
    follower_api = "src/game/navigation/autopilot/RouteFollowerApi.h"
    traffic_api = "src/game/docking/traffic/DockTrafficControllerApi.h"
    landing_api = "src/game/docking/landing/DockLandingControllerApi.h"

    require(planner_api, "class RoutePlanner", "RoutePlanRequest", "RoutePlan")
    forbid(
        planner_api,
        "DockingAdvisoryPlanner",
        "TrajectoryFollower",
        "ManeuverTrackingController",
        "DockTrafficController",
        "DockLandingController",
    )

    require(follower_api, "class RouteFollower", "RouteFollowerPolicy")
    forbid(
        follower_api,
        "TrajectoryFollower.h",
        "ManeuverTrackingController.h",
        "RoutePlannerApi.h",
        "docking/traffic",
        "docking/landing",
    )

    require(
        traffic_api,
        "class DockTrafficController",
        "insideControlledApproachHorizon",
        "DockingClearance",
    )
    forbid(
        traffic_api,
        "navigation/planner",
        "navigation/autopilot",
        "DockLandingController",
        "AcceptedManeuverProgram",
    )

    require(
        landing_api,
        "class DockLandingController",
        "LandingHandoff",
        "mainEnginePermitted",
    )
    forbid(
        landing_api,
        "navigation/planner",
        "navigation/autopilot",
        "DockTrafficController",
        "AcceptedManeuverProgram",
    )

    require(
        "src/game/navigation/planner/RoutePlanner.cpp",
        "DockingAdvisoryPlanner.h",
        "RoutePlanner::plan",
    )
    forbid(
        "src/game/navigation/planner/RoutePlanner.cpp",
        "docking/traffic",
        "docking/landing",
        "navigation/autopilot",
    )

    require(
        "src/game/navigation/autopilot/RouteFollower.cpp",
        "TrajectoryFollower.h",
        "ManeuverTrackingController.h",
        "RouteFollower::follow",
    )
    forbid(
        "src/game/navigation/autopilot/RouteFollower.cpp",
        "navigation/planner",
        "docking/traffic",
        "docking/landing",
    )

    forbid(
        "src/game/docking/traffic/DockTrafficController.cpp",
        "navigation/",
        "docking/landing",
    )
    forbid(
        "src/game/docking/landing/DockLandingController.cpp",
        "navigation/",
        "docking/traffic",
    )

    require(
        "src/game/server/GameServer.cpp",
        "navigation/planner/RoutePlannerApi.h",
        "navigation/autopilot/RouteFollowerApi.h",
    )
    forbid(
        "src/game/server/GameServer.cpp",
        "navigation/DockingAdvisoryPlanner.h",
        "navigation/TrajectoryFollower.h",
    )
    require(
        "src/game/SpaceState.cpp",
        "navigation/planner/RoutePlannerApi.h",
    )
    forbid(
        "src/game/SpaceState.cpp",
        "navigation/DockingAdvisoryPlanner.h",
    )
    forbid(
        "src/game/server/GameServer.h",
        "ManeuverTrackingController.h",
        "ManeuverTrackingController::Policy",
    )

    cmake = read("CMakeLists.txt")
    for token in (
        "add_library(EliteDockingInfrastructure STATIC",
        "src/game/docking/traffic/DockTrafficController.cpp",
        "src/game/docking/landing/DockLandingController.cpp",
        "src/game/navigation/planner/RoutePlanner.cpp",
        "src/game/navigation/autopilot/RouteFollower.cpp",
    ):
        if token not in cmake:
            raise AssertionError(f"CMake ownership missing: {token}")

    if cmake.count("src/game/navigation/DockingAdvisoryPlanner.cpp") != 1:
        raise AssertionError(
            "DockingAdvisoryPlanner backend must have exactly one build owner"
        )

    print("DOCKING MODULE BOUNDARIES: PASS")
    print(" - Planner and Autopilot expose public APIs over private legacy backends")
    print(" - Traffic and Landing compile as a separate infrastructure module")
    print(" - server/client production callers no longer include planner/follower internals")
    print(" - public APIs reject forbidden cross-module dependencies")


if __name__ == "__main__":
    main()
