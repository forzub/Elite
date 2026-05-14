#pragma once

#include <vector>

#include "NavigationBody.h"

namespace game::ship::core
{
class ShipCore;
}

namespace world::navigation
{

std::vector<NavigationBody> buildNavigationBodiesForShip(
    const game::ship::core::ShipCore& ship
);

} // namespace world::navigation