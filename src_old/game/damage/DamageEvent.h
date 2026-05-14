#pragma once

#include <glm/vec3.hpp>

namespace game::damage
{

enum class DamageType
{
    Laser,
    Explosion,
    Collision,
    Radiation,
    EMP
};

struct DamageEvent
{
    DamageType type;

    double energy;          // энергия воздействия

    glm::vec3 position;     // точка удара (world space)

    glm::vec3 direction;    // направление удара
};

}