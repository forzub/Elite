#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation
{

// Ordinary route policy.
//
// This is the single place where normal-route comfort/efficiency decisions
// live. Geometry/topology layers provide legal space; this policy converts
// vehicle state into ordinary lead distances, braking approaches and
// candidate preference. Safety/topology remain hard constraints.
//
// Priority:
//   1. valid/clear/semantic route (enforced by planners/validators)
//   2. preserve comfortable turn geometry
//   3. among valid geometry, prefer shorter distance
//   4. speed yields to geometry; traversal time is only a tie-breaker
class OrdinaryRoutePolicy final
{
public:
    [[nodiscard]] static double stageSpeedLimitMps(
        double routeMaxSpeedMps,
        double authoredStageMaxSpeedMps
    ) noexcept
    {
        const double routeMax =
            std::max(0.0, finiteOrZero(routeMaxSpeedMps));
        const double authored =
            std::max(0.0, finiteOrZero(authoredStageMaxSpeedMps));

        // Missing/zero authored limit means 100% of the ordinary route limit.
        if (authored <= 1.0e-9)
            return routeMax;
        return std::min(routeMax, authored);
    }

    [[nodiscard]] static double requiredStartLeadMeters(
        double initialSpeedMps,
        double routeMaxSpeedMps,
        double brakingAccelerationMps2,
        double lateralAccelerationMps2,
        double maxAngularVelocityRadPerSec,
        double vehicleLengthMeters
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
        const double vehicleLength =
            std::max(0.0, finiteOrZero(vehicleLengthMeters));

        const double baseLead =
            std::max(1000.0, vehicleLength * 10.0);

        if (speed <= 0.5 || routeMax <= 0.5)
            return baseLead;

        // Ordinary geometry is allowed to reduce speed for curvature. The
        // start lead only guarantees enough straight room to shed excess
        // incoming speed and to establish the first comfortable-radius turn.
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

        return std::max(
            baseLead,
            2.0 * (brakingDistance + designTurnRadius)
        );
    }

    [[nodiscard]] static double requiredTerminalApproachMeters(
        double possibleApproachSpeedMps,
        double targetSpeedMps,
        double brakingAccelerationMps2,
        double terminalReferenceDistanceMeters,
        double vehicleLengthMeters,
        double safetyFactor = 1.25
    ) noexcept
    {
        const double v0 =
            std::max(0.0, finiteOrZero(possibleApproachSpeedMps));
        const double v1 =
            std::clamp(
                finiteOrZero(targetSpeedMps),
                0.0,
                v0
            );
        const double braking =
            std::max(1.0e-6, finiteOrZero(brakingAccelerationMps2));
        const double reference =
            std::max(0.0, finiteOrZero(terminalReferenceDistanceMeters));
        const double vehicleLength =
            std::max(0.0, finiteOrZero(vehicleLengthMeters));
        const double margin =
            std::max(1.0, finiteOrZero(safetyFactor));

        const double brakingDistance =
            std::max(
                0.0,
                (v0 * v0 - v1 * v1) / (2.0 * braking)
            );

        // Geometry still needs a modest axis even when no braking is needed.
        // This is a minimum, not a preferred multi-kilometre magic constant.
        const double geometricMinimum =
            std::max({
                700.0,
                3.0 * reference,
                10.0 * vehicleLength
            });

        return std::max(
            geometricMinimum,
            brakingDistance * margin
        );
    }

    [[nodiscard]] static bool preferCandidate(
        double candidateLengthMeters,
        double candidateTraversalSeconds,
        double selectedLengthMeters,
        double selectedTraversalSeconds
    ) noexcept
    {
        if (!std::isfinite(selectedLengthMeters))
            return true;

        constexpr double distanceEpsilonMeters = 1.0e-3;
        if (candidateLengthMeters + distanceEpsilonMeters <
            selectedLengthMeters)
        {
            return true;
        }
        if (selectedLengthMeters + distanceEpsilonMeters <
            candidateLengthMeters)
        {
            return false;
        }

        // Equal-distance routes may use time as a deterministic tie-breaker.
        return candidateTraversalSeconds + 1.0e-6 <
            selectedTraversalSeconds;
    }

private:
    [[nodiscard]] static double finiteOrZero(double value) noexcept
    {
        return std::isfinite(value) ? value : 0.0;
    }
};

} // namespace game::navigation
