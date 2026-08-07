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

    const auto addPickable =
        [&](
            const world::celestial::LocalSceneObject& object,
            bool ship
        )
        {
            HubMapPickable pickable;
            pickable.localCenterMeters = object.positionMeters;
            pickable.screenCenterPx =
                presentation.camera.project(
                    object.positionMeters
                );
            pickable.label = object.name;

            if (!ship)
            {
                const double moduleRadiusMeters =
                    glm::length(object.sizeMeters) * 0.5;

                pickable.screenRadiusPx =
                    std::max(
                        18.0,
                        moduleRadiusMeters * finalScale
                    );
                pickable.priority = object.prime ? 20 : 10;
            }
            else
            {
                const glm::dvec3 visualSize =
                    visualSizeForHubShip(
                        object,
                        presentation.scale,
                        presentation.camera.state.zoom
                    );

                pickable.screenRadiusPx =
                    std::max(
                        object.player ? 22.0 : 18.0,
                        glm::length(visualSize) * 0.5 * finalScale
                    );
                pickable.priority = object.player ? 100 : 50;
                pickable.label =
                    object.player ? "PLAYER" : object.name;
            }

            presentation.frame.pickables.push_back(
                std::move(pickable)
            );
        };

    // Preserve the old render-path insertion order exactly: modules first,
    // ships second. This keeps equal-score picking deterministic.
    for (const auto& object : snapshot.scene.objects)
    {
        if (object.valid &&
            object.objectClass ==
                world::celestial::DetailObjectClass::Hub)
        {
            addPickable(object, false);
        }
    }

    for (const auto& object : snapshot.scene.objects)
    {
        if (object.valid &&
            object.objectClass ==
                world::celestial::DetailObjectClass::Ship)
        {
            addPickable(object, true);
        }
    }

    return presentation;
}

} // namespace game::system_map
