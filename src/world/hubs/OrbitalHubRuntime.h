#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include "src/scene/EntityId.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"

namespace world::hubs
{
    struct OrbitalHubRuntime
    {
        std::string id;
        std::string name;
        std::string owner;

        int systemId = -1;
        std::string parentBodyId;

        world::orbits::OrbitalMotion motion;

        world::coordinates::WorldPosition worldPosition;
        glm::mat4 orientation {1.0f};

        std::vector<EntityId> modules;
    };
}