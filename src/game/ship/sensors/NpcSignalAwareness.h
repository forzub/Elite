#pragma once

#include <vector>

#include "src/scene/EntityID.h"

struct NpcSignalAwareness
{
    struct NpcPerceivedSignal
    {
        EntityId sourceOwner;
        float snr;
        bool decoded;
    };

    std::vector<NpcPerceivedSignal> signals;

    template <typename SignalResultT>
    void update(const std::vector<SignalResultT>& results)
    {
        signals.clear();

        for (const auto& r : results)
        {
            if (r.semanticState == SignalSemanticState::None)
                continue;

            signals.push_back({
                r.owner,
                r.signalToNoiseRatio,
                r.semanticState == SignalSemanticState::Decoded
            });
        }
    }
};
