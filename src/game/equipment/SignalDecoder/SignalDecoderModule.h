#pragma once
#include "src/game/equipment/EquipmentModule.h"

struct SignalDecoderModule : EquipmentModule
{
    uint32_t slotCount;

    // коды акторов / фракций, которые можем дешифровать
    std::vector<ActorCode> installedCodes;

    bool canDecode(ActorCode code) const
    {
        // All — встроенное правило
        if (code == ActorCodeAll)
            return true;

        return std::find(installedCodes.begin(),
                         installedCodes.end(),
                         code) != installedCodes.end();
    }
};

constexpr ActorCode ActorCodeAll = 0xFFFFFFFF;
