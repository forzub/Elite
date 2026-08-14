#pragma once

#include <glm/vec3.hpp>
#include <vector>
#include <glm/mat4x4.hpp>
#include "src/world/types/ObjectType.h"
#include "src/scene/EntityID.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/game/damage/HitComponent.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectDetachedFragmentRuntime.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/StructuralLinkSnapshot.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"
#include <string>



struct StaticObject
{
    EntityId id;
    ObjectType type;
    std::string displayName = "Object";
    std::string ownerName = "Independent";

    // Spatial authority. This exists even for infrastructure that is not a
    // top-level marker on the System map.
    int systemId = -1;

    // Presentation/indexing metadata. Child modules may belong to a system
    // without becoming independent top-level map objects.
    bool systemMapVisible = false;
    std::string mapParentBodyId;

    // Spatial parent for authoritative orbital motion. This is deliberately
    // separate from mapParentBodyId: presentation metadata must never drive
    // simulation placement.
    std::string orbitalParentBodyId;
    std::string hubId;
    std::string hubModuleId;
    
    bool attachedToHub = false;
    glm::dvec3 hubLocalOffsetMeters {0.0};

    // Local placement rotation inside the hub.
    // Degrees. Applied after hub orientation.
    glm::dvec3 hubLocalRotationDeg {0.0};

    bool inheritHubOrientation = true;


    world::coordinates::WorldPosition worldPosition;

    // Legacy mirror.
    // Не источник истины. Не использовать в рендере / дистанциях / origin shifting.
    glm::vec3 position {0.0f, 0.0f, 0.0f};

    void setWorldPosition(const world::coordinates::WorldPosition& p)
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }

    void setWorldPositionMeters(const glm::dvec3& meters)
    {
        setWorldPosition(
            world::coordinates::makeWorldPositionFromMeters(meters)
        );
    }
    
    glm::mat4 orientation {1.0f};
    world::orbits::OrbitalMotion orbitalMotion;

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