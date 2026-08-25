#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace world::navigation
{

/*
    Canonical vehicle attitude for the navigation stack.

    Local vehicle convention is shared with GuidanceFrame/ship presentation:
      +X = right, +Y = top, -Z = nose/forward.

    Keeping this conversion in one renderer/client/server-agnostic helper avoids
    the subtle 180-degree error that appears when a docking-port frame (whose
    forward points out of the aperture) is used directly as the desired vehicle
    attitude (whose nose must point into the aperture).
*/
inline glm::dquat orientationForForwardUp(
    const glm::dvec3& forward,
    const glm::dvec3& upHint
) noexcept
{
    constexpr double Epsilon = 1.0e-9;

    const auto normalizedOr = [&](const glm::dvec3& value, const glm::dvec3& fallback)
    {
        const double length2 = glm::dot(value, value);
        if (!std::isfinite(length2) || length2 <= Epsilon * Epsilon)
            return fallback;
        return value / std::sqrt(length2);
    };

    const glm::dvec3 f = normalizedOr(
        forward,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 back = -f;
    glm::dvec3 up = normalizedOr(
        upHint,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    glm::dvec3 right = glm::cross(up, back);
    if (glm::dot(right, right) <= Epsilon * Epsilon)
    {
        up = std::abs(back.y) < 0.9
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);
        right = glm::cross(up, back);
    }

    right = normalizedOr(right, glm::dvec3(1.0, 0.0, 0.0));
    up = normalizedOr(glm::cross(back, right), up);
    return glm::normalize(glm::quat_cast(glm::dmat3(right, up, back)));
}

} // namespace world::navigation
