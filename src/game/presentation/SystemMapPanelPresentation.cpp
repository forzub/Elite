#include "src/game/presentation/SystemMapPanelPresentation.h"

#include <cmath>

#include "src/game/presentation/GalaxyNavigationPresentation.h"

namespace game::presentation
{
namespace
{
const char* modeName(game::system_map::MapMode mode)
{
    using game::system_map::MapMode;

    switch (mode)
    {
        case MapMode::Galaxy: return "Galaxy";
        case MapMode::System: return "System";
        case MapMode::Detail: return "Detail";
        case MapMode::Hub: return "Hub";
    }

    return "Galaxy";
}
}

nlohmann::json buildSystemMapPanelPayload(
    const SystemMapPanelPresentationInput& input
)
{
    using nlohmann::json;

    json payload;
    payload["universeTimeSeconds"] = input.universeTimeSeconds;
    payload["universeDate"] = input.universeDate;
    payload["universeTimeScale"] = input.universeTimeScale;
    payload["mode"] = modeName(input.mode);

    const auto* galaxy = input.galaxy;
    const auto* system = input.system;
    const auto* nav = input.navigation;

    payload["systemsCount"] = galaxy ? galaxy->systems.size() : 0;
    payload["currentSystemId"] = nav ? nav->currentSystemId : -1;
    payload["currentSystemName"] = input.currentSystemName;
    payload["selectedSystemId"] = input.selectedSystemId;
    payload["selectedEmptySector"] = input.selectedEmptySector;

    if (input.selectedEmptySector && system)
    {
        payload["selectedEmptySectorPositionLy"] = {
            {"x", system->systemPositionLy.x},
            {"y", system->systemPositionLy.y},
            {"z", system->systemPositionLy.z}
        };
    }

    payload["systems"] = json::array();
    payload["selectedBodyId"] = input.selectedBodyId;
    payload["selectedHubId"] = input.selectedHubId;
    payload["canOpenDetail"] = input.canOpenDetail;
    payload["canOpenHub"] = input.canOpenHub;

    if (input.selectedDetailCell)
    {
        const auto& cell = *input.selectedDetailCell;
        payload["selectedDetailCell"] = {
            {"level", cell.level},
            {"maximumLevel", cell.maximumLevel},
            {"x", cell.x},
            {"y", cell.y},
            {"z", cell.z},
            {"edgeAu", cell.edgeAu}
        };
    }

    if (!galaxy)
        return payload;

    const auto playerMarker =
        nav
            ? resolveGalaxyPlayerMarkerPosition(*galaxy, *nav)
            : GalaxyPlayerMarkerPosition{};

    for (const auto& candidate : galaxy->systems)
    {
        double distanceFromPlayerLy = 0.0;
        if (nav)
        {
            const glm::dvec3 delta =
                candidate.positionLy - playerMarker.positionLy;
            distanceFromPlayerLy = glm::length(delta);
        }

        json item;
        item["id"] = candidate.id;
        item["name"] = candidate.name;
        item["starType"] = candidate.starType;
        item["starsCount"] = candidate.starsCount;
        item["xLy"] = candidate.positionLy.x;
        item["yLy"] = candidate.positionLy.y;
        item["zLy"] = candidate.positionLy.z;
        item["current"] = nav && candidate.id == nav->currentSystemId;
        item["selected"] = candidate.id == input.selectedSystemId;
        item["distanceFromPlayerLy"] = distanceFromPlayerLy;
        item["jurisdiction"] =
            candidate.jurisdiction.empty()
                ? "Unregistered"
                : candidate.jurisdiction;

        payload["systems"].push_back(std::move(item));
    }

    return payload;
}
}
