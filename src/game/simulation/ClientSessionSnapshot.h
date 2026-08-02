#pragma once

#include <string>

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/WorldParams.h"

namespace game::simulation
{

struct ClientSessionSnapshot
{
    world::celestial::PlayerNavigationState playerNavigation;
    WorldParams predictionWorldParams {0.0f, 50.0f};

    double universeTimeSeconds = 0.0;
    double universeTimeScale = 1.0;
    double configuredUniverseTimeScale = 10000.0;

    bool universeTimeSimulation = false;
    std::string universeDate;
};

} // namespace game::simulation
