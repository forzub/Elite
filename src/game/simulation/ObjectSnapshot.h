#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include <string>
#include "src/game/simulation/ObjectGraphSnapshot.h"

struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;

    glm::vec3 position;
    glm::mat4 orientation {1.0f};
    // glm::vec3 rotation;

    game::simulation::ObjectGraphSnapshot graph;
};