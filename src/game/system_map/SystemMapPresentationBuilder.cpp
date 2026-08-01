#include "src/game/system_map/SystemMapPresentationBuilder.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "src/game/system_map/SystemMapView.h"
#include "src/world/celestial/CelestialOrbitKinematics.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
namespace
{
    void synchronizeSystemState(
        SystemMapView& view,
        int systemId
    )
    {
        auto& state = view.state();

        if (state.navigationGrid.systemId() == systemId)
            return;

        state.navigationGrid.activateSystem(systemId);
        state.hoverVisualCell.reset();
        state.hoverVisualAlpha = 0.0f;
        state.hoverOutgoingCell.reset();
        state.hoverOutgoingAlpha = 0.0f;
        state.hoverVisualLastTimeSeconds = 0.0;
        state.cubeClickTracker.reset();
        state.navigationCellExplicitlySelected = false;
        state.selectedBodyId.clear();
        state.selectedHubId.clear();
        state.selectedHubParentBodyId.clear();
    }

    double resolvePresentationTimeSeconds(
        SystemMapView& view,
        const world::celestial::SystemMapSnapshot& system,
        double wallNowSeconds
    )
    {
        auto& state = view.state();

        const bool sourceChanged =
            state.presentationSystemId != system.systemId ||
            std::abs(
                state.presentationSourceTimeSeconds -
                    system.universeTimeSeconds
            ) > 0.000001 ||
            std::abs(
                state.presentationTimeScale -
                    system.universeTimeScale
            ) > 0.000001;

        if (sourceChanged)
        {
            state.presentationSystemId = system.systemId;
            state.presentationSourceTimeSeconds =
                system.universeTimeSeconds;
            state.presentationWallTimeSeconds = wallNowSeconds;
            state.presentationTimeScale =
                std::max(0.0, system.universeTimeScale);
        }

        return
            state.presentationSourceTimeSeconds +
            std::max(
                0.0,
                wallNowSeconds -
                    state.presentationWallTimeSeconds
            ) *
            state.presentationTimeScale;
    }

    std::vector<world::celestial::SystemMapBody>
    buildVisualBodies(
        const world::celestial::SystemMapSnapshot& system,
        double presentationTimeSeconds
    )
    {
        std::vector<world::celestial::SystemMapBody> visualBodies =
            system.bodies;

        std::unordered_map<std::string, glm::dvec3>
            visualBodyPositionAuById;

        for (auto& body : visualBodies)
        {
            glm::dvec3 visualOrbitCenter = body.orbitCenterAu;

            const auto parentIt =
                visualBodyPositionAuById.find(body.parentId);

            if (parentIt != visualBodyPositionAuById.end())
                visualOrbitCenter = parentIt->second;

            if (body.drawOrbit &&
                body.orbitRadiusAu > 0.0 &&
                body.orbitalPeriodDays > 0.0)
            {
                const double phaseRad =
                    world::celestial::circularOrbitPhaseRad(
                        presentationTimeSeconds,
                        body.orbitalPeriodDays,
                        body.orbitalDirection,
                        body.orbitalPhaseOffsetRad
                    );

                body.positionAu =
                    visualOrbitCenter +
                    world::celestial::circularOrbitPositionAu(
                        body.orbitRadiusAu,
                        phaseRad
                    );
            }
            else if (parentIt != visualBodyPositionAuById.end())
            {
                body.positionAu =
                    visualOrbitCenter +
                    (body.positionAu - body.orbitCenterAu);
            }

            body.orbitCenterAu = visualOrbitCenter;

            if (body.dayLengthHours > 0.0)
            {
                const double snapshotRotationOffset =
                    body.rotationPhaseRad -
                    static_cast<double>(
                        body.rotationDirection < 0 ? -1 : 1
                    ) *
                    std::fmod(
                        system.universeTimeSeconds /
                            (body.dayLengthHours * 3600.0),
                        1.0
                    ) *
                    world::celestial::OrbitTwoPi;

                body.rotationPhaseRad =
                    world::celestial::bodyRotationPhaseRad(
                        presentationTimeSeconds,
                        body.dayLengthHours,
                        body.rotationDirection,
                        snapshotRotationOffset
                    );
            }

            visualBodyPositionAuById[body.id] = body.positionAu;
        }

        return visualBodies;
    }

