#pragma once

#include "world/types/SignalType.h"
// #include "src/game/ship/ShipSignalProfile.h"

#include <unordered_map>
#include <vector>

struct SignalPattern;

struct ShipSignalProfile
{
    // какие сигналы вообще доступны кораблю
    std::vector<SignalType> availableSignals;

    // опциональные переопределения паттернов
    std::unordered_map<SignalType, const SignalPattern*> customPatterns;

    const SignalPattern* getPattern(SignalType type) const;
};
