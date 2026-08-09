#pragma once

#include <algorithm>
#include <cmath>

#include "src/world/descriptors/LogicalDimensions.h"

namespace game::simulation::activation
{

struct SpatialBounds
{
    // Conservative spherical bound used only for broad interaction queries.
    // Detailed collision remains owned by the physics/hit-volume systems.
    double interactionRadiusMeters = 0.0;
};

inline double conservativeRadiusFromLogicalDimensions(
    const LogicalDimensions& dimensions
) noexcept
{
    if (!dimensions.enabled)
        return 0.0;

    const double length = std::max(0.0, static_cast<double>(dimensions.length));
    const double width = std::max(0.0, static_cast<double>(dimensions.width));
    const double height = std::max(0.0, static_cast<double>(dimensions.height));

    return 0.5 * std::sqrt(
        length * length +
        width * width +
        height * height
    );
}

inline SpatialBounds makeSpatialBounds(
    const LogicalDimensions& dimensions
) noexcept
{
    return SpatialBounds{
        conservativeRadiusFromLogicalDimensions(dimensions)
    };
}

} // namespace game::simulation::activation
