#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation
{

// WORKING GEOMETRY CONTRACT.
//
// A roll phase is defined by TWO points plus an axis:
//   center       -- point on the rotation axis;
//   radialPoint  -- any stable off-axis point (for example "bottom").
//
// This is deliberately independent of dock, ship, HUD and actuator semantics.
// The same function is used to compare dock/tunnel phase and hull/tunnel phase.
// Translation, radial length and world orientation must not change the result.
class TwoPointRollGeometry final
{
public:
    struct Reference
    {
        glm::dvec3 center {0.0};
        glm::dvec3 radialPoint {0.0, -1.0, 0.0};
    };

    [[nodiscard]] static double signedPhase(
        const glm::dvec3& axisRequested,
        const Reference& from,
        const Reference& to
    ) noexcept
    {
        const glm::dvec3 axis =
            normalizedOr(axisRequested, glm::dvec3(0.0, 0.0, -1.0));

        glm::dvec3 a = from.radialPoint - from.center;
        glm::dvec3 b = to.radialPoint - to.center;

        a -= axis * glm::dot(a, axis);
        b -= axis * glm::dot(b, axis);

        const double aLength = glm::length(a);
        const double bLength = glm::length(b);
        if (!(std::isfinite(aLength) && std::isfinite(bLength)) ||
            aLength <= 1.0e-9 ||
            bLength <= 1.0e-9)
        {
            return 0.0;
        }

        a /= aLength;
        b /= bLength;

        return std::atan2(
            glm::dot(axis, glm::cross(a, b)),
            std::clamp(glm::dot(a, b), -1.0, 1.0)
        );
    }

    [[nodiscard]] static Reference fromCenterAndUp(
        const glm::dvec3& center,
        const glm::dvec3& up,
        double radius = 1.0
    ) noexcept
    {
        const double safeRadius =
            std::isfinite(radius) && radius > 1.0e-9
                ? radius
                : 1.0;
        const glm::dvec3 normalizedUp =
            normalizedOr(up, glm::dvec3(0.0, 1.0, 0.0));

        return {
            center,
            center - normalizedUp * safeRadius
        };
    }

private:
    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;

        const double fallbackLength = glm::length(fallback);
        return
            std::isfinite(fallbackLength) &&
            fallbackLength > 1.0e-12
                ? fallback / fallbackLength
                : glm::dvec3(0.0, 0.0, -1.0);
    }
};

} // namespace game::navigation
