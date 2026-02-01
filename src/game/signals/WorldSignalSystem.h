#pragma once

#include <vector>

#include "game/ship/Ship.h"
#include "world/WorldSignal.h"

struct WorldSignalSystem
{
    static void collectFromShips(
        const std::vector<Ship*>& ships,
        std::vector<WorldSignal>& outSignals
    );

    static void updateSignalPattern(WorldSignal& sig, float dt);
        
};

