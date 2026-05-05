#pragma once

#include <string>
#include <glm/glm.hpp>

namespace game::simulation
{

struct ObjectRepairJobSnapshot
{
    std::string moduleId;

    glm::vec3 dronePosition {0.0f};
    glm::vec3 fragmentPosition {0.0f};
    glm::vec3 homePosition {0.0f};

    uint8_t state = 0;
};

}