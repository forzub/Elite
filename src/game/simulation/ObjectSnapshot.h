#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include <string>
#include "src/game/simulation/ObjectGraphSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;

    world::coordinates::WorldPosition worldPosition;
    glm::vec3 position; // legacy mirror
    
    glm::mat4 orientation {1.0f};
    // glm::vec3 rotation;

    game::simulation::ObjectGraphSnapshot graph;
};