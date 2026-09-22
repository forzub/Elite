#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"

namespace game::navigation
{

// Canonical immutable vehicle input for navigation planning/execution.
//
// This is NOT a ship-specific descriptor. The application supplies one profile
// for the concrete vehicle instance/type being planned. Navigation code does
// not reconstruct vehicle constants, read global descriptors, or invent
// alternate capability numbers at subsystem boundaries.
struct VehicleDynamicsProfile
{
    ShipParams physics {};
    glm::dvec3 bodyHalfExtentsMeters {0.5};
    std::uint64_t capabilityRevision = 1;

    [[nodiscard]] bool valid() const noexcept
    {
        const auto finite = [](double v) noexcept
        {
            return std::isfinite(v);
        };

        return
            finite(bodyHalfExtentsMeters.x) &&
            finite(bodyHalfExtentsMeters.y) &&
            finite(bodyHalfExtentsMeters.z) &&
            bodyHalfExtentsMeters.x > 0.0 &&
            bodyHalfExtentsMeters.y > 0.0 &&
            bodyHalfExtentsMeters.z > 0.0 &&
            capabilityRevision != 0;
    }
};

[[nodiscard]] inline double conservativeCollisionRadiusMeters(
    const VehicleDynamicsProfile& profile
) noexcept
{
    return std::max({
        0.0,
        profile.bodyHalfExtentsMeters.x,
        profile.bodyHalfExtentsMeters.y,
        profile.bodyHalfExtentsMeters.z
    });
}

} // namespace game::navigation
