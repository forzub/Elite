#pragma once

#include <vector>

struct SignalPatternPhase
{
    float duration;   // секунды
    bool  active;     // ВКЛ / ВЫКЛ
};

struct SignalPattern
{
    std::vector<SignalPatternPhase> phases;
    bool loop = true;

    float totalDuration() const
    {
        float t = 0.0f;
        for (const auto& p : phases)
            t += p.duration;
        return t;
    }
};
