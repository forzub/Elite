#include "WorldSignalTxSystem.h"
#include <iostream>
#include <cmath>



void WorldSignalTxSystem::collectFromSources(
    const std::vector<ITransmitterSource*>& sources,
    std::vector<WorldSignal>& outSignals
)
{
    outSignals.clear();

    for (const ITransmitterSource* src : sources)
    {
        auto signal = src->emitSignal();
        if (signal)
            outSignals.push_back(*signal);
    }
}







