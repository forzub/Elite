#pragma once

#include <vector>


struct NpcSignalAwareness
{
    struct NpcPerceivedSignal
    {
        const void* source;   // пока opaque, потом можно типизировать
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
                r.source,
                r.signalToNoiseRatio,
                r.semanticState == SignalSemanticState::Decoded
            });
        }
    }
};
