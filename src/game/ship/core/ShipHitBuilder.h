#pragma once

#include "game/damage/HitComponent.h"
#include "game/ship/ShipDescriptor.h"

namespace game::ship
{

class ShipHitBuilder
{
public:

    static void build(
        game::damage::HitComponent& hitComponent,
        const ShipDescriptor& descriptor);

private:

    static void buildRadiatorPanels(
        game::damage::HitComponent& hitComponent,
        const ShipDescriptor& descriptor);
};

}