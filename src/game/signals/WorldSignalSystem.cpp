#include "WorldSignalSystem.h"
#include <iostream>
#include <cmath>

#include "game/signals/SignalPatternLibrary.h"

void WorldSignalSystem::collectFromShips(
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

        if (!tx.hasValidMode())
            continue;

        const auto& mode = tx.currentMode();

        WorldSignal sig;
        sig.type         = mode.type;
        sig.displayClass = tx.displayClass;
        sig.position     = ship->transform.position;
        sig.power        = tx.txPower;
        sig.maxRange     = tx.baseRange;
        sig.enabled      = true;
        sig.label        = ship->desc->name;
        sig.owner        = ship;

        outSignals.push_back(sig);
    }
}








void WorldSignalSystem::updateSignalPattern(WorldSignal& sig, float dt)
{
    if (!sig.pattern || sig.pattern->phases.empty())
    {
        sig.enabled = true;
        return;
    }

    sig.patternTime += dt;

    const float total = sig.pattern->totalDuration();
    float t = sig.patternTime;

    if (sig.pattern->loop && total > 0.0f)
        t = std::fmod(t, total);

    float acc = 0.0f;
    for (const auto& phase : sig.pattern->phases)
    {
        acc += phase.duration;
        if (t <= acc)
        {
            sig.enabled = phase.active;
            return;
        }
    }

    sig.enabled = false;
}

