#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"


struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;

    glm::vec3 position;
    glm::vec3 rotation;
};