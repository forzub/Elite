#pragma once

#include <algorithm>
#include <cmath>

namespace game::equipment::engine
{

// Immutable functional passport of one propulsion unit.
//
// Damage remains one scalar health value in the owning module runtime.
// This descriptor only defines how that health loss degrades the engine's
// useful functions and operating cost.
struct EngineUnitDescriptor
{
    // Healthy maximum thrust of this one physical engine.
    double nominalThrustNewtons = 0.0;

    // Nominal electrical/thermal cost at full generated thrust. These are
    // equipment ratings; the actuator allocator supplies actual throttle.
    double nominalElectricalPowerMW = 0.0;
    double nominalWasteHeatMW = 0.0;

    // Healthy TVC/gimbal capability. PhysicalControlModel still owns the
    // geometric slot position and the ship-level hard gimbal envelope.
    double nominalGimbalRangeRad = 0.0;
    double nominalGimbalRateRadPerSec = 0.0;

    // Damage response exponents. 1.0 is linear. Values >1 make a function
    // degrade faster as health falls; values <1 make it more tolerant.
    double thrustHealthExponent = 1.0;
    double vectoringHealthExponent = 1.35;

    // Extra operating cost at zero health, interpolated by damage=(1-health).
    // Example 0.50 => at severe damage the surviving useful thrust may require
    // up to 1.5x nominal electrical power per unit of generated thrust.
    double maxPowerCostPenalty = 0.50;
    double maxHeatPenalty = 1.00;
};

struct EngineUnitEffectiveState
{
    bool operational = true;
    double health01 = 1.0;

    // Maximum functional authority relative to the healthy unit.
    double thrustAvailable01 = 1.0;
    double maximumThrustNewtons = 0.0;
    double vectoringAvailable01 = 1.0;
    double gimbalRangeRad = 0.0;
    double gimbalRateRadPerSec = 0.0;

    // Multipliers applied to nominal cost for ACTUAL generated thrust.
    // A dead engine has no generated-thrust demand, so these multipliers are
    // irrelevant when operational=false.
    double powerCostMultiplier = 1.0;
    double heatMultiplier = 1.0;
};

inline double finiteClamp01(double value) noexcept
{
    return std::isfinite(value)
        ? std::clamp(value, 0.0, 1.0)
        : 0.0;
}

inline double positiveFiniteOr(
    double value,
    double fallback
) noexcept
{
    return std::isfinite(value) && value >= 0.0
        ? value
        : fallback;
}

[[nodiscard]] inline EngineUnitEffectiveState evaluateEngineUnit(
    const EngineUnitDescriptor& descriptor,
    double health01
) noexcept
{
    EngineUnitEffectiveState out;
    out.health01 = finiteClamp01(health01);

    if (out.health01 <= 1.0e-9)
    {
        out.operational = false;
        out.thrustAvailable01 = 0.0;
        out.maximumThrustNewtons = 0.0;
        out.vectoringAvailable01 = 0.0;
        out.gimbalRangeRad = 0.0;
        out.gimbalRateRadPerSec = 0.0;
        out.powerCostMultiplier = 1.0;
        out.heatMultiplier = 1.0;
        return out;
    }

    const double thrustExponent =
        std::max(
            0.01,
            positiveFiniteOr(
                descriptor.thrustHealthExponent,
                1.0
            )
        );
    const double vectoringExponent =
        std::max(
            0.01,
            positiveFiniteOr(
                descriptor.vectoringHealthExponent,
                1.35
            )
        );

    out.thrustAvailable01 =
        std::pow(out.health01, thrustExponent);
    out.vectoringAvailable01 =
        std::pow(out.health01, vectoringExponent);
    out.maximumThrustNewtons =
        positiveFiniteOr(
            descriptor.nominalThrustNewtons,
            0.0
        ) * out.thrustAvailable01;

    out.gimbalRangeRad =
        positiveFiniteOr(
            descriptor.nominalGimbalRangeRad,
            0.0
        ) * out.vectoringAvailable01;
    out.gimbalRateRadPerSec =
        positiveFiniteOr(
            descriptor.nominalGimbalRateRadPerSec,
            0.0
        ) * out.vectoringAvailable01;

    const double damage01 = 1.0 - out.health01;
    out.powerCostMultiplier =
        1.0 +
        damage01 *
            positiveFiniteOr(
                descriptor.maxPowerCostPenalty,
                0.0
            );
    out.heatMultiplier =
        1.0 +
        damage01 *
            positiveFiniteOr(
                descriptor.maxHeatPenalty,
                0.0
            );

    return out;
}

[[nodiscard]] inline double requestedElectricalPowerMW(
    const EngineUnitDescriptor& descriptor,
    const EngineUnitEffectiveState& state,
    double generatedThrust01
) noexcept
{
    if (!state.operational)
        return 0.0;

    return
        positiveFiniteOr(
            descriptor.nominalElectricalPowerMW,
            0.0
        ) *
        finiteClamp01(generatedThrust01) *
        state.powerCostMultiplier;
}

[[nodiscard]] inline double generatedWasteHeatMW(
    const EngineUnitDescriptor& descriptor,
    const EngineUnitEffectiveState& state,
    double generatedThrust01
) noexcept
{
    if (!state.operational)
        return 0.0;

    return
        positiveFiniteOr(
            descriptor.nominalWasteHeatMW,
            0.0
        ) *
        finiteClamp01(generatedThrust01) *
        state.heatMultiplier;
}

} // namespace game::equipment::engine
