#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace world::navigation
{

enum class NavigationBodyShape
{
    Sphere,
    Box,
    Capsule
};

struct NavigationBody
{
    uint32_t entityId = 0;

    std::string debugName;

    NavigationBodyShape shape =
        NavigationBodyShape::Sphere;

    glm::vec3 center {0.0f};

    glm::quat rotation {1, 0, 0, 0};

    // Sphere
    float radius = 1.0f;

    // Box
    glm::vec3 halfExtents {1.0f};

    // Capsule
    float halfLength = 1.0f;

    glm::vec3 linearVelocity {0.0f};
    glm::vec3 angularVelocity {0.0f};

    bool dynamic = false;
};

} // namespace world::navigation