#pragma once

#include "DamageEvent.h"
#include "IDamageable.h"

namespace game::damage
{

class DamageSystem
{
public:

    static void applyDamage(
        IDamageable& target,
        const DamageEvent& event
    );

};

}