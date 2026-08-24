#include "src/game/system_map/LocalMapPresentationBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>


#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
namespace
{


double detailSpatialCameraDistanceMeters(
    const world::celestial::DetailMapSnapshot& snapshot
)
{
    return std::max(
        snapshot.detailHalfExtentMeters * 5.0,
        1.0
    );
}



glm::dvec3 visualSizeForHubShip(
    const world::celestial::HubMapShip& ship,
    double scale,
    double zoom
)
{
    glm::dvec3 physicalSizeMeters(
        90.0,
        35.0,
        160.0
    );

    if (ship.player)
    {
        physicalSizeMeters =
            glm::dvec3(
                130.0,
                50.0,
                210.0
            );
    }

    const double pixelsPerMeter = scale * zoom;

    if (pixelsPerMeter <= 0.0)
        return physicalSizeMeters;

    const double longestPx =
        std::max(
            physicalSizeMeters.x,
            std::max(
                physicalSizeMeters.y,
                physicalSizeMeters.z
            )
        ) * pixelsPerMeter;

    const double minimumLongestPx =
        ship.player ? 18.0 : 11.0;

    if (longestPx >= minimumLongestPx)
        return physicalSizeMeters;

    const double factor =
        minimumLongestPx /
        std::max(1.0, longestPx);

    return physicalSizeMeters * factor;
}

double localObjectPhysicalSizeMeters(
    const world::celestial::LocalSceneObject& object
)
{
    return std::max({
        std::abs(object.sizeMeters.x),
        std::abs(object.sizeMeters.y),
        std::abs(object.sizeMeters.z),
        object.boundingRadiusMeters * 2.0,
        1.0
    });
}

std::string localObjectTypeName(
    const world::celestial::LocalSceneObject& object
)
{
    if (!object.typeName.empty())
        return object.typeName;
    if (object.objectClass == world::celestial::DetailObjectClass::Ship)
        return object.player ? "Player ship" : "Ship";
    if (object.kind == "hub")
        return "Hub";
    if (object.objectClass == world::celestial::DetailObjectClass::Hub)
        return object.kind.empty() ? "Infrastructure" : object.kind;
    return object.kind.empty() ? "Object" : object.kind;
}

glm::vec4 localObjectColor(
    const world::celestial::LocalSceneObject& object
)
{
    if (object.player)
        return glm::vec4(1.00f, 0.78f, 0.28f, 0.98f);
    if (object.kind == "hub")
        return glm::vec4(0.34f, 0.88f, 1.00f, 0.96f);
    return glm::vec4(0.78f, 0.86f, 0.94f, 0.96f);
}

MapObjectGlyphKind localObjectGlyphKind(
    const world::celestial::LocalSceneObject& object
)
{
    if (object.objectClass == world::celestial::DetailObjectClass::Ship)
        return MapObjectGlyphKind::Ship;
    if (object.kind == "hub")
        return MapObjectGlyphKind::Hub;
    return MapObjectGlyphKind::Infrastructure;
}

struct LocalNavigationHubBinding
{
    std::string hubId;
    std::string parentBodyId;
};

LocalNavigationHubBinding resolveLocalNavigationHubBinding(
    const world::celestial::LocalSceneInventory& scene,
    const world::celestial::LocalSceneObject& object
)
{
    if (object.objectClass == world::celestial::DetailObjectClass::Hub &&
        object.kind == "hub" && !object.stableId.empty())
    {
        return {object.stableId, object.parentStableId};
    }

    if (object.parentStableId.empty())
        return {};

    const auto parentHub = std::find_if(
        scene.objects.begin(),
        scene.objects.end(),
        [&](const world::celestial::LocalSceneObject& candidate)
        {
            return candidate.valid &&
                   candidate.objectClass ==
                       world::celestial::DetailObjectClass::Hub &&
                   candidate.kind == "hub" &&
                   candidate.stableId == object.parentStableId;
        }
    );

    if (parentHub == scene.objects.end())
        return {};

    return {parentHub->stableId, parentHub->parentStableId};
}

glm::dvec2 projectDirection(
    const LocalMapCameraSnapshot& camera,
    const glm::dvec3& position,
    const glm::dvec3& direction,
    double stepMeters
)
{
    if (glm::length(direction) <= 1.0e-12)
        return glm::dvec2(0.0, -1.0);

    const glm::dvec2 start = camera.project(position);
    const glm::dvec2 end = camera.project(
        position + glm::normalize(direction) * std::max(stepMeters, 1.0)
    );
    return normalizedScreenDirection(end - start);
}

glm::dvec3 localSceneObjectWorldMeters(
    const world::celestial::LocalSceneInventory& scene,
    const world::celestial::LocalSceneObject& object
)
{
    if (object.coordinateSpace ==
        world::celestial::LocalSceneCoordinateSpace::SystemWorldMeters)
    {
        return object.positionMeters;
    }
    return scene.originWorldMeters + object.positionMeters;
}

glm::dvec3 hubLocalToWorldMeters(
    const world::celestial::HubMapSnapshot& snapshot,
    const glm::dvec3& localMeters
)
{
    return snapshot.hubWorldPositionMeters +
        snapshot.hubWorldAxes.x * localMeters.x +
        snapshot.hubWorldAxes.y * localMeters.y +
        snapshot.hubWorldAxes.z * localMeters.z;
}

bool visibleInViewport(
    const glm::dvec2& point,
    const Viewport& viewport,
    double margin = 20.0
)
{
    return
        point.x >= -margin &&
        point.y >= -margin &&
        point.x <= static_cast<double>(viewport.width) + margin &&
        point.y <= static_cast<double>(viewport.height) + margin;
}

} // namespace


