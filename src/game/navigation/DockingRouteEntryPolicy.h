#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation
{

// WORKING ENTRY CONTRACT FOR MOVING-START ROUTES.
//
// A moving ship is not allowed to "teleport" its measured velocity onto the
// route. Planner entry geometry must give the craft real distance to:
//   1) continue along its CURRENT VELOCITY direction while planning;
//   2) brake to a physically admissible first-turn speed;
//   3) enter the first authored turn with enough tangent room.
//
// Hull nose is intentionally NOT the moving-start course reference. At useful
// speed, velocity owns navigation.
class DockingRouteEntryPolicy final
{
public:
    [[nodiscard]] static glm::dvec3 initialCourse(
        const glm::dvec3& velocityMapMps,
        const glm::dvec3& fallbackHullForward,
        double velocityThresholdMps = 0.5
    ) noexcept
    {
        const double speed = glm::length(velocityMapMps);
        if (std::isfinite(speed) &&
            speed > std::max(0.0, velocityThresholdMps))
        {
            return velocityMapMps / speed;
        }

        const double fallbackLength =
            glm::length(fallbackHullForward);
        if (std::isfinite(fallbackLength) &&
            fallbackLength > 1.0e-12)
        {
            return fallbackHullForward / fallbackLength;
        }

        return glm::dvec3(0.0, 0.0, -1.0);
    }

    [[nodiscard]] static double requiredInitialForwardLeadMeters(
        double initialSpeedMps,
        double routeMaxSpeedMps,
        double brakingAccelerationMps2,
        double lateralAccelerationMps2,
        double maxAngularVelocityRadPerSec,
        double hullLengthMeters
    ) noexcept
    {
        const double speed =
            std::max(0.0, finiteOrZero(initialSpeedMps));
        const double routeMax =
            std::max(0.0, finiteOrZero(routeMaxSpeedMps));
        const double braking =
            std::max(1.0e-6, finiteOrZero(brakingAccelerationMps2));
        const double lateral =
            std::max(1.0e-6, finiteOrZero(lateralAccelerationMps2));
        const double maxOmega =
            std::max(1.0e-6, finiteOrZero(maxAngularVelocityRadPerSec));
        const double hullLength =
            std::max(0.0, finiteOrZero(hullLengthMeters));

        const double baseLead =
            std::max(1000.0, hullLength * 10.0);

        if (speed <= 0.5 || routeMax <= 0.5)
            return baseLead;

        // Generic transit turns are authored around 0.8 * route max speed.
        // If the craft arrives faster, the protected launch straight must be
        // long enough to brake down to that design speed before curvature.
        const double designTurnSpeed =
            std::min(speed, 0.80 * routeMax);

        const double brakingDistance =
            speed > designTurnSpeed
                ? (speed * speed -
                   designTurnSpeed * designTurnSpeed) /
                    (2.0 * braking)
                : 0.0;

        const double lateralRadius =
            designTurnSpeed * designTurnSpeed / lateral;
        const double angularRadius =
            designTurnSpeed / maxOmega;
        const double designTurnRadius =
            std::max(lateralRadius, angularRadius);

        // DockingAdvisoryPlanner intentionally protects roughly the first
        // half of this lead as straight flight and leaves the other half for
        // the launch fillet. Doubling (braking distance + radius) guarantees
        // both budgets exist independently instead of forcing the fillet to
        // shrink merely because initial speed was high.
        const double dynamicLead =
            2.0 * (brakingDistance + designTurnRadius);

        return std::max(baseLead, dynamicLead);
    }

private:
    [[nodiscard]] static double finiteOrZero(double value) noexcept
    {
        return std::isfinite(value) ? value : 0.0;
    }
};

} // namespace game::navigation
