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
    route = require(
        "src/game/navigation/RoutePlan.h",
        "NavigationArrivalMode",
        "NavigationWaypointTransitKind",
        "Rendezvous",
        "SafeZone",
        "Follow",
        "Formation",
        "ParadeFormation",
        "RouteTargetRef",
        "ShipInstanceId shipInstanceId",
        "orderedRouteWaypoints",
        "moveIntermediateWaypoint(std::uint64_t nodeId",
        "routeVisibleOnHud",
        "showOnHud",
        "dynamicTarget",
        "clearRoute",
    )
    if "EntityId" in route or "targetEntityId" in route:
        raise AssertionError("Route Plan regressed to materialized EntityId identity")

    require(
        "src/game/navigation/TargetTrackingState.h",
        "class TargetTrackingState",
        "reconcileOpenCards",
        "numberedShipTarget",
    )
    require(
        "src/game/navigation/ClientNavigationWorkspace.h",
        "TargetTrackingState m_targets",
        "RoutePlan m_routePlan",
    )

    overlay = require(
        "src/game/system_map/NavigationRouteOverlay.cpp",
        "NavigationMapTextProfile",
        "textProfile.routeTitle",
        "textProfile.showOnHud",
        "textProfile.dragWaypoints",
        "textProfile.deleteRoute",
        "textProfile.deleteWaypoint",
        "m_liveNodeDrag",
        "selectedRouteNodeId",
        "moveIntermediateWaypoint",
        "setFinishArrivalMode",
        "removeRouteWaypoint",
        "clearRoute",
        "drawArrivalGlyph",
        "reorderOffsetPx",
        "focusRouteNodeId",
    )
    if "Trajectory" in overlay or "delta" in overlay.lower():
        raise AssertionError("route authoring UI should not expose solver complexity")
    for token in ("locale.rfind", 'ru ? "', 'zh ? "', 'es ? "', 'ja ? "'):
        if token in overlay:
            raise AssertionError(f"route renderer bypasses localization storage: {token}")

    renderer = require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'waypointAction.key = "toggle_intermediate"',
        'finishAction.key = "toggle_finish"',
        '"set_rendezvous"',
        "routeTargetRefForOverlayItem",
        "target.shipInstanceId = item.shipInstanceId",
        "findByTarget",
        "findById",
        "focusRouteWaypoint",
        "revealPendingRouteFocus",
        "m_routeOverlayRenderer.render",
        "m_routeOverlayState.handlePointer",
        "item.routeDisplayIndex",
    )
    if "m_navigationTrackingState" in renderer:
        raise AssertionError("renderer regained ownership of navigation state")

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
        "navigation.routePlan().routeVisibleOnHud()",
        "navigation.routePlan().orderedRouteWaypoints()",
        "waypoint.showOnHud",
        "waypoint.dynamicTarget",
        "resolveRouteTargetKinematics",
        "target.shipInstanceId",
        "std::to_string(marker.displayIndex)",
    )
    require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "drawRoutePoint",
        "square + center dot",
        "routeDisplayIndex",
        "numberedShipTarget",
        "MapObjectGlyphKind::Ship",
    )

    map_localization = text("src/assets/localization/ui/maps/map.json")
    confirmation_localization = text("src/assets/localization/ui/common/confirmation.json")
    for key in (
        "map.route.title",
        "map.route.show_on_hud",
        "map.route.waypoint",
        "map.route.finish",
        "map.route.drag_waypoints",
        "map.route.delete_route",
        "map.route.delete_waypoint",
        "map.route.arrival.safe_zone",
        "map.route.arrival.follow",
        "map.route.arrival.formation",
        "map.route.arrival.parade",
    ):
        if key not in map_localization:
            raise AssertionError(f"route/map localization missing {key}")
    for key in ("confirm.yes", "confirm.no"):
        if key not in confirmation_localization:
            raise AssertionError(f"confirmation localization missing {key}")

    require(
        "src/game/navigation/ROUTE_NAVIGATION_CONTRACT.md",
        "simple, visual and almost cartoon-like",
        "ClientNavigationWorkspace",
        "RoutePlan",
        "RouteTargetRef",
        "Trajectory Solution",
        "Autopilot",
    )

    print("[PASS] renderer-independent Route Plan + stable route identity contract")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
