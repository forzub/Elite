#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "src/game/system_map/MapMode.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::presentation
{

enum class SystemMapPanelActionType
{
    SelectSystem,
    OpenGalaxy,
    OpenSystem,
    OpenDetail,
    OpenHub
};

struct SystemMapPanelAction
{
    SystemMapPanelActionType type = SystemMapPanelActionType::OpenSystem;
    int systemId = -1;
};

enum class SystemMapPanelCommandType
{
    None,
    OpenSelectedGalaxyTarget,
    SelectSystem,
    Galaxy,
    LoadedSystem,
    LoadedDetail,
    SelectedDetail,
    Hub
};

struct SystemMapPanelCommand
{
    SystemMapPanelCommandType type = SystemMapPanelCommandType::None;
    int systemId = -1;
};

SystemMapPanelCommand resolveSystemMapPanelAction(
    const SystemMapPanelAction& action,
    game::system_map::MapMode mode
);

struct SystemMapPanelPresentationInput
{
    double universeTimeSeconds = 0.0;
    std::string universeDate;
    double universeTimeScale = 1.0;

    game::system_map::MapMode mode =
        game::system_map::MapMode::Galaxy;

    const world::celestial::GalaxyMapSnapshot* galaxy = nullptr;
    const world::celestial::SystemMapSnapshot* system = nullptr;
    const world::celestial::PlayerNavigationState* navigation = nullptr;

    std::string currentSystemName;

    bool selectedEmptySector = false;
    int selectedSystemId = -1;
    std::string selectedBodyId;
    std::string selectedHubId;

    // The System layer is a generic cubic-space layer. When it is rooted in
    // an empty Galaxy sector there is no star system, so the UI names it
    // SPACE instead of pretending that a system exists.
    bool systemLayerIsSpace = false;

    bool canOpenDetail = false;
    std::optional<world::celestial::DetailSpatialCell> selectedDetailCell;
    bool canOpenHub = false;
};

struct SystemMapPanelSystemItem
{
    int id = -1;
    std::string name;
    std::string starType;
    std::string jurisdiction;
    double distanceFromPlayerLy = 0.0;
    bool current = false;
    bool selected = false;
};

struct SystemMapPanelNavigationAction
{
    SystemMapPanelActionType action = SystemMapPanelActionType::OpenSystem;
    bool enabled = false;
};

using SystemMapPanelNavigationActions =
    std::array<SystemMapPanelNavigationAction, 3>;

struct SystemMapPanelPresentation
{
    double universeTimeSeconds = 0.0;
    std::string universeDate;
    double universeTimeScale = 1.0;
    game::system_map::MapMode mode = game::system_map::MapMode::Galaxy;

    std::size_t systemsCount = 0;
    int currentSystemId = -1;
    std::string currentSystemName;
    int selectedSystemId = -1;
    bool selectedEmptySector = false;
    glm::dvec3 selectedEmptySectorPositionLy {0.0};

    std::string selectedBodyId;
    std::string selectedHubId;
    bool systemLayerIsSpace = false;
    bool canOpenDetail = false;
    std::optional<world::celestial::DetailSpatialCell> selectedDetailCell;
    bool canOpenHub = false;

    std::vector<SystemMapPanelSystemItem> systems;
};

SystemMapPanelPresentation buildSystemMapPanelPresentation(
    const SystemMapPanelPresentationInput& input
);

SystemMapPanelNavigationActions buildSystemMapPanelNavigationActions(
    const SystemMapPanelPresentation& panel
);
}
