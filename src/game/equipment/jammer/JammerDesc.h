#pragma once

#include "src/game/equipment/EquipmentDescriptor.h"

struct JammerDesc
{
    EquipmentDescriptor base;

    float jammingPower = 0.0f; // нормализованная мощность шума
    float radius       = 0.0f; // радиус действия в метрах
};