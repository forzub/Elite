#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/ship/core/ShipParams.h"

namespace game::ship
{

inline constexpr double StandardGravityMps2 = 9.80665;

[[nodiscard]] inline bool validShipDynamics(
    const ShipParams& params
) noexcept
{
    const auto nonNegativeFinite = [](double value) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    };
    const auto positiveFinite = [](double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    };
    const auto physicalGimbalAngle = [](double value) noexcept
    {
        constexpr double HalfPi =
            1.5707963267948966192313216916398;
        return
            std::isfinite(value) &&
            value >= 0.0 &&
            value < HalfPi;
    };

    return
        nonNegativeFinite(params.maxPitchRate) &&
        nonNegativeFinite(params.maxYawRate) &&
        nonNegativeFinite(params.maxRollRate) &&
        nonNegativeFinite(params.angularAccel) &&
        nonNegativeFinite(params.angularDamping) &&
        nonNegativeFinite(params.maxCombatSpeed) &&
        nonNegativeFinite(params.maxCruiseSpeed) &&
        nonNegativeFinite(params.throttleAccel) &&
        nonNegativeFinite(params.stopSpeedEpsilonMps) &&
        std::isfinite(params.brakeAlignmentCosine) &&
        params.brakeAlignmentCosine >= -1.0f &&
        params.brakeAlignmentCosine <= 1.0f &&
        nonNegativeFinite(params.assistedMinimumTargetSpeedChangeRateMps2) &&
        nonNegativeFinite(
            params.assistedTargetSpeedChangeRateFractionPerSecond
        ) &&
        nonNegativeFinite(params.fallbackThrottleResponsePerSecond) &&
        nonNegativeFinite(params.forwardMainEngineAccelerationMps2) &&
        nonNegativeFinite(params.reverseMainEngineAccelerationMps2) &&
        physicalGimbalAngle(params.mainGimbalPitchLimitRad) &&
        physicalGimbalAngle(params.mainGimbalYawLimitRad) &&
        nonNegativeFinite(params.mainGimbalRateRadPerSec) &&
        nonNegativeFinite(params.autoLevelStrength) &&
        nonNegativeFinite(params.strafeAccel) &&
        nonNegativeFinite(params.strafeDamping) &&
        nonNegativeFinite(params.maxStrafeSpeed) &&
        nonNegativeFinite(params.manoeuvreThrusterAccel) &&
        nonNegativeFinite(params.manoeuvreGasUsePerSecond) &&
        nonNegativeFinite(params.manoeuvreGasRechargePerSecond) &&
        std::isfinite(params.manoeuvreGasRestartFraction) &&
        params.manoeuvreGasRestartFraction >= 0.0f &&
        params.manoeuvreGasRestartFraction <= 1.0f &&
        nonNegativeFinite(params.maxGs) &&
        nonNegativeFinite(params.maxLinearGs) &&
        nonNegativeFinite(params.turnRadius) &&
        positiveFinite(params.massKg) &&
        positiveFinite(params.pitchInertiaKgM2) &&
        positiveFinite(params.yawInertiaKgM2) &&
        positiveFinite(params.rollInertiaKgM2);
}

[[nodiscard]] inline double mainGimbalPitchLimitRad(
    const ShipParams& params
) noexcept
{
    return std::max(
        0.0,
        static_cast<double>(params.mainGimbalPitchLimitRad)
    );
}

[[nodiscard]] inline double mainGimbalYawLimitRad(
    const ShipParams& params
) noexcept
{
    return std::max(
        0.0,
        static_cast<double>(params.mainGimbalYawLimitRad)
    );
}

[[nodiscard]] inline double mainGimbalRateRadPerSec(
    const ShipParams& params
) noexcept
{
    return std::max(
        0.0,
        static_cast<double>(params.mainGimbalRateRadPerSec)
    );
}

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

[[nodiscard]] inline double installedDirectionalMainAccelerationMps2(
    float configuredAccelerationMps2,
    const ShipParams& params
) noexcept
{
    const double loadLimit = mainAccelerationLimitMps2(params);
    const double installed = std::max(
        0.0,
        static_cast<double>(configuredAccelerationMps2)
    );

    // Existing descriptors that predate explicit directional ratings retain
    // their old main-engine authority. New descriptors should specify the
    // installed rating explicitly.
    return installed > 0.0
        ? std::min(installed, loadLimit)
        : loadLimit;
}

