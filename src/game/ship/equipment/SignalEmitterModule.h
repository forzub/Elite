#pragma once

#include "EquipmentModule.h"
#include "world/types/SignalType.h"
#include "world/types/SignalDisplayClass.h"


#include <vector>


struct TransmitterSignalMode
{
    SignalType         type;
    
};


struct SignalEmitterModule : public EquipmentModule
{
    // --- физика --
    float txPower = 0.0f;        // нормализованная мощность
    float baseRange = 0.0f;   // базовая дальность

    // --- режимы передачи ---
    std::vector<TransmitterSignalMode>          modes;
    SignalDisplayClass                          displayClass = SignalDisplayClass::Local;
    int                                         activeMode = 0;
    // WorldSignal                                 activeSignal;

    bool hasValidMode() const
    {
        return activeMode >= 0 && activeMode < (int)modes.size();
    }


    const TransmitterSignalMode& currentMode() const
    {
        return modes[activeMode];
    }


    void nextMode()
    {
        if (!modes.empty())
            activeMode = (activeMode + 1) % modes.size();
    }
};



// if (ship.equipment.transmitter.isOperational())
// {
//     WorldSignal sig;
//     sig.power = ship.equipment.transmitter.txPower;
//     sig.position = ship.transform.position;
//     // ...
// }