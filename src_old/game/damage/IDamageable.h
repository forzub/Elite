#pragma once

#include "DamageEvent.h"

namespace game::damage
{

class IDamageable
{
public:

    virtual void applyDamage(const DamageEvent& event) = 0;

    virtual ~IDamageable() = default;
};

}