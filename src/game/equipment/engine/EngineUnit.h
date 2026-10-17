#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/equipment/EquipmentDamage.h"

namespace game::equipment::engine
{

using EngineDamageInputs = game::equipment::EquipmentDamageState;
using EngineDamageWeights = game::equipment::EquipmentDamageSensitivity;

struct EngineUnitDescriptor
{
    double nominalThrustNewtons = 0.0;

    // Electrical input required to sustain nominal full thrust at healthy
    // efficiency. Keep at zero when the ship's energy budget is not authored.
    double nominalElectricalPowerMW = 0.0;

    // Healthy overall conversion efficiency, 0..1. This is intentionally an
    // abstract gameplay-level efficiency rather than a detailed rocket/jet
    // thermodynamic cycle model.
    double nominalEfficiency01 = 1.0;

    double nominalGimbalRangeRad = 0.0;
    double nominalGimbalRateRadPerSec = 0.0;

    // Primary functional sensitivity (efficiency) belongs to the common
    // EquipmentDamageLayer. The engine adds one specialized mechanical
    // response because nozzle vectoring is a distinct useful function.
    EngineDamageWeights gimbalDamage {
        0.15, // laser
        0.65, // impact
        0.05, // thermal
        0.00, // radiation
        0.15  // wear
    };

    // Lowest surviving efficiency before the unit is considered unable to
    // sustain useful thrust. Avoids singular power demand as efficiency -> 0.
    double minimumOperationalEfficiency01 = 0.05;
};

struct EngineUnitEffectiveState
{
    bool operational = true;

    double efficiency01 = 1.0;
    double efficiencyAuthority01 = 1.0;

    double gimbalAuthority01 = 1.0;

    double maximumThrustNewtons = 0.0;
    double gimbalRangeRad = 0.0;
    double gimbalRateRadPerSec = 0.0;

    // Multipliers relative to the healthy unit for the same generated thrust.
    double powerCostMultiplier = 1.0;
    double heatMultiplier = 0.0;
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
    const game::equipment::EquipmentDamageLayer& damage
) noexcept
{
    EngineUnitEffectiveState out;

    const double efficiencyDamage01 =
        game::equipment::weightedFunctionalDamage01(damage);

    game::equipment::EquipmentDamageLayer gimbalLayer = damage;
    gimbalLayer.sensitivity = descriptor.gimbalDamage;
    const double gimbalDamage01 =
        game::equipment::weightedFunctionalDamage01(gimbalLayer);

    out.efficiencyAuthority01 =
        std::clamp(1.0 - efficiencyDamage01, 0.0, 1.0);
    out.gimbalAuthority01 =
        std::clamp(1.0 - gimbalDamage01, 0.0, 1.0);

    const double nominalEfficiency =
        std::clamp(
            positiveFiniteOr(descriptor.nominalEfficiency01, 1.0),
            1.0e-6,
            1.0
        );

    out.efficiency01 =
        nominalEfficiency * out.efficiencyAuthority01;

    const double minimumOperationalEfficiency =
        std::clamp(
            positiveFiniteOr(
                descriptor.minimumOperationalEfficiency01,
                0.05
            ),
            0.0,
            1.0
        );

    out.operational =
        out.efficiencyAuthority01 > minimumOperationalEfficiency;

    if (!out.operational)
    {
        out.maximumThrustNewtons = 0.0;
        out.gimbalRangeRad = 0.0;
        out.gimbalRateRadPerSec = 0.0;
        out.powerCostMultiplier = 0.0;
        out.heatMultiplier = 0.0;
        return out;
    }

    // With fixed available input power, useful output scales with efficiency.
    out.maximumThrustNewtons =
        positiveFiniteOr(
            descriptor.nominalThrustNewtons,
            0.0
        ) * out.efficiencyAuthority01;

    out.gimbalRangeRad =
        positiveFiniteOr(
            descriptor.nominalGimbalRangeRad,
            0.0
        ) * out.gimbalAuthority01;
    out.gimbalRateRadPerSec =
        positiveFiniteOr(
            descriptor.nominalGimbalRateRadPerSec,
            0.0
        ) * out.gimbalAuthority01;

    // For the same generated useful thrust, lower efficiency requires more
    // electrical input. Waste heat is derived from input minus useful output.
    out.powerCostMultiplier =
        out.efficiencyAuthority01 > 1.0e-9
            ? 1.0 / out.efficiencyAuthority01
            : 0.0;

    out.heatMultiplier =
        out.efficiencyAuthority01 > 1.0e-9
            ? (1.0 / out.efficiencyAuthority01) - 1.0
            : 0.0;

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

    const double inputMW =
        requestedElectricalPowerMW(
            descriptor,
            state,
            generatedThrust01
        );

    const double usefulHealthyMW =
        positiveFiniteOr(
            descriptor.nominalElectricalPowerMW,
            0.0
        ) *
        finiteClamp01(generatedThrust01) *
        std::clamp(
            positiveFiniteOr(descriptor.nominalEfficiency01, 1.0),
            0.0,
            1.0
        );

    return std::max(0.0, inputMW - usefulHealthyMW);
}

} // namespace game::equipment::engine
