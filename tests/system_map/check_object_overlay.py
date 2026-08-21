#!/usr/bin/env python3
"""Lock tactical object-overlay semantics without requiring OpenGL/GLM runtime."""

from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_text(path: str, *needles: str) -> str:
    text = read(path)
    for needle in needles:
        require(needle in text, f"{path}: missing contract token: {needle!r}")
    return text


def main() -> int:
    overlay = require_text(
        "src/game/system_map/MapObjectOverlay.h",
        "class MapObjectOverlayState",
        "std::unordered_map<std::string, MapObjectInfoPanelState> m_panels",
        "std::unordered_map<std::string, int> m_trackNumbers",
        "MapObjectVelocityMode arrowVelocityMode",
        "std::vector<MapObjectTrajectory> trajectories",
        "std::vector<MapObjectInfoField> extraFields",
        "m_pointerCaptured",
        "bestPhysicalSizeMeters",
        "dominantExternalPhysicalSizeMeters",
        "mapObjectVelocityArrowLengthScale",
        "speedMps / maxReferenceSpeedMps",
        "MapObjectInfoKind",
        "WaypointCandidate",
        "screenAffordance",
        "std::vector<std::string> openObjectIds() const",
        "std::string actionObjectId",
        "std::string activatedObjectId",
        "std::string m_activeObjectId",
        "result.activatedObjectId = panel->objectId",
        "result.activatedObjectId = picked->objectId",
    )
    require("std::optional<MapObjectInfoPanelState>" not in overlay,
            "overlay regressed to a single-card state")

    renderer = require_text(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "drawTriangle(item)",
        "drawHubCube(item)",
        "item.kind == MapObjectGlyphKind::Hub",
        "wrapLabelForWidth",
        "measureTextPx",
        "drawVelocityArrow(item)",
        "mapObjectVelocityArrowLengthScale",
        "drawActiveObjectRing(item)",
        "activePanel ? 2.0f : 1.0f",
        "kGlobalVelocityColor",
        "kLocalVelocityColor",
        "drawLine(item->screenPx, panelCenter",
        "trajectory.points.size() < 2",
        "Only authoritative or",
        "previousGlState",
    )
    require("frame.trajectories.push_back" not in renderer,
            "renderer must not synthesize trajectory samples")

    detail_bridge = require_text(
        "src/game/client/ClientDetailMapBridge.h",
        "object.sizeMeters = glm::dvec3(4000.0, 1500.0, 4000.0);",
        "makeDetailHubObject",
    )
    require(
        'cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);' in detail_bridge,
        "diagnostic cube must retain its authored diagnostic size",
    )

    local_builder = require_text(
        "src/game/system_map/LocalMapPresentationBuilder.cpp",
        "const bool wantsLocalVelocity = state.sceneIsSpatialVolume",
        "item.velocityMode = MapObjectVelocityMode::Local",
        "item.arrowVelocityMode = MapObjectVelocityMode::Local",
        "item.velocityMode = MapObjectVelocityMode::Global",
        "hubItem.wideVelocityArrow = true",
        "hubItem.velocityMode = MapObjectVelocityMode::Local",
        "hubItem.arrowVelocityMode = MapObjectVelocityMode::Global",
        "item.glyphScale = mapObjectGlyphScale",
    )

    detail_bridge = require_text(
        "src/game/client/ClientDetailMapBridge.h",
        "relativeVelocityWorldMps =",
        "travelFrame.localToWorldVector(",
        "ship.localVelocityMps",
        "hasRelativeVelocity = true",
        "globalVelocityMps = ship.worldVelocityMps",
    )

    system_builder = require_text(
        "src/game/system_map/SystemMapSceneFrameBuilder.cpp",
        "MapObjectOverlayItem overlay",
        "overlay.velocityMode = MapObjectVelocityMode::Global",
        "mapObjectGlyphScale(",
        "frame.interaction.objectOverlay.items.push_back",
        "point.physicalSizeMeters",
    )

    system_inl = read("src/game/system_map/SystemMapRendererSystem.inl")
    overlay_start = system_inl.find("void SystemMapRenderer::drawSystemObjectOverlays")
    helper_start = system_inl.find("void SystemMapRenderer::addMapObjectCube", overlay_start)
    require(overlay_start >= 0 and helper_start > overlay_start,
            "cannot locate System object overlay section")
    object_section = system_inl[overlay_start:helper_start]
    require("addMapObjectCube(" not in object_section,
            "System objects regressed to legacy cube markers")

    detail_pass = read("src/game/system_map/DetailMapGeometryPass.cpp")
    require("Hub/ship/infrastructure markers are rendered by the shared tactical" in detail_pass,
            "Detail pass does not document tactical overlay ownership")

    hub_backend = read("src/game/system_map/HubMapBackend.cpp")
    require("rendered by the shared tactical overlay" in hub_backend,
            "Hub ships regressed to backend-owned debug geometry")

    hub_view = require_text(
        "src/game/system_map/HubMapView.h",
        "m_controls.maxZoom = 64.0",
    )
    interaction = require_text(
        "src/game/system_map/LocalMapInteraction.cpp",
        "panZoomAllowance",
        "camera.zoom * 2.0",
    )

    renderer_facade = require_text(
        "src/game/system_map/SystemMapRenderer.cpp",
        "m_objectOverlayState.handlePointer",
        "m_objectOverlayRenderer.render",
        "focusHubSelection",
        "focusTacticalObjectSelection",
        "overlayPointer.activatedObjectId",
        "m_objectOverlayState.activate",
        "m_objectOverlayState.clearActive",
        "m_detailView.selectHub",
        "m_detailView.clearHubSelection",
        "largestDirectBodyPhysicalSizeMetersAt",
    )
    require("overlayPointer.consumed" in renderer_facade,
            "overlay input must suppress underlying map-camera gestures")

    require_text(
        "src/game/system_map/SystemMapInteraction.cpp",
        "focusTacticalObjectSelection",
        "state.selectedBodyId.clear()",
        "state.selectedHubId.clear()",
        "state.navigationGrid.clearSelectedCell()",
        "state.navigationCellExplicitlySelected = false",
    )

    require_text(
        "src/game/system_map/SystemMapRendererCommon.inl",
        "m_objectOverlayState.activate(result.hubId)",
        "m_objectOverlayState.clearActive()",
    )

    pick_context = require_text(
        "src/game/system_map/SystemMapFrameInteractionContext.cpp",
        "bestPhysicalSizeMeters",
        "largestDirectBodyPhysicalSizeMetersAt",
        "point.physicalSizeMeters",
        "bodySize > bestPhysicalSizeMeters",
    )
    require("bestDistance = std::numeric_limits<float>::max()" in pick_context,
            "crowded-pick tie breaking lost deterministic distance fallback")

    localization_path = ROOT / "src/assets/localization/ui/maps/map.json"
    data = json.loads(localization_path.read_text(encoding="utf-8"))
    raw = localization_path.read_text(encoding="utf-8")
    for key in (
        "map.object_info.type",
        "map.object_info.name",
        "map.object_info.local_speed",
        "map.object_info.global_speed",
        "map.object_info.azimuth",
        "map.object_info.elevation",
        "map.object_info.owner",
        "map.object_info.radius",
        "map.object_info.address",
        "map.object_info.space_target",
        "map.object_info.set_finish",
        "map.navigation_hud.object",
        "map.navigation_hud.celestial",
        "map.navigation_hud.finish",
        "map.navigation_hud.waypoint",
        "map.navigation_hud.relative_speed_short",
        "map.navigation_hud.global_speed_short",
    ):
        require(key in raw, f"localization missing {key}")
    require(data is not None, "map localization JSON failed to parse")

    require_text(
        "src/game/client/ClientSystemMapShipSampler.h",
        "#include <glm/gtc/quaternion.hpp>",
        "worldVelocityMps",
        "orientation",
        "glm::slerp",
    )
    require_text(
        "src/game/client/ClientSystemMapInfrastructureSampler.h",
        "#include <glm/gtc/quaternion.hpp>",
        "linearVelocityMps",
        "worldVelocityMps",
        "orientation",
        "glm::slerp",
    )

    require_text(
        "src/game/system_map/LocalMapPresentationBuilder.cpp",
        "object.relativeVelocityWorldMps",
        "hubItem.stellarVelocityMps = glm::dvec3(0.0)",
        "wantsLocalVelocity && object.hasRelativeVelocity",
        "Keep card bearing/elevation tied to the displayed local motion",
    )
    require("std::log10" not in overlay,
            "velocity-vector scale regressed to logarithmic mapping")

    require_text(
        "src/game/navigation/NavigationTrackingState.h",
        "class NavigationTrackingState",
        "NavigationTrackedTacticalObject",
        "NavigationTrackedCelestialBody",
        "std::vector<NavigationWaypoint> m_waypoints",
        "NavigationWaypointRole::Finish",
        "NavigationArrivalMode",
        "orderedRouteWaypoints",
        "moveIntermediateWaypoint",
        "routeVisibleOnHud",
        "reconcileOpenCards",
    )

    hud_presentation = require_text(
        "src/game/presentation/NavigationHudPresentation.h",
        "cockpitNavigationUsesGlobalSpeed",
        "resolveCockpitNavigationTargetSpeed",
        "resolveHubFromPlayerPresentationFrame",
        "player.renderReferenceFrame",
        "MotionMode::Cruise",
        "MotionMode::JumpTransit",
        "playerMotion.travelFrame.worldToLocalVelocity",
        "glm::length(targetLocalVelocity)",
        "NavigationHudVocabulary",
        "NavigationHudMarkerShape::TacticalTriangle",
        "NavigationHudMarkerShape::CelestialDiamond",
        "NavigationHudMarkerShape::WaypointCorners",
    )
    require("targetLocalVelocity - motion.localVelocityMps" not in hud_presentation,
            "cockpit target speed regressed to player-target closing speed")
    require("resolved.worldVelocityMps - playerWorldVelocity" not in hud_presentation,
            "cockpit target speed regressed to raw world-vector subtraction")
    require("server" not in hud_presentation.lower(),
            "client HUD navigation presentation should not acquire server ownership")

    require_text(
        "src/game/ship/view/PlayerShipView.cpp",
        "renderNavigationMarkers",
        "hudEdgeMapper.isInsideBoundary",
        "hudEdgeMapper.projectDirection",
        "Direction-only projection deliberately ignores target distance",
    )
    world_label = require_text(
        "src/render/HUD/WorldLabelRenderer.cpp",
        "renderNavigationMarkers",
        "TacticalTriangle",
        "leftColumnRight",
        "target number still share one right edge",
        "CelestialDiamond",
        "WaypointCorners",
        "truncateHudText",
        "navigationDistanceText",
        "navigationSpeedText",
    )
    require("TacticalPlate" not in world_label,
            "cockpit tactical marker regressed to the wide rectangular plate")
    require_text(
        "src/game/SpaceState.cpp",
        "buildNavigationHudMarkers",
        "navigationTrackingState()",
        "renderNavigationMarkers",
        "canOpenSelectedLocalContext()",
        "setSystemMapDetailMode();",
    )

    require_text(
        "src/game/system_map/SystemMapSceneFrameBuilder.cpp",
        "MapObjectInfoKind::Celestial",
        '"body:" + std::to_string(system.systemId)',
        "trackingWorldPosition",
        "body.radiusKm * 2000.0",
    )
    require_text(
        "src/world/celestial/SystemMapTypes.h",
        "std::string parentHubId",
    )
    require_text(
        "src/game/client/ClientSystemMapShipSampler.h",
        "std::string hubId",
        "out.hubId = ship.transform.motion.hubId",
    )
    require_text(
        "src/game/system_map/SystemMapSceneFrameBuilder.cpp",
        "overlay.navigationHubId",
        "overlay.navigationSystemPositionAu",
        "overlay.hasNavigationSystemPositionAu = true",
    )
    renderer_facade = require_text(
        "src/game/system_map/SystemMapRenderer.cpp",
        "synchronizeNavigationTracking",
        "refreshGalaxyWaypointCandidate",
        "refreshSystemWaypointCandidate",
        "candidate.screenAffordance = true",
        "candidate.pointerInteractive = true",
        "candidate.drawGlyph = true",
        "applyWaypointAction",
        "setWaypointRouteMetadata",
        "set_waypoint",
        "set_finish",
        "updateActiveTacticalLocalContext",
        "navigationHubId",
        "m_activeTacticalDetailCell",
        "clickedBodyId",
        "clickedNavigationCell",
    )
    require("m_navigationTrackingState" in renderer_facade,
            "map facade lost client-only navigation memory")
    require_text(
        "src/game/system_map/SystemMapRenderer.h",
        "void applyWaypointAction(",
        "const std::string& objectId,",
        "const std::string& actionKey",
        "bool canOpenSelectedLocalContext() const",
        "m_activeTacticalDetailCell",
    )
    require_text(
        "src/game/system_map/SystemMapRendererSystem.inl",
        "canOpenSelectedLocalContext() const",
        "m_activeTacticalLocalTargetObjectId",
        "m_activeTacticalDetailCell",
    )
    require_text(
        "tests/system_map/SystemMapBehaviorTests.cpp",
        "const std::string finishSourceId = finish.sourceObjectId",
        "const std::string intermediateSourceId = intermediate.sourceObjectId",
    )

    require_text(
        "src/game/system_map/MAP_OBJECT_OVERLAY_CONTRACT.md",
        "bounded **linear** scale",
        "client-only navigation tracking",
        "same motion regime as the displayed",
    )
    renderer_source = (ROOT / "src/game/system_map/SystemMapRenderer.cpp").read_text(encoding="utf-8")
    require('''m_objectOverlayState.ensureOpen(
                *m_galaxyWaypointCandidate''' not in renderer_source,
            "Galaxy cube selection must not auto-open waypoint card")
    require('''m_objectOverlayState.ensureOpen(
                *m_systemWaypointCandidate''' not in renderer_source,
            "System cube selection must not auto-open waypoint card")


    require_text(
        "src/game/system_map/NavigationRouteOverlay.cpp",
        "SHOW ON HUD",
        "moveIntermediateWaypoint",
        "setFinishArrivalMode",
        "removeRouteWaypoint",
        "clearRoute",
    )
    require_text(
        "src/game/system_map/SystemMapRenderer.cpp",
        "m_routeOverlayState.handlePointer",
        "m_routeOverlayRenderer.render",
    )
    require_text(
        "src/game/presentation/NavigationHudPresentation.h",
        "routeVisibleOnHud",
        "waypoint.showOnHud",
        "waypoint.dynamicTarget",
        "resolveTacticalKinematics",
    )

    require_text(
        "src/game/navigation/NAVIGATION_TRACKING_CONTRACT.md",
        "client-only ownership",
        "persistent route-plan baseline",
        "HudEdgeMapper",
        "MotionMode::Cruise",
        "MotionMode::JumpTransit",
        "NOT IMPLEMENTED",
    )

    print("[PASS] tactical/body cards, linear velocity vectors, client-only tracking, cockpit markers, persistent route-plan seam, displayed-motion bearings and existing map overlay contracts are locked")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        raise SystemExit(1)
