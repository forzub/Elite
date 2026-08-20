#include "src/game/presentation/SystemMapPanelPresentation.h"

#include <glm/geometric.hpp>
#include <utility>

#include "src/game/presentation/GalaxyNavigationPresentation.h"

namespace game::presentation
{
SystemMapPanelCommand resolveSystemMapPanelAction(
    const SystemMapPanelAction& action,
    game::system_map::MapMode mode)
{
    using game::system_map::MapMode;

    switch (action.type)
    {
        case SystemMapPanelActionType::SelectSystem:
            return {SystemMapPanelCommandType::SelectSystem, action.systemId};

        case SystemMapPanelActionType::OpenGalaxy:
            return mode == MapMode::Galaxy
                ? SystemMapPanelCommand{}
                : SystemMapPanelCommand{SystemMapPanelCommandType::Galaxy, -1};

        case SystemMapPanelActionType::OpenSystem:
            if (mode == MapMode::Galaxy)
            {
                return {
                    SystemMapPanelCommandType::OpenSelectedGalaxyTarget,
                    -1
                };
            }
            if (mode == MapMode::Detail || mode == MapMode::Hub)
            {
                return {SystemMapPanelCommandType::LoadedSystem, -1};
            }
            return {};

        case SystemMapPanelActionType::OpenDetail:
            if (mode == MapMode::System)
                return {SystemMapPanelCommandType::SelectedDetail, -1};
            if (mode == MapMode::Hub)
                return {SystemMapPanelCommandType::LoadedDetail, -1};
            return {};

        case SystemMapPanelActionType::OpenHub:
            if (mode == MapMode::System || mode == MapMode::Detail)
                return {SystemMapPanelCommandType::Hub, -1};
            return {};
    }

    return {};
}

SystemMapPanelNavigationActions buildSystemMapPanelNavigationActions(
    const SystemMapPanelPresentation& panel)
{
    using game::system_map::MapMode;

    switch (panel.mode)
    {
        case MapMode::Galaxy:
            return {{{SystemMapPanelActionType::OpenSystem, true},
                     {SystemMapPanelActionType::OpenDetail, false},
                     {SystemMapPanelActionType::OpenHub, false}}};

        case MapMode::System:
            return {{{SystemMapPanelActionType::OpenGalaxy, true},
                     {SystemMapPanelActionType::OpenDetail, panel.canOpenDetail},
                     {SystemMapPanelActionType::OpenHub, panel.canOpenHub}}};

        case MapMode::Detail:
            return {{{SystemMapPanelActionType::OpenSystem, true},
                     {SystemMapPanelActionType::OpenGalaxy, true},
                     {SystemMapPanelActionType::OpenHub, panel.canOpenHub}}};

        case MapMode::Hub:
            return {{{SystemMapPanelActionType::OpenDetail, true},
                     {SystemMapPanelActionType::OpenSystem, true},
                     {SystemMapPanelActionType::OpenGalaxy, true}}};
    }

    return {};
}

SystemMapPanelPresentation buildSystemMapPanelPresentation(
    const SystemMapPanelPresentationInput& input)
{
    SystemMapPanelPresentation panel;
    panel.universeTimeSeconds = input.universeTimeSeconds;
    panel.universeDate = input.universeDate;
    panel.universeTimeScale = input.universeTimeScale;
    panel.mode = input.mode;
    panel.currentSystemId = input.navigation
        ? input.navigation->currentSystemId
        : -1;
    panel.currentSystemName = input.currentSystemName;
    panel.selectedSystemId = input.selectedSystemId;
    panel.selectedEmptySector = input.selectedEmptySector;
    panel.selectedBodyId = input.selectedBodyId;
    panel.selectedHubId = input.selectedHubId;
    panel.systemLayerIsSpace = input.systemLayerIsSpace;
    panel.canOpenDetail = input.canOpenDetail;
    panel.selectedDetailCell = input.selectedDetailCell;
    panel.canOpenHub = input.canOpenHub;

    if (input.selectedEmptySector && input.system)
        panel.selectedEmptySectorPositionLy = input.system->systemPositionLy;

    if (!input.galaxy)
        return panel;

    panel.systemsCount = input.galaxy->systems.size();
    panel.systems.reserve(input.galaxy->systems.size());

    const auto playerMarker = input.navigation
        ? resolveGalaxyPlayerMarkerPosition(*input.galaxy, *input.navigation)
        : GalaxyPlayerMarkerPosition{};

    for (const auto& candidate : input.galaxy->systems)
    {
        SystemMapPanelSystemItem item;
        item.id = candidate.id;
        item.name = candidate.name;
        item.starType = candidate.starType;
        item.jurisdiction = candidate.jurisdiction.empty()
            ? "Unregistered"
            : candidate.jurisdiction;
        item.current = input.navigation &&
            candidate.id == input.navigation->currentSystemId;
        item.selected = candidate.id == input.selectedSystemId;

        if (input.navigation)
        {
            const glm::dvec3 delta =
                candidate.positionLy - playerMarker.positionLy;
            item.distanceFromPlayerLy = glm::length(delta);
        }

        panel.systems.push_back(std::move(item));
    }

    return panel;
}
}
