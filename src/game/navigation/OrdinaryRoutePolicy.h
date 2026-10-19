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

    // Assisted initial straight: one *braking distance* plus one complete
    // *comfortable turn radius* is enough to reach the tangent arc. The old
    // generic lead doubles BOTH terms, which builds kilometres of extra
    // straight at 350-500 m/s. Keep the original conservative helper for
    // Newtonian/legacy callers (different turn and braking laws).
    // The separate HUD/manual-turn radius limit is untouched.
    [[nodiscard]] static double requiredAssistedStartLeadMeters(
        double initialSpeedMps,
        double routeMaxSpeedMps,
        double brakingAccelerationMps2,
        double lateralAccelerationMps2,
        double maxAngularVelocityRadPerSec,
        double vehicleLengthMeters
    ) noexcept
    {
        const double legacy =
            requiredStartLeadMeters(
                initialSpeedMps,
                routeMaxSpeedMps,
                brakingAccelerationMps2,
                lateralAccelerationMps2,
                maxAngularVelocityRadPerSec,
                vehicleLengthMeters
            );
        const double minimum =
            std::max(1000.0, std::max(0.0, vehicleLengthMeters) * 10.0);
        // Legacy = max(minimum, 2*(D_brake+R_turn)).
        return std::max(minimum, 0.5 * legacy);
    }

    [[nodiscard]] static double requiredGuidanceTunnelRadiusMeters(
        double gateSpacingMeters,
        double vehicleLengthMeters
    ) noexcept
    {
        const double spacing =
            std::max(1.0, finiteOrZero(gateSpacingMeters));
        const double vehicleLength =
            std::max(0.0, finiteOrZero(vehicleLengthMeters));

        // Manual/autopilot HUD corridor contract: neighbouring tunnel frames
        // must still read as one continuous tunnel. A quarter-turn may not be
        // compressed into one or two sparse 500 m gates. 30 degrees per
        // characteristic interval means a 90-degree bend spans at least three
        // visible frame intervals. This is geometric safety and is deliberately
        // independent of how slowly the speed profiler is willing to crawl.
        constexpr double MaxFrameTurnRadians =
            0.52359877559829887308; // pi / 6

        const double characteristicInterval =
            std::max(spacing, vehicleLength);

        return
            characteristicInterval /
            MaxFrameTurnRadians;
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
