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
// what the client KNOWS of a city pad now (audit #6 A6): the map / HUD show these four apart
enum class ClientPadState
{
    Usable,     // built, intact or lightly hit
    NotBuilt,   // its building is still going up (not in the package)
    Wrecked,    // gutted / destroyed
    Unknown     // the package is still assembling or its assembly failed: the plan stands in, the server decides
};

inline ClientPadState clientCityPadState(const ClientObjectState& city, const std::string& address)
{
    if (game::scene::clientLayoutCityPackageState(city.type, city.hubAttachment.hubId) != game::scene::ClientCityPackageState::Ready)
        return ClientPadState::Unknown;
    const auto* nodes = game::scene::clientLayoutCityPadNodes(city.type, city.hubAttachment.hubId);
    if (!nodes)
        return ClientPadState::Unknown;
    switch (game::simulation::infrastructureStateOf(game::simulation::cityPadPresence(*nodes, city.buildingStates, address)))
    {
        case game::simulation::InfrastructureState::Existing:
        case game::simulation::InfrastructureState::Damaged: return ClientPadState::Usable;
        case game::simulation::InfrastructureState::Destroyed: return ClientPadState::Wrecked;
        case game::simulation::InfrastructureState::NotBuilt: return ClientPadState::NotBuilt;
        default: return ClientPadState::Unknown;
    }
}

// usable or not known yet (the plan stands in while the package assembles; the server's dispatcher has the last word)
inline bool clientCityPadUsable(const ClientObjectState& city, const std::string& address)
{
    const auto s = clientCityPadState(city, address);
    return s == ClientPadState::Usable || s == ClientPadState::Unknown;
}
} // namespace game::client
