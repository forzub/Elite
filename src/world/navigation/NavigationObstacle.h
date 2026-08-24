#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace world::navigation
{

enum class NavigationObstacleShape : std::uint8_t
{
    Sphere = 0,
    Box,
    Capsule
};

/*
    Canonical static geometry consumed by every geometric route planner.

    centerMeters/localToWorldBasis define the obstacle pose in the caller's
    planning frame. Motion/observation uncertainty belongs to the higher-level
    planning snapshot; the physical geometry itself stays reusable by repair
    drones, ships and later server-side route calculation.
*/
struct NavigationObstacle
{
    std::string id;
    std::uint32_t entityId = 0;

    NavigationObstacleShape shape = NavigationObstacleShape::Sphere;
    glm::dvec3 centerMeters {0.0};
    glm::dmat3 localToWorldBasis {1.0};

    // Sphere radius, or capsule radial radius.
    double radiusMeters = 1.0;

    // OBB half sizes in local obstacle axes.
    glm::dvec3 halfExtentsMeters {1.0};

    // Capsule centerline extends +/- this distance along local +Z/-Z.
    double capsuleHalfLengthMeters = 0.0;

    // Authored safety policy that applies in addition to agent clearance.
    double requiredClearanceMeters = 0.0;

    bool finite() const noexcept
    {
        const auto finite3 = [](const glm::dvec3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) &&
                std::isfinite(v.z);
        };
        return finite3(centerMeters) && finite3(halfExtentsMeters) &&
            finite3(glm::dvec3(localToWorldBasis[0])) &&
            finite3(glm::dvec3(localToWorldBasis[1])) &&
            finite3(glm::dvec3(localToWorldBasis[2])) &&
            std::isfinite(radiusMeters) &&
            std::isfinite(capsuleHalfLengthMeters) &&
            std::isfinite(requiredClearanceMeters);
    }

    double conservativeRadiusMeters() const noexcept
    {
        switch (shape)
        {
            case NavigationObstacleShape::Box:
                return glm::length(glm::max(halfExtentsMeters, glm::dvec3(0.0)));
            case NavigationObstacleShape::Capsule:
                return std::max(0.0, capsuleHalfLengthMeters) +
                    std::max(0.0, radiusMeters);
            case NavigationObstacleShape::Sphere:
            default:
                return std::max(0.0, radiusMeters);
        }
    }
};

} // namespace world::navigation
