#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation
{

/*
    Physical vehicle envelope used by local 6-DOF guidance.

    Dimensions use the canonical ship basis:
      X = right/left  -> width
      Y = top/belly   -> height
      Z = tail/nose   -> length

    The first implementation intentionally uses a conservative bounding box
    and its circumscribed sphere for obstacle safety.  The contract is kept
    separate so swept oriented-box/mesh checks can replace that conservative
    evaluator without changing docking targets, corridor presentation or the
    future follower interface.
*/
struct VehicleGuidanceEnvelope
{
    double lengthMeters = 0.0;
    double widthMeters = 0.0;
    double heightMeters = 0.0;
    double hullClearanceMeters = 0.0;
    bool valid = false;

    glm::dvec3 halfExtentsMeters() const noexcept
    {
        return {
            std::max(0.0, widthMeters) * 0.5,
            std::max(0.0, heightMeters) * 0.5,
            std::max(0.0, lengthMeters) * 0.5
        };
    }

    double conservativeSafetyRadiusMeters() const noexcept
    {
        if (!valid)
            return 0.0;

        const glm::dvec3 half = halfExtentsMeters();
        return std::sqrt(glm::dot(half, half)) +
            std::max(0.0, hullClearanceMeters);
    }

    double terminalCenterDepthMeters(double dockClearanceMeters) const noexcept
    {
        if (!valid)
            return std::max(0.0, dockClearanceMeters);

        return std::max(0.0, lengthMeters) * 0.5 +
            std::max({0.0, hullClearanceMeters, dockClearanceMeters});
    }
};

} // namespace game::navigation
