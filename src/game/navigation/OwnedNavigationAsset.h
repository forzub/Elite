#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationAssetRef.h"
#include "src/scene/EntityID.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/types/ObjectType.h"

namespace game::navigation
{

/*
    Per-session server projection of route-capable assets.

    Ownership/command permission is authoritative. EntityId and kinematics are
    merely the latest materialized binding/presentation sample and may change
    without changing NavigationAssetRef.
*/
struct OwnedNavigationAsset
{
    NavigationAssetRef asset;
    EntityId materializedEntityId {0};
    ObjectType typeId = ObjectType::None;
    std::string displayName;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    bool kinematicsValid = false;
    bool commandable = false;
};

} // namespace game::navigation
