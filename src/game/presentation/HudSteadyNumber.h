#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace game::presentation
{
/*
    Steady HUD numbers (author 2026-11-12: "the figures sometimes twitch and shake the whole line - zero useful
    information, only noise"). A shown value changes only when the new one leaves a band around it (hysteresis:
    relative share of the shown value, but at least an absolute floor), and it is rounded to a few significant
    digits, so the text width stays put while the value is steady and the digits still run while it really changes.
*/
inline double hudSteady(const std::string& key, double value, double relativeBand, double absoluteBand)
{
    static std::unordered_map<std::string, double> shown;
    auto [it, inserted] = shown.try_emplace(key, value);
    if (!inserted)
    {
        const double band = std::max(absoluteBand, std::abs(it->second) * relativeBand);
        if (std::abs(value - it->second) > band || (value == 0.0) != (it->second == 0.0))
            it->second = value;
    }
    return it->second;
}

// `value` rounded to `digits` significant digits (integers below 10^digits stay exact)
inline double hudSignificant(double value, int digits)
{
    const double a = std::abs(value);
    if (a < 1.0)
        return value;
    const int magnitude = static_cast<int>(std::floor(std::log10(a))) + 1;
    if (magnitude <= digits)
        return value;
    const double step = std::pow(10.0, magnitude - digits);
    return std::round(value / step) * step;
}
}
