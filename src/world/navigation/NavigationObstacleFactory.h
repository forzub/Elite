#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/types/ObjectType.h"

namespace world::navigation
{

/*
    Shared object -> navigation geometry adapter.

    Client-local and future server-shared route calculation must call the same
    adapter so an object never changes collision shape when computation owner
    moves between client and server.
*/
std::optional<NavigationObstacle> makeNavigationObstacleForObject(
    ObjectType type,
    const std::string& id,
    std::uint32_t entityId,
    const glm::dvec3& centerMeters,
    const glm::dmat3& localToWorldBasis,
    double requiredClearanceMeters = 0.0
);

} // namespace world::navigation