DetailMapPresentation LocalMapPresentationBuilder::buildDetail(
    DetailMapView& view,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& snapshot
) const
{
    auto& state = view.state();
    const auto& controls = view.controls();

    state.sceneIsSpatialVolume =
        snapshot.valid &&
        snapshot.detailTarget.sceneKind ==
            world::celestial::DetailSceneKind::SpatialVolume;

    state.minimumZoom =
        state.sceneIsSpatialVolume
            ? controls.spatialVolumeMinimumZoom
            : controls.minZoom;

    state.camera.zoom =
        std::max(
            state.camera.zoom,
            state.minimumZoom
        );

    if (state.sceneIsSpatialVolume)
        state.camera.pan = glm::dvec2(0.0);

    DetailMapPresentation presentation;
    presentation.valid = snapshot.valid;
    presentation.systemId = snapshot.systemId;
    presentation.sceneIsSpatialVolume =
        state.sceneIsSpatialVolume;
    presentation.minimumZoom = state.minimumZoom;
    presentation.centerPx =
        glm::dvec2(
            static_cast<double>(viewport.width) * 0.5,
            static_cast<double>(viewport.height) * 0.5
        );

    presentation.selectedHubId = state.selectedHubId;
    presentation.selectedHubParentBodyId =
        state.selectedHubParentBodyId;

    const double spatialCameraDistanceMeters =
        detailSpatialCameraDistanceMeters(snapshot);

    const DetailMapCameraSnapshot fitCamera =
        view.cameraSnapshot(
            1.0,
            presentation.centerPx,
            snapshot.planetCenterMeters,
            state.sceneIsSpatialVolume &&
                snapshot.detailHalfExtentMeters > 0.0,
            spatialCameraDistanceMeters
        );

    presentation.camera = fitCamera;

    if (!snapshot.valid)
        return presentation;

    double maxRadiusMeters = snapshot.planetRadiusMeters;

    for (const auto& orbit : snapshot.hubOrbits)
    {
        if (orbit.valid)
        {
            maxRadiusMeters =
                std::max(maxRadiusMeters, orbit.radiusMeters);
        }
    }

    for (const auto& orbit : snapshot.playerOrbits)
    {
        if (orbit.valid)
        {
            maxRadiusMeters =
                std::max(maxRadiusMeters, orbit.radiusMeters);
        }
    }

    if (!state.sceneIsSpatialVolume)
    {
        for (const auto& object : snapshot.scene.objects)
        {
            if (!object.valid)
                continue;

            /*
                Camera fit belongs to the scene anchor, not to transient
                participants. A fast ship must not pull the camera away from
                the selected planet and cause zoom pumping on every snapshot.
            */
            if (object.objectClass ==
                    world::celestial::DetailObjectClass::Ship &&
                object.role ==
                    world::celestial::LocalSceneObjectRole::Participant)
            {
                continue;
            }

            maxRadiusMeters =
                std::max(
                    maxRadiusMeters,
                    glm::length(
                        object.positionMeters -
                        snapshot.planetCenterMeters
                    )
                );
        }
    }

    if (state.sceneIsSpatialVolume &&
        snapshot.detailHalfExtentMeters > 0.0)
    {
        maxRadiusMeters =
            std::max(
                maxRadiusMeters,
                snapshot.detailHalfExtentMeters *
                    std::sqrt(3.0)
            );
    }
    else if (!snapshot.hasCentralBody)
    {
        maxRadiusMeters =
            std::max(maxRadiusMeters, 100000.0);
    }

    double fitExtentMeters = maxRadiusMeters;

    if (state.sceneIsSpatialVolume &&
        snapshot.detailHalfExtentMeters > 0.0)
    {
        const double halfExtent =
            snapshot.detailHalfExtentMeters;
        fitExtentMeters = 1.0;

        for (int x = -1; x <= 1; x += 2)
        {
            for (int y = -1; y <= 1; y += 2)
            {
                for (int z = -1; z <= 1; z += 2)
                {
                    const glm::dvec3 cameraSpace =
                        fitCamera.vectorToCamera(
                            glm::dvec3(
                                static_cast<double>(x) * halfExtent,
                                static_cast<double>(y) * halfExtent,
                                static_cast<double>(z) * halfExtent
                            )
                        );

                    const double perspectiveFactor =
                        fitCamera.perspectiveFactor(
                            cameraSpace.z
                        );

                    fitExtentMeters =
                        std::max(
                            fitExtentMeters,
                            std::max(
                                std::abs(cameraSpace.x),
                                std::abs(cameraSpace.y)
                            ) * perspectiveFactor
                        );
                }
            }
        }
    }

    const double mapHalfPx =
        std::min(viewport.width, viewport.height) *
        (state.sceneIsSpatialVolume ? 0.43 : 0.42);

    presentation.maxRadiusMeters = maxRadiusMeters;
    presentation.scale =
        mapHalfPx /
        std::max(1.0, fitExtentMeters);

    presentation.camera =
        view.cameraSnapshot(
            presentation.scale,
            presentation.centerPx,
            snapshot.planetCenterMeters,
            state.sceneIsSpatialVolume &&
                snapshot.detailHalfExtentMeters > 0.0,
            spatialCameraDistanceMeters
        );

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid ||
            (object.objectClass != world::celestial::DetailObjectClass::Ship &&
             object.objectClass != world::celestial::DetailObjectClass::Hub))
        {
            continue;
        }

        MapObjectOverlayItem item;
        item.objectId = object.stableId.empty()
            ? "entity:" + std::to_string(object.id.value)
            : object.stableId;
        item.shipInstanceId = object.shipInstanceId;
        item.trackingSystemId = presentation.systemId;
        item.name = object.name;
        item.typeName = localObjectTypeName(object);
        item.kind = localObjectGlyphKind(object);
        const auto navigationHub = resolveLocalNavigationHubBinding(
            snapshot.scene,
            object
        );
        item.navigationHubId = navigationHub.hubId;
        item.navigationHubParentBodyId = navigationHub.parentBodyId;
        item.trackingWorldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                localSceneObjectWorldMeters(snapshot.scene, object)
            );
        item.hasTrackingWorldPosition = true;
        item.factionColor = localObjectColor(object);
        item.screenPx = presentation.camera.project(object.positionMeters);
        item.visible = visibleInViewport(item.screenPx, viewport);
        item.physicalSizeMeters = localObjectPhysicalSizeMeters(object);

        const double perspective = presentation.camera.perspectiveFactor(
            presentation.camera.pointToCamera(object.positionMeters).z
        );
        const double pixelsPerMeter =
            presentation.scale *
            presentation.camera.state.zoom *
            perspective;
        item.glyphScale = mapObjectGlyphScale(
            item.physicalSizeMeters,
            pixelsPerMeter
        );
        item.hitRadiusPx = 15.0 * item.glyphScale;

        const glm::dvec3 forwardWorld = -object.axes.z;
        item.facingScreenDirection = projectDirection(
            presentation.camera,
            object.positionMeters,
            forwardWorld,
            std::max(item.physicalSizeMeters, 1000.0)
        );

        const bool wantsLocalVelocity = state.sceneIsSpatialVolume;
        if (wantsLocalVelocity && object.hasRelativeVelocity)
        {
            item.velocityMode = MapObjectVelocityMode::Local;
            item.arrowVelocityMode = MapObjectVelocityMode::Local;
            item.displayedVelocityMps = object.relativeVelocityMps;
            item.velocityArrowMps = object.relativeVelocityMps;
            item.velocityScreenDirection = projectDirection(
                presentation.camera,
                object.positionMeters,
                object.relativeVelocityWorldMps,
                std::max(item.physicalSizeMeters, 1000.0)
            );
        }
        else
        {
            item.velocityMode = MapObjectVelocityMode::Global;
            item.arrowVelocityMode = MapObjectVelocityMode::Global;
            item.displayedVelocityMps = object.hasGlobalVelocity
                ? object.globalVelocityMps
                : object.velocityMps;
            item.velocityArrowMps = item.displayedVelocityMps;
            item.velocityScreenDirection = projectDirection(
                presentation.camera,
                object.positionMeters,
                item.displayedVelocityMps,
                std::max(item.physicalSizeMeters, 1000.0)
            );
        }
        // Card azimuth/elevation must describe the same motion regime as the
        // displayed speed.  When the Detail scene is a terminal spatial/local
        // view, use the relative motion vector re-expressed in world axes
        // instead of the dominant orbital/global drift.
        item.stellarVelocityMps =
            (wantsLocalVelocity && object.hasRelativeVelocity)
                ? object.relativeVelocityWorldMps
                : (object.hasGlobalVelocity
                    ? object.globalVelocityMps
                    : object.velocityMps);

        presentation.frame.objectOverlay.items.push_back(std::move(item));
    }

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid ||
            object.objectClass !=
                world::celestial::DetailObjectClass::Hub ||
            object.kind != "hub")
        {
            continue;
        }

        const glm::dvec2 projected =
            presentation.camera.project(object.positionMeters);

        DetailHubScreenPoint point;
        point.hubId = object.stableId;
        point.parentBodyId = snapshot.planetBodyId;
        point.name = object.name;
        point.screen =
            glm::vec2(
                static_cast<float>(projected.x),
                static_cast<float>(projected.y)
            );
        point.visible =
            projected.x >= 0.0 &&
            projected.y >= 0.0 &&
            projected.x <= viewport.width &&
            projected.y <= viewport.height;
        point.screenRadiusPx = 15.0f;

        presentation.frame.hubScreenPoints.push_back(
            std::move(point)
        );
    }

    if (!state.selectedHubId.empty())
    {
        const auto selected =
            std::find_if(
                presentation.frame.hubScreenPoints.begin(),
                presentation.frame.hubScreenPoints.end(),
                [&](const DetailHubScreenPoint& point)
                {
                    return point.hubId == state.selectedHubId;
                }
            );

        if (selected == presentation.frame.hubScreenPoints.end())
            view.clearHubSelection();
    }

    presentation.selectedHubId = state.selectedHubId;
    presentation.selectedHubParentBodyId =
        state.selectedHubParentBodyId;

    return presentation;
}


