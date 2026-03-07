#pragma once

#include "game/damage/IDamageHandler.h"
namespace game::ship::core { class ShipCore; }

namespace game::ship
{

class Ship;

class ShipDamageHandler : public game::damage::IDamageHandler
{
public:

    game::ship::core::ShipCore* ship = nullptr;

    void handleDamage(
        const game::damage::DamageResult& result
    ) override;

};

}