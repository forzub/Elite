#pragma once

#include <algorithm>
#include <cmath>

namespace game::navigation::assisted_budget
{

// All Assisted axes share installed main motors. Each signed six-axis basis
// maneuver is feasible by itself, but summing three full demands
// (curve-normal, braking, attitude) is not. The engine force/nozzle cones
// are convex under positive scaling: one third of each feasible basis
// command can be blended without exceeding any motor's force limit.
//
// This is a conservative guaranteed planning fraction, not an increase in
// real actuator authority. Once an exact multi-axis feasible-wrench query is
// exposed to navigation, it should replace this lower bound.
inline constexpr double ConcurrentTurnAxisFraction = 1.0 / 3.0;

[[nodiscard]] inline double reservedCurveLateralAccelerationMps2(
    double installedLateralMps2
) noexcept
{
    return std::max(0.0, installedLateralMps2) *
        ConcurrentTurnAxisFraction;
}

[[nodiscard]] inline double maximumSustainableCurveSpeedMps(
    double installedLateralMps2,
    double curvaturePerMeter
) noexcept
{
    if (!(curvaturePerMeter > 0.0) ||
        !std::isfinite(curvaturePerMeter) ||
        !std::isfinite(installedLateralMps2))
        return 0.0;
    return std::sqrt(
        reservedCurveLateralAccelerationMps2(installedLateralMps2) /
        curvaturePerMeter
    );
}

} // namespace game::navigation::assisted_budget
