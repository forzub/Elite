// ShipDescriptor - паспорт - что это за корабль
// не меняется во время игры
// может грузиться из C++ / JSON позже
// один дескриптор → много экземпляров

#pragma once

#include <string>
#include "game/ship/ShipParams.h"
#include "game/ship/ShipHudProfile.h"

#include "game/equipment/signalNode/ReceiverModule.h"
#include "game/equipment/signalNode/SignalTransmitterModule.h"
#include "game/equipment/jammer/JammerModule.h"

#include "ShipSignalProfile.h"


struct ShipDescriptor
{      

    // другие системы
    // ShipSignalProfile  signals;
    // ShipCombatProfile  combat;
    // ShipCargoProfile   cargo;

    // общие тестовые данные (имя, тип, фракция)
    std::string                 name;                   // "Cobra Mk III"

    // физические параметры
    ShipParams                  physics;                // лётные характеристики
   
    
    // HUD / кабина
    ShipHudProfile              hud;                
    
    // описание оборудования equipment
    ReceiverModule              receiver;
    SignalTransmitterModule     transmitter;
    // JammerModule                jammer;

    struct EquipmentSlots
    {
        const ReceiverDesc*             receiver     = nullptr;
        const SignalTransmitterDesc*    transmitter  = nullptr;
        const JammerDesc*               jammer        = nullptr;
    } equipment;

    // управление передатчиком 
    ShipSignalProfile                   signalProfile;
};