HubMapPresentation LocalMapPresentationBuilder::buildHub(
    const HubMapView& view,
    const Viewport& viewport,
    const world::celestial::HubMapSnapshot& snapshot
) const
{
    HubMapPresentation presentation;
    presentation.valid = snapshot.valid;
    presentation.systemId = snapshot.systemId;
    presentation.hubId = snapshot.hubId;
    presentation.centerPx =
        glm::dvec2(
            static_cast<double>(viewport.width) * 0.5,
            static_cast<double>(viewport.height) * 0.5
        );
    presentation.camera =
        view.cameraSnapshot(
            presentation.scale,
            presentation.centerPx
        );

    if (!snapshot.valid)
        return presentation;

    /*
        Hub Map is anchored on the hub origin. Its camera fit is determined by
        infrastructure only. Ships are dynamic participants and may leave the
        local scene; including them in auto-fit makes the whole map breathe,
        shrink and jump as their trajectory changes.
    */
    double maxDistance = 2500.0;

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid ||
            object.objectClass !=
                world::celestial::DetailObjectClass::Hub)
        {
            continue;
        }

        maxDistance =
            std::max(
                maxDistance,
                glm::length(object.positionMeters) +
                glm::length(object.sizeMeters)
            );
    }

    const double halfPx =
        std::min(viewport.width, viewport.height) * 0.38;

    presentation.scale =
        halfPx /
        std::max(1.0, maxDistance);
    presentation.camera =
        view.cameraSnapshot(
            presentation.scale,
            presentation.centerPx
        );

    presentation.frame.scale = presentation.scale;
    presentation.frame.centerPx = presentation.centerPx;

    const double finalScale =
        presentation.scale * presentation.camera.state.zoom;

    // The hub itself keeps the existing geometry on Hub Map. Only its broad,
    // translucent global-motion arrow is added to the tactical overlay.
    {
        MapObjectOverlayItem hubItem;
        hubItem.objectId = snapshot.hubId;
        hubItem.name = snapshot.displayName.empty()
            ? snapshot.hubId
            : snapshot.displayName;
        hubItem.typeName = "Hub";
        hubItem.kind = MapObjectGlyphKind::Hub;
        hubItem.trackingSystemId = presentation.systemId;
        hubItem.navigationHubId = snapshot.hubId;
        hubItem.navigationHubParentBodyId = snapshot.parentBodyId;
        hubItem.trackingWorldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                snapshot.hubWorldPositionMeters
            );
        hubItem.hasTrackingWorldPosition = true;
        hubItem.velocityMode = MapObjectVelocityMode::Local;
        hubItem.arrowVelocityMode = MapObjectVelocityMode::Global;
        hubItem.displayedVelocityMps = glm::dvec3(0.0);
        hubItem.velocityArrowMps = snapshot.hubWorldVelocityMps;
        // The Hub card on Hub Map reports local speed (which is zero for the
        // reference hub), so its bearing fields should not silently inherit the
        // hub's global orbital drift.
        hubItem.stellarVelocityMps = glm::dvec3(0.0);
        hubItem.factionColor = glm::vec4(0.34f, 0.88f, 1.00f, 0.96f);
        hubItem.physicalSizeMeters = std::max(1.0, maxDistance * 2.0);
        hubItem.screenPx = presentation.camera.project(glm::dvec3(0.0));
        hubItem.visible = visibleInViewport(hubItem.screenPx, viewport);
        hubItem.drawGlyph = false;
        hubItem.wideVelocityArrow = true;
        hubItem.hitRadiusPx = 26.0;

        const glm::dvec3 hubVelocityLocal(
            glm::dot(snapshot.hubWorldVelocityMps, snapshot.hubWorldAxes.x),
            glm::dot(snapshot.hubWorldVelocityMps, snapshot.hubWorldAxes.y),
            glm::dot(snapshot.hubWorldVelocityMps, snapshot.hubWorldAxes.z)
        );
        hubItem.velocityScreenDirection = projectDirection(
            presentation.camera,
            glm::dvec3(0.0),
            hubVelocityLocal,
            1000.0
        );
        hubItem.facingScreenDirection = glm::dvec2(0.0, -1.0);
        presentation.frame.objectOverlay.items.push_back(std::move(hubItem));
    }

    // Hub infrastructure remains rendered by the geometry pass, but it also
    // participates in the shared overlay interaction/card system.  The
    // presentation id is namespaced while semanticTargetId keeps the durable
    // authored hub-module identity.
    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid ||
            object.objectClass != world::celestial::DetailObjectClass::Hub)
        {
            continue;
        }

        MapObjectOverlayItem item;
        item.objectId = "hub-module:" + object.stableId;
        item.semanticTargetId = object.stableId;
        item.trackingSystemId = presentation.systemId;
        item.name = object.name;
        item.typeName = localObjectTypeName(object);
        item.kind = MapObjectGlyphKind::Infrastructure;
        item.infoKind = MapObjectInfoKind::Infrastructure;
        item.navigationHubId = snapshot.hubId;
        item.navigationHubParentBodyId = snapshot.parentBodyId;
        item.trackingWorldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubLocalToWorldMeters(snapshot, object.positionMeters)
            );
        item.hasTrackingWorldPosition = true;
        item.factionColor = localObjectColor(object);
        item.screenPx = presentation.camera.project(object.positionMeters);
        item.visible = visibleInViewport(item.screenPx, viewport);
        item.physicalSizeMeters = localObjectPhysicalSizeMeters(object);
        item.glyphScale = mapObjectGlyphScale(
            item.physicalSizeMeters,
            finalScale
        );
        // Infrastructure body selection is resolved against the actual CPU
        // assembly mesh on the pointer press.  Keeping the broad glyph radius
        // here would make empty space around long/concave modules clickable.
        item.hitRadiusPx = 0.0;
        item.pickPriority = 20;
        // The real module mesh is already visible underneath; overlay draws
        // only the active-selection ring and information card.
        item.drawGlyph = false;
        item.pointerInteractive = false;
        presentation.frame.objectOverlay.items.push_back(std::move(item));
    }

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid ||
            object.objectClass != world::celestial::DetailObjectClass::Ship)
        {
            continue;
        }

        MapObjectOverlayItem item;
        item.objectId = object.stableId.empty()
            ? "entity:" + std::to_string(object.id.value)
            : object.stableId;
        item.shipInstanceId = object.shipInstanceId;
        item.trackingSystemId = presentation.systemId;
        item.name = object.name;
        item.typeName = localObjectTypeName(object);
        item.kind = MapObjectGlyphKind::Ship;
        item.navigationHubId = snapshot.hubId;
        item.navigationHubParentBodyId = snapshot.parentBodyId;
        item.trackingWorldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubLocalToWorldMeters(snapshot, object.positionMeters)
            );
        item.hasTrackingWorldPosition = true;
        item.factionColor = localObjectColor(object);
        item.screenPx = presentation.camera.project(object.positionMeters);
        item.visible = visibleInViewport(item.screenPx, viewport);
        item.physicalSizeMeters = localObjectPhysicalSizeMeters(object);
        item.glyphScale = mapObjectGlyphScale(
            item.physicalSizeMeters,
            finalScale
        );
        item.hitRadiusPx = 15.0 * item.glyphScale;
        item.facingScreenDirection = projectDirection(
            presentation.camera,
            object.positionMeters,
            -object.axes.z,
            std::max(item.physicalSizeMeters, 100.0)
        );
        item.velocityMode = MapObjectVelocityMode::Local;
        item.arrowVelocityMode = MapObjectVelocityMode::Local;
        item.displayedVelocityMps = object.hasRelativeVelocity
            ? object.relativeVelocityMps
            : object.velocityMps;
        item.velocityArrowMps = item.displayedVelocityMps;
        // Keep card bearing/elevation tied to the displayed local motion for
        // tactical Hub scenes; otherwise all orbiting ships inherit nearly the
        // same stellar azimuth from the shared hub/orbit drift.
        item.stellarVelocityMps = object.hasRelativeVelocity
            ? object.relativeVelocityWorldMps
            : (object.hasGlobalVelocity
                ? object.globalVelocityMps
                : object.velocityMps);
        item.velocityScreenDirection = projectDirection(
            presentation.camera,
            object.positionMeters,
            object.velocityMps,
            std::max(item.physicalSizeMeters, 100.0)
        );
        presentation.frame.objectOverlay.items.push_back(std::move(item));
    }

    const auto addOrbitPivotPickable =
        [&](const world::celestial::LocalSceneObject& object)
        {
            HubMapPickable pickable;
            pickable.localCenterMeters = object.positionMeters;
            pickable.screenCenterPx =
                presentation.camera.project(
                    object.positionMeters
                );

            if (object.objectClass ==
                world::celestial::DetailObjectClass::Ship)
            {
                const glm::dvec3 visualSize = visualSizeForHubShip(
                    object,
                    presentation.scale,
                    presentation.camera.state.zoom
                );
                pickable.screenRadiusPx = std::max(
                    object.player ? 22.0 : 18.0,
                    glm::length(visualSize) * 0.5 * finalScale
                );
                pickable.priority = object.player ? 100 : 50;
                pickable.label = object.player ? m_playerLabel : object.name;
            }
            else
            {
                // Infrastructure remains selected by exact assembly-mesh hits;
                // this radius is only an orbit-pivot affordance. It lets a
                // drag near a station module orbit around that module instead
                // of silently falling back to the remote Hub origin.
                pickable.screenRadiusPx = std::max(
                    18.0,
                    std::max(1.0, object.boundingRadiusMeters) * finalScale
                );
                pickable.priority = 20;
                pickable.label = object.name;
            }

            presentation.frame.pickables.push_back(
                std::move(pickable)
            );
        };

    // Orbit pivot candidates are deliberately separate from card/selection
    // hit testing. Information windows never participate. Infrastructure uses
    // a broad proximity affordance here while actual selection remains exact
    // mesh-body based in SystemMapRenderer.
    for (const auto& object : snapshot.scene.objects)
    {
        if (object.valid &&
            (object.objectClass ==
                world::celestial::DetailObjectClass::Ship ||
             object.objectClass ==
                world::celestial::DetailObjectClass::Hub))
        {
            addOrbitPivotPickable(object);
        }
    }

    return presentation;
}

} // namespace game::system_map
