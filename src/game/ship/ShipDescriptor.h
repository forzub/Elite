#pragma once

#include <string>
#include "game/ship/ShipParams.h"
#include "game/ship/ShipHudProfile.h"

struct ShipDescriptor
{
    std::string name;        // "Cobra Mk III"

    ShipParams physics;      // лётные характеристики

    ShipHudProfile hud;      // HUD / кабина
};
