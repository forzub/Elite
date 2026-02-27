#pragma once

#include <vector>

#include "game/ship/Ship.h"
#include "world/WorldSignal.h"
#include "src/world/ITransmitterSource.h"

struct WorldSignalTxSystem
{
    static void collectFromSources(
        const std::vector<ITransmitterSource*>& sources,
        std::vector<WorldSignal>& outSignals
        );

        
};

