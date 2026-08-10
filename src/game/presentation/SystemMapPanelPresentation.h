#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "src/game/system_map/MapMode.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::presentation
{
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

    bool canOpenDetail = false;
    std::optional<world::celestial::DetailSpatialCell> selectedDetailCell;
    bool canOpenHub = false;
};

nlohmann::json buildSystemMapPanelPayload(
    const SystemMapPanelPresentationInput& input
);
}
