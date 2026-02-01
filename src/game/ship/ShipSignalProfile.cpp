#include "ShipSignalProfile.h"
#include "src/game/signals/SignalPatternLibrary.h"
#include "src/game/ship/ShipSignalProfile.h"

const SignalPattern* ShipSignalProfile::getPattern(SignalType type) const
{
    auto it = customPatterns.find(type);
    if (it != customPatterns.end())
        return it->second;

    // return SignalPatternLibrary::instance().get(type);
    return nullptr;
}
