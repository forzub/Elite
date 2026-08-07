#include "src/game/system_map/SystemMapPresentationBuilder.h"

#include <algorithm>
#include <string>

#include "src/game/system_map/SystemMapView.h"
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
    presentation.timeSeconds = system.universeTimeSeconds;
    presentation.bodies = system.bodies;
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
