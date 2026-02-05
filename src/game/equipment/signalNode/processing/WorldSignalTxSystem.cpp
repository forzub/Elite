#include "WorldSignalTxSystem.h"
#include <iostream>
#include <cmath>

#include "game/signals/SignalPatternLibrary.h"

void WorldSignalTxSystem::collectFromShips(
    const std::vector<Ship*>& ships,
    std::vector<WorldSignal>& outSignals
)
{
    outSignals.clear();

        // printf(
        //     "[TX] collectFromShips: ships=%zu\n",
        //     ships.size()
        // );

    for (const Ship* ship : ships)
    {
        const auto& tx = ship->equipment.transmitter;

        if (!tx.isOperational())
            continue;

        const WorldSignal& sig = ship->emittedSignal;


        // printf(
        //     "[TX] ship=%p enabled=%d type=%d power=%.2f range=%.2f owner=%p label=%s\n",
        //     (void*)ship,
        //     sig.enabled,
        //     (int)sig.type,
        //     sig.power,
        //     sig.maxRange,
        //     (void*)sig.owner,
        //     sig.label.c_str()
        // );



        if (sig.enabled)
        {
            outSignals.push_back(sig);
                // printf("[TX]   -> pushed\n");
        }
        else
        {
                // printf("[TX]   -> skipped (disabled)\n");
        }

    }

    // printf(
    //     "[TX] collectFromShips done: outSignals=%zu\n",
    //     outSignals.size()
    // );
}







