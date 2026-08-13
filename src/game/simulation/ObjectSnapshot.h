#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include <string>
#include "src/game/simulation/ObjectGraphSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/world/orbits/OrbitalMotion.h"

struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;
    int systemId = -1;

    world::coordinates::WorldPosition worldPosition;
    glm::vec3 position; // legacy mirror
    
    glm::mat4 orientation {1.0f};
    glm::dvec3 linearVelocityMps {0.0};
    game::simulation::HubAttachmentSnapshot hubAttachment;

    // Authoritative instance/navigation facts. These are not map DTO fields:
    // the client decides whether/how to present them on any map.
    std::string displayName;
    std::string ownerName;
    bool navigationVisible = false;
    std::string navigationParentBodyId;
    world::orbits::OrbitalMotion orbitalMotion;

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