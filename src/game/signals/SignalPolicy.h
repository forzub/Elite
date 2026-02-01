#pragma once

#include "world/WorldSignal.h"

inline bool signalAllowsDistance(const WorldSignal& sig, float dist)
{
    switch (sig.type)
    {
        case SignalType::Planets:
        case SignalType::StationClass:
        case SignalType::Beacon:
            return true;

        case SignalType::Transponder:
        case SignalType::SOSModern:
            return dist < sig.maxRange * 0.6f;

        case SignalType::SOSAntic:
        case SignalType::Unknown:
            return false;
    }
    return false;
}
