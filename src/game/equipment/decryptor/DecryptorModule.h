#pragma once

#include <vector>
#include "src/game/equipment/EquipmentModule.h"
#include "DecryptorDesc.h"
#include "src/galaxy/Actors.h"


struct CipherCard
{
    std::vector<ActorCode> acceptedCodes;
};


struct DecryptorModule : public EquipmentModule
{
    int slotCount;

    void init(const DecryptorDesc& desc)
    {
        
        this->desc = desc.base;
        slotCount = desc.slotCount;
        
        enabled = true;
        health = 1.0f;
        
        printf("[DEBUG DecryptorModule] %s: name=%d, enabled=%d \n",desc.base.id, enabled);
    }

    
};