    float calculateSystemScale(
        const SystemMapView& view,
        const std::vector<world::celestial::SystemMapBody>& bodies
    )
    {
        double maxAu =
            bodies.empty()
                ? std::max(
                    1.0,
                    view.state().navigationGrid.cellSize(
                        view.state().navigationGrid
                            .definition()
                            .minimumLevel
                    ) * 0.5
                )
                : 1.0;

        for (const auto& body : bodies)
        {
            const double positionRadius =
                glm::length(body.positionAu);

            maxAu = std::max(maxAu, positionRadius);

            if (body.drawOrbit && body.orbitRadiusAu > 0.0)
            {
                maxAu =
                    std::max(
                        maxAu,
                        glm::length(body.orbitCenterAu) +
                            body.orbitRadiusAu
                    );
            }
        }

        return
            view.controls().fittedSystemRadiusWorld /
            static_cast<float>(maxAu);
    }

    void fitCameraOnce(
        SystemMapView& view,
        int systemId
    )
    {
        auto& state = view.state();

        if (state.lastCameraFitSystemId == systemId)
            return;

        view.cancelCameraFlight(false);

        state.camera.target = glm::dvec3(0.0);
        state.camera.distance =
            std::clamp(
                view.controls().fittedSystemRadiusWorld *
                    view.controls().initialFitPadding,
                SystemMapView::minimumCameraHalfHeight,
                SystemMapView::maximumCameraHalfHeight
            );

        state.navigationGrid.setAnchorFromPosition(glm::dvec3(0.0));
        state.navigationGrid.selectCell(
            state.navigationGrid.anchorCell()
        );
        state.navigationCellExplicitlySelected = false;
        state.lastCameraFitSystemId = systemId;
    }

    void removeStaleSelections(
        SystemMapView& view,
        const SystemMapPresentation& presentation,
        const world::celestial::SystemMapSnapshot& system
    )
    {
        using world::celestial::BodyType;

        auto& state = view.state();

        if (!state.selectedBodyId.empty())
        {
            const auto selectedBody =
                std::find_if(
                    presentation.bodies.begin(),
                    presentation.bodies.end(),
                    [&](const auto& body)
                    {
                        return body.id == state.selectedBodyId;
                    }
                );

            if (selectedBody == presentation.bodies.end() ||
                selectedBody->type == BodyType::Star)
            {
                state.selectedBodyId.clear();
            }
        }

        if (!state.selectedHubId.empty())
        {
            const auto selectedObject =
                std::find_if(
                    system.objects.begin(),
                    system.objects.end(),
                    [&](const auto& object)
                    {
                        return
                            systemMapObjectStableKey(object) ==
                            state.selectedHubId;
                    }
                );

            if (selectedObject == system.objects.end())
            {
                state.selectedHubId.clear();
                state.selectedHubParentBodyId.clear();
            }
        }
    }
}

std::string systemMapObjectStableKey(
    const world::celestial::SystemMapObject& object
)
{
    if (!object.stableId.empty())
        return object.stableId;

    return "entity:" + std::to_string(object.id.value);
}

SystemMapPresentation SystemMapPresentationBuilder::build(
    SystemMapView& view,
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system,
    double wallNowSeconds,
    bool updateHoverPresentation
) const
{
    synchronizeSystemState(view, system.systemId);

    SystemMapPresentation presentation;
    presentation.systemId = system.systemId;
    presentation.timeSeconds =
        resolvePresentationTimeSeconds(
            view,
            system,
            wallNowSeconds
        );
    presentation.bodies =
        buildVisualBodies(
            system,
            presentation.timeSeconds
        );
    presentation.systemScale =
        calculateSystemScale(
            view,
            presentation.bodies
        );

    view.state().lastScale = presentation.systemScale;

    fitCameraOnce(view, system.systemId);

    if (updateHoverPresentation &&
        view.state().navigationGrid.enabled() &&
        presentation.systemScale > 0.0f)
    {
        view.updateNavigationHoverPresentation(
            viewport,
            wallNowSeconds
        );
    }

    removeStaleSelections(
        view,
        presentation,
        system
    );

    return presentation;
}
}
