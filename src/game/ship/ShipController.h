#pragma once

#include "game/ship/core/ShipParams.h"
#include "game/ship/core/ShipTransform.h"
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