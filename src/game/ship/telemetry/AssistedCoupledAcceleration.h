#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>

#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"

namespace game::ship::telemetry
{

// Exact motor-program mixing cost for a demanded six-component wrench.
// Returns FALSE if even one installed motor cone/cap is exhausted.
// Mirrors AssistedManeuverPrograms::execute: the executor adds these signed
// basis vectors, then applies a common global scaling when any physical motor
// saturates. By admitting only commands needing NO scaling, navigation never
// claims a turn acceleration that the executor would reduce.
// The cone remains valid for these mixtures because each signed preset is
// inside the same positive thrust cone (a convex set).
[[nodiscard]] inline bool assistedCombinedDemandFeasible(
    const AssistedCoupledMotorAuthority& model,
    const std::array<double,6>& demand
) noexcept
{
    if (!model.valid || model.motorCount == 0 ||
        model.motorCount > model.MaxMotors)
        return false;

    std::array<double, AssistedCoupledMotorAuthority::ForceComponents> mix {};
    for (std::size_t axis=0;axis<6;++axis)
    {
        if (!std::isfinite(demand[axis]))
            return false;
        if (std::abs(demand[axis]) < 1.0e-5)
            continue;
        const std::size_t signedAxis =
            2*axis+(demand[axis]<0.0 ? 1u : 0u);
        const double available=model.achievable[signedAxis];
        if (!(available>1.0e-9) || !std::isfinite(available))
            return false;
        const double share=std::abs(demand[axis])/available;
        // The physical executor never provides more than one complete
        // basis program for any single axis, irrespective of throttle.
        if (share>1.0+1.0e-6)
            return false;
        for(std::size_t i=0;i<3*model.motorCount;++i)
            mix[i]+=
                share*static_cast<double>(model.fractions[signedAxis][i]);
    }

    for (std::size_t i=0;i<model.motorCount;++i)
    {
        const double capacity=model.available01[i];
        if (!std::isfinite(capacity) || capacity<0.0)
            return false;
        const double size=std::sqrt(
            mix[3*i]*mix[3*i]+
            mix[3*i+1]*mix[3*i+1]+
            mix[3*i+2]*mix[3*i+2]
        );
        if (!std::isfinite(size) || size>capacity+1.0e-5)
            return false;
    }
    return true;
}

// Find the physical vector acceleration available in the route-curve normal,
// alongside CURRENT axial demand and hull angular-control demand. Normal
// must be in ship BODY (+X right,+Y up,+Z aft). The answer is a measured
// installed-engine allocation limit, NOT a planner preference or fixed
// per-axis fraction. Bisection only queries the already-cached motor basis.
[[nodiscard]] inline double maximumAssistedCurveNormalAccelerationMps2(
    const AssistedCoupledMotorAuthority& model,
    const ShipControlAuthorityTelemetry& limits,
    const glm::dvec3& normalBody,
    const std::array<double,6>& concurrentDemand
) noexcept
{
    const double size=glm::length(normalBody);
    if (!(size>0.99 && size<1.01) || !std::isfinite(size) ||
        !assistedCombinedDemandFeasible(model, concurrentDemand))
        return 0.0;

    std::array<double,6> proposed=concurrentDemand;
    double hi=0.0;
    for (std::size_t i=0;i<3;++i)
    {
        const double axis=normalBody[static_cast<int>(i)];
        if (std::abs(axis)<1.0e-9)
            continue;
        const double headroom=(axis>0.0
            ? limits.positive[i] : limits.negative[i]);
        if (!(headroom>0.0))
            return 0.0;
        hi+=headroom*headroom;
    }
    hi=std::sqrt(hi);
    if (!(hi>0.0) || !std::isfinite(hi))
        return 0.0;

    double lo=0.0;
    for (int iteration=0;iteration<24;++iteration)
    {
        const double mid=(lo+hi)*0.5;
        for(std::size_t i=0;i<3;++i)
            proposed[i]=concurrentDemand[i]+
                mid*normalBody[static_cast<int>(i)];
        if (assistedCombinedDemandFeasible(model,proposed))
            lo=mid;
        else
            hi=mid;
    }
    return lo;
}

} // namespace game::ship::telemetry
