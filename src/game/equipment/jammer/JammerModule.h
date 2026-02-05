#pragma once
#include "JammerDesc.h"
#include "src/game/equipment/EquipmentModule.h"


struct JammerModule : public EquipmentModule
{
    float jammingPower = 0.0f;
    float radius = 0.0f;

    void init(const JammerDesc& desc)
    {
        this->desc = desc.base;
        jammingPower = desc.jammingPower;
        radius = desc.radius;

        enabled = true;
        health = 1.0f;
    }

    
};
