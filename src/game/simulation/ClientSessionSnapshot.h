#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/world/celestial/CelestialTypes.h"
#include "src/game/navigation/OwnedNavigationAsset.h"
#include "src/world/WorldParams.h"

namespace game::simulation
{

struct ClientSessionSnapshot
{
    world::celestial::PlayerNavigationState playerNavigation;
    std::vector<game::navigation::OwnedNavigationAsset> ownedNavigationAssets;
    WorldParams predictionWorldParams {0.0f, 50.0f};

    double universeTimeSeconds = 0.0;
    double universeTimeScale = 1.0;
    std::uint64_t universeTimelineRevision = 1;
    double configuredUniverseTimeScale = 10000.0;

    bool universeTimeSimulation = false;
    std::string universeDate;
};

} // namespace game::simulation
