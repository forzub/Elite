#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def text(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        raise AssertionError(f"missing {path}")
    return p.read_text(encoding="utf-8")

def require(path: str, *tokens: str) -> str:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")
    return body

def main() -> int:
    state = require(
        "src/game/navigation/NavigationTrackingState.h",
        "NavigationArrivalMode",
        "NavigationWaypointTransitKind",
        "Rendezvous",
        "SafeZone",
        "Follow",
        "Formation",
        "ParadeFormation",
        "orderedRouteWaypoints",
        "moveIntermediateWaypoint",
        "routeVisibleOnHud",
        "showOnHud",
        "dynamicTarget",
        "targetEntityId",
        "waypoint.role == NavigationWaypointRole::None &&",
        "clearRoute",
    )
    if "return open.find(waypoint.sourceObjectId) == open.end();" in state:
        raise AssertionError("route intent regressed to source-card lifetime")

    overlay = require(
        "src/game/system_map/NavigationRouteOverlay.cpp",
        "SHOW ON HUD",
        "DRAG WAYPOINTS LIVE",
        "DELETE ROUTE?",
        "DELETE WAYPOINT?",
        "localizedYes",
        "localizedNo",
        "m_liveNodeDrag",
        "selectedSourceObjectId",
        "moveIntermediateWaypoint",
        "setFinishArrivalMode",
        "removeRouteWaypoint",
        "clearRoute",
        "drawArrivalGlyph",
        "reorderOffsetPx",
        "focusSourceObjectId",
        "БЕЗОП. ЗОНА",
        "ЗВЕНО",
    )
    if "Trajectory" in overlay or "delta" in overlay.lower():
        raise AssertionError("route authoring UI should not expose solver complexity")

    require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'waypointAction.key = "toggle_intermediate"',
        'finishAction.key = "toggle_finish"',
        '"set_waypoint"',
        '"set_rendezvous"',
        '"set_finish"',
        "hasFinishWaypoint()",
        "focusRouteWaypoint",
        "revealPendingRouteFocus",
        "m_routeOverlayRenderer.render",
        "m_routeOverlayState.handlePointer",
        "setWaypointRouteMetadata",
        "universal green numbered route pin",
        "item.routeDisplayIndex",
    )
    map_intent = require(
        "src/game/system_map/MapIntent.h",
        "RecallRouteMap",
        "requestedMapMode",
        "recallRouteMap",
        "openBody",
        "openHub",
    )
    require(
        "src/game/SpaceState.cpp",
        "MapIntentType::RecallRouteMap",
        "MapIntentType::OpenBody",
        "MapIntentType::OpenHub",
        "setSystemMapGalaxyMode()",
        "setSystemMapLoadedSystemMode()",
        "setSystemMapDetailMode()",
        "setSystemMapHubMode()",
    )
    renderer = text("src/game/system_map/SystemMapRenderer.cpp")
    route_recall = renderer[
        renderer.index("SystemMapRenderer::focusRouteWaypoint"):renderer.index("SystemMapRenderer::revealPendingRouteFocus")
    ]
    if "setMode(Mode::" in route_recall:
        raise AssertionError("route recall must leave map-mode changes to SpaceState")
    for token in ("MapIntent::recallRouteMap", "MapIntent::openBody", "MapIntent::openHub"):
        if token not in route_recall:
            raise AssertionError(f"route recall missing canonical intent {token}")

    require(
        "src/game/presentation/NavigationHudPresentation.h",
        "tracking.routeVisibleOnHud()",
        "tracking.orderedRouteWaypoints()",
        "waypoint.showOnHud",
        "waypoint.dynamicTarget",
        "resolveTacticalKinematics",
        "static_cast<int>(tracking.routeSize())",
        "std::to_string(marker.displayIndex)",
    )
    require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "drawRoutePoint",
        "square + center dot",
        "routeDisplayIndex",
        "item.routeDisplayIndex > 0",
        "0.38f, 0.96f, 0.58f",
    )
    require(
        "src/game/navigation/ROUTE_NAVIGATION_CONTRACT.md",
        "simple, visual and almost cartoon-like",
        "Route Plan",
        "Trajectory Solution",
        "Autopilot",
        "SAFE",
        "FOLLOW",
        "FORMATION",
        "PARADE",
        "ROUTE RENDEZVOUS",
        "double-click",
        "trajectory prediction is a shared service",
    )

    print("[PASS] persistent simple Route Plan + arrival-profile contract")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
