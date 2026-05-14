#pragma once
#include "game/ship/Ship.h"

class NpcAiSystem
{
public:
    ShipControlState computeControl(const Ship& ship, float dt);
};