[[nodiscard]] inline double forwardMainAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    return params.forwardMainEngineAvailable
        ? installedDirectionalMainAccelerationMps2(
              params.forwardMainEngineAccelerationMps2,
              params
          )
        : 0.0;
}

[[nodiscard]] inline double reverseMainAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    return params.reverseMainEngineAvailable
        ? installedDirectionalMainAccelerationMps2(
              params.reverseMainEngineAccelerationMps2,
              params
          )
        : 0.0;
}

[[nodiscard]] inline double
newtonianMainVectoringLateralAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    const double main =
        forwardMainAccelerationLimitMps2(params);
    if (main <= 0.0)
        return 0.0;

    // Continuous Newtonian curvature without a hull flip is limited by the
    // main-nozzle deflection itself, not by the full aft-main magnitude.
    // Use the weaker of pitch/yaw gimbal envelopes as the 3-D route budget.
    const double gimbal =
        std::min(
            mainGimbalPitchLimitRad(params),
            mainGimbalYawLimitRad(params)
        );

    return main * std::sin(gimbal);
}

[[nodiscard]] inline double manoeuvreAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    const double dedicated = std::max(
        0.0,
        static_cast<double>(params.manoeuvreThrusterAccel)
    );

    // A migrated physical-control model may explicitly have no manoeuvre
    // thrusters at all (Assisted). Zero is therefore a real hardware fact,
    // not a request to resurrect the legacy strafeAccel fallback.
    if (params.manoeuvreThrusterAuthorityExplicit)
        return dedicated;

    if (dedicated > 0.0)
        return dedicated;

    // Compatibility only for descriptors not yet migrated to the dedicated
    // RCS field. This fallback lives in ONE place so navigation/physics do not
    // invent independent interpretations of ShipParams.
    return std::max(0.0, static_cast<double>(params.strafeAccel));
}

// Whether the signed per-axis RCS authority is known (published by the
// authoritative control telemetry).
[[nodiscard]] inline bool newtonianRcsPerAxisKnown(
    const ShipParams& params
) noexcept
{
    const auto& p = params.newtonianRcsPositiveMps2;
    const auto& n = params.newtonianRcsNegativeMps2;
    return p.x + p.y + p.z + n.x + n.y + n.z > 1.0e-9;
}

// RCS translation authority in one body direction: axis 0 = right,
// 1 = up, 2 = forward (nose, body -Z); `component` gives the sign. A
// direction can be genuinely zero (lost clusters). Unknown per-axis data
// falls back to the scalar all-directions bound.
[[nodiscard]] inline double newtonianRcsDirectionAccelerationMps2(
    const ShipParams& params,
    int axis,
    double component
) noexcept
{
    if (!newtonianRcsPerAxisKnown(params))
        return manoeuvreAccelerationLimitMps2(params);
    const auto& p = params.newtonianRcsPositiveMps2;
    const auto& n = params.newtonianRcsNegativeMps2;
    switch (axis)
    {
        case 0: return component >= 0.0 ? p.x : n.x;
        case 1: return component >= 0.0 ? p.y : n.y;
        default: return component >= 0.0 ? n.z : p.z;
    }
}

[[nodiscard]] inline double assistedLateralStabilizationAccelerationLimitMps2(
    const ShipParams& params
) noexcept
{
    // Assisted lateral stabilization is a distinct automatic actuator budget.
    // The legacy strafeAccel field is the ship-profile aggregate authority for
    // that controller (e.g. vectoring + automatic translation thrusters).
    // Manual keypad RCS continues to use manoeuvreThrusterAccel and its gas
    // accumulator. Both remain bounded by the common linear-load envelope.
    const double configured = std::max(
        manoeuvreAccelerationLimitMps2(params),
        std::max(0.0, static_cast<double>(params.strafeAccel))
    );
    return std::min(
        configured,
        mainAccelerationLimitMps2(params)
    );
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
