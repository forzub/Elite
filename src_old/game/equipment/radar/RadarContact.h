#pragma once

#include "src/scene/EntityId.h"
#include <glm/vec3.hpp>

namespace game
{

struct RadarContact
{
    EntityId    id;
    double      distance;
    glm::vec3   localPosition;  // позиция цели в локальных координатах
//     glm::vec3   worldPosition;  // позиция цели в локальных координатах
};

}