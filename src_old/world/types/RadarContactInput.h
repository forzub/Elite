#pragma once

#include "src/scene/EntityId.h"
#include <glm/vec3.hpp>

namespace world
{

struct RadarContactInput
{
    EntityId            id;
    glm::vec3           worldPosition;
    double              radarCrossSection;
};

}