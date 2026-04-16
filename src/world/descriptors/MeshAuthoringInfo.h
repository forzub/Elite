#pragma once

#include <glm/glm.hpp>

struct MeshAuthoringInfo
{
    glm::vec3 forwardAxis {0.0f, 0.0f, -1.0f};
    glm::vec3 upAxis      {0.0f, 1.0f,  0.0f};

    // Сколько метров в одной единице меша
    float unitsToMeters = 1.0f;
};