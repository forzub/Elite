#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include <string>
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/ObjectAssemblyModuleSnapshot.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

struct ObjectSnapshot
{
    EntityId id;
    ObjectType type;

    glm::vec3 position;
    glm::mat4 orientation {1.0f};
    // glm::vec3 rotation;

    std::vector<game::simulation::ObjectModuleSnapshot> modules;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> assemblyModules;
    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;
};