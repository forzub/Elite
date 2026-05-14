#pragma once

#include "src/world/objects/StaticObject.h"
#include "src/game/damage/DamageEvent.h"

namespace world::objects
{

void applyDamage(StaticObject& obj, const game::damage::DamageEvent& event);

}