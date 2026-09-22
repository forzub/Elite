#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/ship/core/ShipParams.h"

namespace game::ship
{

inline constexpr double StandardGravityMps2 = 9.80665;

[[nodiscard]] inline double effectiveLinearGs(
    const ShipParams& params
) noexcept
{
    return std::max(
        0.0,
        params.maxLinearGs > 0.0f
            ? static_cast<double>(params.maxLinearGs)
            : static_cast<double>(params.maxGs)
    );
}

[[nodiscard]] inline double mainAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    return effectiveLinearGs(params) * StandardGravityMps2;
}

[[nodiscard]] inline double forwardMainAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    return params.forwardMainEngineAvailable
        ? mainAccelerationLimitMps2(params)
        : 0.0;
}

[[nodiscard]] inline double reverseMainAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    return params.reverseMainEngineAvailable
        ? mainAccelerationLimitMps2(params)
        : 0.0;
}

[[nodiscard]] inline double manoeuvreAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    const double dedicated =
        static_cast<double>(params.manoeuvreThrusterAccel);
    if (dedicated > 0.0)
        return dedicated;

    // Compatibility only for descriptors not yet migrated to the dedicated
    // RCS field. This fallback lives in ONE place so navigation/physics do not
    // invent independent interpretations of ShipParams.
    return std::max(0.0, static_cast<double>(params.strafeAccel));
}

[[nodiscard]] inline double controlledSpeedLimitMps(
    const ShipParams& params
) noexcept
{
    return std::max(0.0, static_cast<double>(params.maxCombatSpeed));
}

[[nodiscard]] inline double maximumAngularSpeedRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::max({
        0.0,
        static_cast<double>(params.maxPitchRate),
        static_cast<double>(params.maxYawRate),
        static_cast<double>(params.maxRollRate)
    });
}

[[nodiscard]] inline double angularAccelerationLimitRadPerSec2(
    const ShipParams& params
) noexcept
{
    return std::max(0.0, static_cast<double>(params.angularAccel));
}

} // namespace game::ship
