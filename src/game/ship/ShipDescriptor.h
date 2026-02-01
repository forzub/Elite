// ShipDescriptor - паспорт - что это за корабль
// не меняется во время игры
// может грузиться из C++ / JSON позже
// один дескриптор → много экземпляров

#pragma once

#include <string>
#include "game/ship/ShipParams.h"
#include "game/ship/ShipHudProfile.h"

#include "game/ship/equipment/ReceiverModule.h"
#include "game/ship/equipment/SignalEmitterModule.h"
#include "game/ship/equipment/JammerModule.h"


struct ShipDescriptor
{      

    std::string         name;               // "Cobra Mk III"

    ShipParams          physics;     // лётные характеристики
    // ShipSignalProfile  signals;
    // ShipCombatProfile  combat;
    // ShipCargoProfile   cargo;
    ShipHudProfile          hud;         // HUD / кабина
    ReceiverModule          receiver;
    SignalEmitterModule     transmitter;
    JammerModule            jammer;
};
