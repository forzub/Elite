#pragma once
#include <vector>

struct SignalPulse
{
    float duration;    // секунды
    float amplitude;   // 0..1
};

struct SignalPattern
{
    std::vector<SignalPulse> pulses;
    float repeatDelay; // пауза между циклами
};
