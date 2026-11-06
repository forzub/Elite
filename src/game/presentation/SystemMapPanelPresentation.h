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
    OpenHub,
    SelectObject    // System / Detail: a hub or beacon picked from the object list
};

struct SystemMapPanelAction
{
    SystemMapPanelActionType type = SystemMapPanelActionType::OpenSystem;
    int systemId = -1;
    std::string objectId;
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
    Hub,
    SelectObject
};

struct SystemMapPanelCommand
{
    SystemMapPanelCommandType type = SystemMapPanelCommandType::None;
    int systemId = -1;
    std::string objectId;
};

SystemMapPanelCommand resolveSystemMapPanelAction(
    const SystemMapPanelAction& action,
    game::system_map::MapMode mode
);

// One line of the System Map information block (right panel).
struct SystemMapInfoLine
{
    // Subheading: a small group title (a hub block); Module: one hub module
    // row - pictogram `icon` (module type), name `label`, `value` = "×N" and
    // its build status in brackets.
    enum class Kind { Heading, Field, Paragraph, Bullet, Subheading, Module };
    Kind kind = Kind::Field;
    std::string label;
    std::string value;
    std::string icon;
    int status = 0;            // Module: 0 operating, 1 under construction, 2 planned
};

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

// System / Detail map: one row of the hub and beacon list (replaces the system list there).
// A heading row groups the objects of one planet / the belt / the beacons and is not selectable.
struct SystemMapPanelObjectItem
{
    std::string objectId;      // overlay object id of the marker (replicated hub id or "site:<id>")
    std::string label;
    bool heading = false;
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
    std::vector<SystemMapPanelObjectItem> objects;

    // System / Detail map: "what is here, what happens, how to get there".
    std::string infoTitle;
    std::vector<SystemMapInfoLine> info;
};

SystemMapPanelPresentation buildSystemMapPanelPresentation(
    const SystemMapPanelPresentationInput& input
);

SystemMapPanelNavigationActions buildSystemMapPanelNavigationActions(
    const SystemMapPanelPresentation& panel
);
}
