#pragma once

#include <glm/vec3.hpp>
#include <vector>
#include <glm/mat4x4.hpp>
#include "src/world/types/ObjectType.h"
#include "src/scene/EntityId.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/game/damage/HitComponent.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectDetachedFragmentRuntime.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/StructuralLinkSnapshot.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

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

    // Static snapshot payload cache.
    // Rotation changes every tick, but modules / structural links / debug hit volumes
    // usually do not. Rebuilding and copying them every frame causes CPU spikes.
    bool staticSnapshotPayloadDirty = true;
    std::vector<game::simulation::ObjectModuleSnapshot> cachedModuleSnapshots;
    std::vector<game::simulation::StructuralLinkSnapshot> cachedStructuralLinkSnapshots;
    std::vector<game::simulation::DebugHitVolumeSnapshot> cachedDebugHitVolumeSnapshots;
};