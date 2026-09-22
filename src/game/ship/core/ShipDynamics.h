#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

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

[[nodiscard]] inline double angularLoadRateLimitRadPerSec(
    const ShipParams& params
) noexcept
{
    if (params.maxGs <= 0.0f || params.turnRadius <= 0.0f)
        return std::numeric_limits<double>::infinity();

    return std::sqrt(
        std::max(0.0, static_cast<double>(params.maxGs)) *
        StandardGravityMps2 /
        static_cast<double>(params.turnRadius)
    );
}

[[nodiscard]] inline double angularAccelerationLimitRadPerSec2(
    const ShipParams& params
) noexcept
{
    const double configured =
        std::max(0.0, static_cast<double>(params.angularAccel));

    if (params.maxGs <= 0.0f || params.turnRadius <= 0.0f)
        return configured;

    const double byLinearLoad =
        std::max(0.0, static_cast<double>(params.maxGs)) *
        StandardGravityMps2 /
        static_cast<double>(params.turnRadius);

    return std::min(configured, byLinearLoad);
}

[[nodiscard]] inline double pitchRateLimitRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::min(
        std::max(0.0, static_cast<double>(params.maxPitchRate)),
        angularLoadRateLimitRadPerSec(params)
    );
}

[[nodiscard]] inline double yawRateLimitRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::min(
        std::max(0.0, static_cast<double>(params.maxYawRate)),
        angularLoadRateLimitRadPerSec(params)
    );
}

[[nodiscard]] inline double rollRateLimitRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::min(
        std::max(0.0, static_cast<double>(params.maxRollRate)),
        angularLoadRateLimitRadPerSec(params)
    );
}

[[nodiscard]] inline double maximumAngularSpeedRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::max({
        pitchRateLimitRadPerSec(params),
        yawRateLimitRadPerSec(params),
        rollRateLimitRadPerSec(params)
    });
}

} // namespace game::ship
