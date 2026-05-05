#pragma once

#include <glm/vec3.hpp>
#include <string>

#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>
#include <string>

namespace game::damage
{

enum class HitZoneType
{
    Generic,
    Hull,
    Cockpit,
    Reactor,
    Engine,
    Radiator,
    Cargo
};

struct HitVolume
{
    HitZoneType zone = HitZoneType::Generic;

    int priority = 0;
    int layerIndex = 0;

    glm::vec3 center {0.0f};
    glm::vec3 halfSize {0.5f};
    glm::mat3 orientation {1.0f};

    std::string m_label;
    std::string moduleId;
    std::string subsystemId;

    int panelIndex = -1;
    bool destructible = false;
    int meshChunk = -1;

    float health = 1.0f;
    float maxHealth = 1.0f;

    float armor = 0.0f;
    float penetrationResistance = 0.0f;

    bool destroyed = false;

    // Новый слой: support-link hit volumes
    bool supportLinkVolume = false;
    std::string supportLinkId;
    std::string supportModuleId;
    float impulseTolerance = 0.0f;

    bool contains(const glm::vec3& point) const
    {
        glm::vec3 local = glm::transpose(orientation) * (point - center);

        return
            std::abs(local.x) <= halfSize.x &&
            std::abs(local.y) <= halfSize.y &&
            std::abs(local.z) <= halfSize.z;
    }
};

} // namespace game::damage