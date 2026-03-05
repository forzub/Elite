#pragma once

#include "DamageType.h"
#include "DamageSeverity.h"

namespace game::damage
{

struct DamageEvent
{
    DamageType      type;
    DamageSeverity  severity;

    // 0.0f = постоянное воздействие
    // > 0.0f = временное (секунды)
    float           duration = 0.0f;

    // внутренняя служебная величина
    float           elapsed = 0.0f;
};

}