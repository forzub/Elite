#pragma once

#include <string>
#include <glm/glm.hpp>

namespace game::simulation
{

struct DebugHitVolumeSnapshot
{
    std::string moduleId;
    std::string subsystemId;

    int layerIndex = 0;
    int priority = 0;

    glm::vec3 center {0.0f};
    glm::vec3 halfSize {0.5f};
    glm::mat3 orientation {1.0f};

    bool destructible = false;
    bool destroyed = false;

    float health = 0.0f;
    float maxHealth = 0.0f;
};

} // namespace game::simulation