#pragma once

#include "src/scene/EntityId.h"
#include "src/world/coordinates/WorldPosition.h"

namespace world
{

struct RadarContactInput
{
    EntityId id;

    // Истинная глобальная позиция цели.
    world::coordinates::WorldPosition worldPosition;

    double radarCrossSection = 1.0;
};

}