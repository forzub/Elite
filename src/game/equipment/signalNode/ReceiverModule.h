#pragma once

#include "src/game/equipment/EquipmentModule.h"
#include "ReceiverDesc.h"



struct ReceiverModule : public EquipmentModule
{
    float sensitivity = 1.0f;

    void init(const ReceiverDesc& desc)
    {
        this->desc = desc.base;
        sensitivity = desc.sensitivity;

        enabled = true;
        health  = 1.0f;
    }

    
};
