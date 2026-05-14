#pragma once

#include "game/damage/IDamageHandler.h"
#include <iostream>

namespace game::damage
{

class TestDamageHandler : public game::damage::IDamageHandler
{
public:

    void handleDamage(const game::damage::DamageResult& result) override;

};

}