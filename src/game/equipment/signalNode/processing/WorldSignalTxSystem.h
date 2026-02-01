#pragma once

#include <vector>

#include "game/ship/Ship.h"
#include "world/WorldSignal.h"

struct WorldSignalTxSystem
{
    static void collectFromShips(
        const std::vector<Ship*>& ships,
        std::vector<WorldSignal>& outSignals
    );


        
};

