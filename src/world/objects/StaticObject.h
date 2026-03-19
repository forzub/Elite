#pragma once

#include <glm/vec3.hpp>
#include "src/world/types/ObjectType.h"
#include "src/scene/EntityId.h"

struct StaticObject
{
    EntityId id;
    ObjectType type;

    // === Transform ===
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation {0.0f, 0.0f, 0.0f};

    // === Будущее (не обязательно использовать сейчас) ===
    glm::vec3 angularVelocity {0.0f, 0.0f, 0.0f}; // вращение станции
    glm::vec3 linearVelocity  {0.0f, 0.0f, 0.0f}; // если будут таскать
};