#pragma once

#include "src/game/ship/core/ShipControlState.h"

struct PlayerInputMapper
{
    void update(ShipControlState& ctrl);
};
