#pragma once

#include "game/damage/IDamageable.h"
#include "game/damage/HitComponent.h"
#include "game/damage/IDamageHandler.h"
#include "game/math/MathTransform.h"

namespace game::debug
{

class DamageTestObject : public game::damage::IDamageable
{
public:

    game::damage::HitComponent hitComponent;

    game::damage::IDamageHandler* handler = nullptr;

    game::math::MathTransform transform;

    DamageTestObject();

    void applyDamage(const game::damage::DamageEvent& event) override;

};

}