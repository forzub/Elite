#pragma once

#include <string>
#include <glm/glm.hpp>
#include "src/world/coordinates/WorldPosition.h"

namespace game::simulation
{

struct ObjectRepairJobSnapshot
{
    std::string moduleId;

    world::coordinates::WorldPosition droneWorldPosition;
    world::coordinates::WorldPosition fragmentWorldPosition;
    world::coordinates::WorldPosition homeWorldPosition;

    glm::vec3 dronePosition {0.0f};     // legacy mirror
    glm::vec3 fragmentPosition {0.0f};  // legacy mirror
    glm::vec3 homePosition {0.0f};      // legacy mirror

    uint8_t state = 0;
};

}