#pragma once

#include "IEquipmentModule.h"

struct EquipmentSlot
{
    IEquipmentModule* module = nullptr;
    const void*       desc   = nullptr;
};
