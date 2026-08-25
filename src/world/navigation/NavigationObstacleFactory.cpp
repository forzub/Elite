#include "src/world/navigation/NavigationObstacleFactory.h"

#include <algorithm>

#include "src/world/descriptors/ObjectDescriptorRegistry.h"

namespace world::navigation
{

std::optional<NavigationObstacle> makeNavigationObstacleForObject(
    ObjectType type,
    const std::string& id,
    std::uint32_t entityId,
    const glm::dvec3& centerMeters,
    const glm::dmat3& localToWorldBasis,
    double requiredClearanceMeters
)
{
    if (type != ObjectType::Station &&
        type != ObjectType::GuidanceDockCube &&
        type != ObjectType::GuidanceDockCylinder)
    {
        return std::nullopt;
    }

    ObjectDescriptorRegistry::ensureInitialized();
    const IObjectDescriptor& descriptor = ObjectDescriptorRegistry::get(type);

    // Navigation geometry follows the normalized logical object basis:
    // +X width, +Y height, +Z length.  Raw mesh extents are an authoring
    // detail and are not reliable for the assembled station.
    glm::dvec3 sizeMeters(descriptor.getMeshSizeMeters());
    const auto& logical = descriptor.logicalDimensions();
    if (logical.enabled &&
        logical.width > 0.0f &&
        logical.height > 0.0f &&
        logical.length > 0.0f)
    {
        sizeMeters = glm::dvec3(
            logical.width,
            logical.height,
            logical.length
        );
    }

    NavigationObstacle obstacle;
    obstacle.id = id;
    obstacle.entityId = entityId;
    obstacle.centerMeters = centerMeters;
    obstacle.localToWorldBasis = localToWorldBasis;
    obstacle.requiredClearanceMeters = std::max(0.0, requiredClearanceMeters);

    if (type == ObjectType::Station ||
        type == ObjectType::GuidanceDockCube)
    {
        obstacle.shape = NavigationObstacleShape::Box;
        obstacle.halfExtentsMeters = glm::max(
            sizeMeters * 0.5,
            glm::dvec3(0.001)
        );
        obstacle.radiusMeters = glm::length(obstacle.halfExtentsMeters);
        return obstacle;
    }

    // The diagnostic cylinder uses a conservative oriented box. A capsule
    // would incorrectly add the cylinder radius to its axial half-length and
    // could swallow the authored docking approach point. OBB keeps the flat
    // end-plane semantics while remaining conservative around the round wall.
    obstacle.shape = NavigationObstacleShape::Box;
    obstacle.halfExtentsMeters = glm::max(
        sizeMeters * 0.5,
        glm::dvec3(0.001)
    );
    obstacle.radiusMeters = glm::length(obstacle.halfExtentsMeters);
    return obstacle;
}

} // namespace world::navigation
