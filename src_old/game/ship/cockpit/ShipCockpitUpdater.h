#pragma once
#include "src/game/ship/cockpit/ShipCockpitState.h"

struct Ship;

class ShipCockpitUpdater {
public:
    void updateFromShip(
        const Ship& ship,
        ShipCockpitState& state
    );
};
