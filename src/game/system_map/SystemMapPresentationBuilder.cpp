#include "src/game/system_map/SystemMapPresentationBuilder.h"

#include <algorithm>
#include <unordered_set>
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
        const Viewport& viewport,
        int systemId
    )
    {
        auto& state = view.state();

        if (state.lastCameraFitSystemId == systemId)
            return;

        // A newly opened system: the S0 root as the current parent, on the
        // whole screen (the same frame model as the Galaxy Map).
        view.resetHierarchyToRoot(viewport);
        state.navigationGrid.selectCell(state.navigationGrid.cell({}, 0));
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

namespace
{
    /*
        Presentation levels: the physical system keeps every body; the map
        shows major planets, stars and significant belts at every zoom and
        brings minor planets, dwarf planets and minor belts in as their
        orbits grow on screen. Moons of a hidden body are hidden with it; the
        selected body always stays.
    */
    constexpr double MinorPlanetOrbitPx = 150.0;
    constexpr double DwarfPlanetOrbitPx = 300.0;
    constexpr double MinorReservoirOrbitPx = 150.0;

    void filterBodiesByPresentationLevel(
        SystemMapView& view,
        const Viewport& viewport,
        SystemMapPresentation& presentation
    )
    {
        const double worldUnitsPerPixel =
            view.cameraSnapshot(viewport).worldUnitsPerPixel;
        if (worldUnitsPerPixel <= 0.0 || presentation.systemScale <= 0.0f)
            return;
        const double pixelsPerAu =
            static_cast<double>(presentation.systemScale) / worldUnitsPerPixel;
        const std::string& selected = view.state().selectedBodyId;

        std::unordered_set<std::string> hidden;
        for (const auto& body : presentation.bodies)
        {
            // Nothing outside the current parent cube is shown or a target.
            if (!view.hierarchyContains(body.positionAu))
            {
                hidden.insert(body.id);
                continue;
            }
            const double orbitPx = body.orbitRadiusAu * pixelsPerAu;
            const std::string& level = body.presentationLevel;
            const bool show =
                body.id == selected ||
                (level == "minor_planet" ? orbitPx >= MinorPlanetOrbitPx
                 : level == "dwarf_planet" ? orbitPx >= DwarfPlanetOrbitPx
                 : level == "minor_reservoir" ? orbitPx >= MinorReservoirOrbitPx
                 : true);
            if (!show)
                hidden.insert(body.id);
        }
        // Children (moons, rings' parents' satellites) follow their parent.
        for (bool changed = true; changed;)
        {
            changed = false;
            for (const auto& body : presentation.bodies)
                if (!hidden.count(body.id) && body.id != selected && hidden.count(body.parentId))
                {
                    hidden.insert(body.id);
                    changed = true;
                }
        }
        if (hidden.empty())
            return;
        presentation.bodies.erase(
            std::remove_if(
                presentation.bodies.begin(),
                presentation.bodies.end(),
                [&](const auto& body) { return hidden.count(body.id) != 0; }
            ),
            presentation.bodies.end()
        );
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

    fitCameraOnce(view, viewport, system.systemId);

    filterBodiesByPresentationLevel(view, viewport, presentation);

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
