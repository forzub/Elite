#pragma once

#include "EquipmentModule.h"

struct JammerModule : public EquipmentModule
{
    float jammingPower = 0.0f;   // мощность помех
    float radius = 0.0f;       // радиус действия
};





// if (ship.equipment.jammer.isOperational())
// {
//     InterferenceSource src;
//     src.position = ship.transform.position;
//     src.power    = ship.equipment.jammer.jammingPower;
//     src.radius   = ship.equipment.jammer.radius;
//     src.enabled  = true;

//     interferenceSources.push_back(src);
// }
