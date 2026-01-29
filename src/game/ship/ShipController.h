#pragma once

#include "game/ship/ShipParams.h"
#include "game/ship/ShipTransform.h"
#include "world/WorldParams.h"

class ShipController
{
public:
    void update(
        float dt,
        const ShipParams& params,
        ShipTransform& ship,
        const WorldParams& world
    );
};