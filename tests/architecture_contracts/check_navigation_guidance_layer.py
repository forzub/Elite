#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def text(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(rel: str, *tokens: str) -> None:
    body = text(rel)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{rel}: missing current guidance token {token!r}")


try:
    for retired in (
        "src/game/navigation/DockingPathPlanner.h",
        "src/game/navigation/DockingPathPlanner.cpp",
        "src/world/navigation/GuidanceTunnel.h",
        "src/world/navigation/GuidanceTunnel.cpp",
        "src/game/navigation/ManualDockingGuidancePlan.h",
    ):
        if (ROOT / retired).exists():
            raise AssertionError(f"retired guidance file returned: {retired}")

    require(
        "src/game/navigation/GuidanceCorridor.h",
        "GuidanceSource", "DockingComputer", "GuidanceFrame",
        "spatialAdvisoryGates", "recommendedSpeedMps", "NavigationGuidanceState",
    )
    require(
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "GeometricPathPlanner::plan", "segmentClearOfNavigationObstacles",
        "gateSpacingMeters", "brakingMps2", "lateralMps2",
    )
    require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'calculate.key = "show_docking_route"',
        "dockingRouteRequests().request", "cancelDockingTaskForClosedCard",
        "dockingRouteRequests().clear", "decorateActiveGuidanceTrajectory",
    )
    require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "drawProjectedTrajectory", "MapTrajectoryKind::Planned", "GL_LINE_STRIP",
    )
    require(
        "src/game/SpaceState.cpp",
        "updateDockingAdvisory", "buildAuthoritativeHubSnapshot",
        "GuidanceSource::DockingComputer", "spatialAdvisoryGates = true",
        "request.gateSpacingMeters = 500.0",
    )
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "sampleHubMapRuntimeAtServerTime", "sourceEpoch",
        "CoordinateRoundTripToleranceMeters",
    )
    print("[PASS] current docking advisory + shared guidance presentation boundaries")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
