#include "src/game/ship/cockpit/ShipCockpitUpdater.h"
#include "src/game/ship/Ship.h"

void ShipCockpitUpdater::updateFromShip(
    const Ship& ship,
    ShipCockpitState& state
) {
    // float speedNorm = ship.speed() / ship.maxSpeed();

    // auto& e = state.elements["speed_bar"];
    // e.value01 = speedNorm;
    // e.color = {0.2f, 0.8f, 0.2f, 1.0f};
}
