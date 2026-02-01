#pragma once

#include "src/game/equipment/EquipmentModule.h"
#include "SignalTransmitterDesc.h"
// #include "world/types/SignalType.h"
// #include "world/types/SignalDisplayClass.h"


#include <vector>


struct SignalTransmitterModule : public EquipmentModule
{
                                                // --- физика --
    float txPower   = 0.0f;                       // нормализованная мощность
    float baseRange = 0.0f;                     // базовая дальность

                                                // --- режимы передачи ---
   
    SignalDisplayClass                          displayClass = SignalDisplayClass::Local;
    

    // --- init ---
    void init(const SignalTransmitterDesc& desc)
    {
        this->desc = desc.base;

        txPower     = desc.txPower;
        baseRange   = desc.baseRange;
        displayClass = desc.displayClass;

        enabled = true;
        health  = 1.0f;
    }



};



