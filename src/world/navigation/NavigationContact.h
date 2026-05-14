#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace world::navigation
{

enum class NavigationContactKind : uint8_t
{
    Unknown = 0,

    Ship,
    StaticObject,
    DetachedFragment,
    Missile,
    Debris,
    Asteroid,
    Station
};

struct NavigationContact
{
    uint32_t entityId = 0;
    uint32_t ownerEntityId = 0;

    NavigationContactKind kind = NavigationContactKind::Unknown;

    glm::vec3 center {0.0f};
    float radius = 1.0f;

    glm::vec3 linearVelocity {0.0f};
    glm::vec3 angularVelocity {0.0f};

    float danger = 1.0f;

    bool dynamic = false;
    bool predictable = true;
};

struct NavigationContactQuery
{
    glm::vec3 center {0.0f};
    float radius = 100.0f;

    uint32_t ignoreEntityId = 0;
};

} // namespace world::navigation