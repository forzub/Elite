#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include "src/world/types/ObjectType.h"
#include "src/scene/EntityId.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/game/damage/HitComponent.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectDetachedFragmentRuntime.h"

struct StaticObject
{
    EntityId id;
    ObjectType type;

    glm::vec3 position {0.0f, 0.0f, 0.0f};
    // glm::vec3 rotation {0.0f, 0.0f, 0.0f};
    glm::mat4 orientation {1.0f};

    glm::vec3 angularVelocity {0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity  {0.0f, 0.0f, 0.0f};

    world::modules::ObjectModuleRuntime moduleRuntime;
    world::modules::ObjectStructuralLinkRuntime structuralLinkRuntime;
    world::modules::ObjectAssemblyRuntime assemblyRuntime;
    world::modules::ObjectDetachedFragmentRuntime detachedFragmentRuntime;
    game::damage::HitComponent hitComponent;
    bool hitVolumesDirty = true;
};