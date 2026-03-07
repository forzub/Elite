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
    HitZoneType zone;
    int priority = 0;
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat3 orientation {1.0f};
    
    std::string m_label;
    int panelIndex = -1;
    bool destructible = false;
    int meshChunk = -1;
    float health = 1.0f;
    bool destroyed = false;

    bool contains(const glm::vec3& point) const
    {
        glm::vec3 local = glm::transpose(orientation) * (point - center);

        return
            abs(local.x) <= halfSize.x &&
            abs(local.y) <= halfSize.y &&
            abs(local.z) <= halfSize.z;
    }
};

}