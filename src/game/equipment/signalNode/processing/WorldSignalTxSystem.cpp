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

    for (const Ship* ship : ships)
    {
        const auto& tx = ship->equipment.transmitter;

        if (!tx.isOperational())
            continue;

        const WorldSignal& sig = ship->emittedSignal;
        if (!sig.enabled)
            continue;

        // if (!ship->getEmittedSignal().enabled)
        //     continue;


        // WorldSignal sig;
        // sig = ship->getEmittedSignal();
        // sig.displayClass = tx.displayClass;
        // sig.position     = ship->transform.position;
        // sig.power        = tx.txPower;
        // sig.maxRange     = tx.baseRange;
        // sig.enabled      = true;
        // sig.label        = ship->desc->name;
        // sig.owner        = ship;

        outSignals.push_back(sig);
    }
}







