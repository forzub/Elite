#pragma once

// The client's view of a city pad's ACTUAL existence (spatial audit A6): the same rule as the server's
// GameSimulation::cityPadPresenceOf (CityPadPresence.h) - the pads the client's copy of the city package has, and the
// replicated damage states (ObjectSnapshot::buildingStates). While the package is still being assembled the plan stands in
// (presence unknown = usable, as before), so nothing flickers off during loading.
#include <string>

#include "src/game/client/ClientWorldState.h"
#include "src/game/scene/LayoutCityHitVolumes.h"
#include "src/game/simulation/CityPadPresence.h"

namespace game::client
{
inline bool clientCityPadUsable(const ClientObjectState& city, const std::string& address)
{
    const auto* nodes = game::scene::clientLayoutCityPadNodes(city.type, city.hubAttachment.hubId);
    if (!nodes || nodes->empty())
        return true;   // not assembled yet (or a city without a package): the plan stands in
    return game::simulation::cityPadPresence(*nodes, city.buildingStates, address).usable();
}
} // namespace game::client
