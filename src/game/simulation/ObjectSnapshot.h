#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include <string>
#include "src/game/simulation/ObjectGraphSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"

struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;
    int systemId = -1;

    world::coordinates::WorldPosition worldPosition;
    glm::vec3 position; // legacy mirror
    
    glm::mat4 orientation {1.0f};
    game::simulation::HubAttachmentSnapshot hubAttachment;
    // glm::vec3 rotation;

    game::simulation::ObjectGraphSnapshot graph;


    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }
};