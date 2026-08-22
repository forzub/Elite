#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(path: str) -> str:
    p = ROOT / path
    if not p.exists():
        raise AssertionError(f"missing {path}")
    return p.read_text(encoding="utf-8")

def require(path: str, *tokens: str) -> str:
    body = read(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")
    return body

def main() -> int:
    if (ROOT / "src/game/navigation/NavigationTrackingState.h").exists():
        raise AssertionError("legacy combined NavigationTrackingState must stay removed")

    workspace = require(
        "src/game/navigation/ClientNavigationWorkspace.h",
        "class ClientNavigationWorkspace",
        "TargetTrackingState m_targets",
        "RoutePlan m_routePlan",
        "TargetTrackingState& targets()",
        "RoutePlan& routePlan()",
    )
    require(
        "src/game/navigation/TargetTrackingState.h",
        "class TargetTrackingState",
        "Transient client presentation tracking",
        "numberedShipTarget",
        "NavigationTrackedCelestialBody",
    )
    route = require(
        "src/game/navigation/RoutePlan.h",
        "class RoutePlan",
        "struct RouteTargetRef",
        "ShipInstanceId shipInstanceId",
        "sameRouteTarget",
        "findById",
        "findByTarget",
        "bindPresentationSource",
        "moveIntermediateWaypoint(std::uint64_t nodeId",
        "removeRouteWaypoint(std::uint64_t nodeId)",
    )
    for forbidden in (
        "EntityId",
        "targetEntityId",
        "SystemMapRenderer",
        "MapObjectOverlay",
        "render/",
        "system_map/",
    ):
        if forbidden in route:
            raise AssertionError(f"RoutePlan owns forbidden renderer/runtime dependency: {forbidden}")

    renderer_h = require(
        "src/game/system_map/SystemMapRenderer.h",
        "SystemMapRenderer(game::navigation::ClientNavigationWorkspace& navigationWorkspace)",
        "game::navigation::ClientNavigationWorkspace& m_navigationWorkspace",
        "std::uint64_t m_pendingRouteFocusNodeId",
    )
    for forbidden in (
        "NavigationTrackingState",
        "RoutePlan m_",
        "TargetTrackingState m_",
        "navigationTrackingState()",
    ):
        if forbidden in renderer_h:
            raise AssertionError(f"renderer regained navigation ownership: {forbidden}")

    space_h = require(
        "src/game/SpaceState.h",
        "ClientNavigationWorkspace m_navigationWorkspace",
        "SystemMapRenderer m_systemMapRenderer",
    )
    if space_h.index("ClientNavigationWorkspace m_navigationWorkspace") > space_h.index("SystemMapRenderer m_systemMapRenderer"):
        raise AssertionError("workspace must outlive renderer reference")
    require(
        "src/game/SpaceState.cpp",
        "m_systemMapRenderer(m_navigationWorkspace)",
        "buildNavigationHudMarkers(",
        "m_navigationWorkspace,",
    )

    overlay_h = require(
        "src/game/system_map/NavigationRouteOverlay.h",
        "std::uint64_t focusRouteNodeId",
        "std::uint64_t selectedRouteNodeId",
        "game::navigation::RoutePlan& routePlan",
    )
    for forbidden in ("focusSourceObjectId", "selectedSourceObjectId", "NavigationTrackingState"):
        if forbidden in overlay_h:
            raise AssertionError(f"route overlay uses presentation identity as route identity: {forbidden}")

    hud = require(
        "src/game/presentation/NavigationHudPresentation.h",
        "const game::navigation::ClientNavigationWorkspace& navigation",
        "navigation.targets()",
        "navigation.routePlan()",
        "resolveRouteTargetKinematics",
        "target.shipInstanceId",
    )
    if "navigationTrackingState()" in hud:
        raise AssertionError("HUD must not recover navigation state through renderer")

    require(
        "src/game/client/ClientSystemMapShipSampler.h",
        "ShipInstanceId instanceId",
        "out.instanceId = ship.instanceId",
    )
    require(
        "src/game/client/ClientDetailMapRuntimeSampler.h",
        "ShipInstanceId instanceId",
        "out.instanceId = ship.instanceId",
    )
    require(
        "src/world/celestial/SystemMapTypes.h",
        "ShipInstanceId shipInstanceId",
    )
    require(
        "src/world/celestial/LocalSceneTypes.h",
        "ShipInstanceId shipInstanceId",
    )
    overlay_h = require(
        "src/game/system_map/MapObjectOverlay.h",
        "ShipInstanceId shipInstanceId",
        "shipTargetNumberFor",
        "shipTargetLabelFor",
    )
    if "trackNumberFor(" in overlay_h or "trackLabelFor(" in overlay_h:
        raise AssertionError("generic target-number API can number non-ship map objects")

    overlay = require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "numberedShipTarget",
        "item.kind == MapObjectGlyphKind::Ship",
        "item->kind == MapObjectGlyphKind::Ship",
    )
    if "numberedShipTarget\n                    ? state.shipTargetLabelFor(item.objectId)" not in overlay:
        raise AssertionError("map glyph numbering is no longer guarded by ship kind")
    if "numberedShipTarget\n                ? state.shipTargetLabelFor(item->objectId)" not in overlay:
        raise AssertionError("info-card numbering is no longer guarded by ship kind")

    hud_numbering = require(
        "src/game/presentation/NavigationHudPresentation.h",
        "for (const auto& [id, tracked] : navigation.targets().celestialBodies())",
        "marker.displayIndex = 0",
        "marker.displayIndex = tracked.displayIndex",
    )
    celestial_loop = hud_numbering[
        hud_numbering.index("navigation.targets().celestialBodies()"):
        hud_numbering.index("if (!navigation.routePlan().routeVisibleOnHud())")
    ]
    if "marker.displayIndex = 0" not in celestial_loop:
        raise AssertionError("celestial HUD markers regained target numbers")

    print("[PASS] renderer-independent navigation workspace + stable route identity + ship-only target numbering")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
