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

    // Primary thrust failure policy belongs to the common EquipmentDamageLayer.
    // The engine adds one specialized mechanical function because nozzle
    // vectoring can fail independently while axial thrust remains available.
    EngineDamageWeights gimbalDamage {
        0.15, // laser
        0.65, // impact
        0.05, // thermal
        0.00, // radiation
        0.15  // wear
    };
    game::equipment::EquipmentFailureProfile gimbalFailure {0.95};

    // Above this primary-damage level the thrust chamber can no longer hold
    // steady output. Instability grows continuously until the authored thrust
    // failure threshold is reached.
    double irregularThrustStartsAtDamage01 = 0.70;
};

struct EngineUnitEffectiveState
{
    bool thrustFailed = false;
    bool gimbalFailed = false;

    double efficiency01 = 1.0;
    double efficiencyAuthority01 = 1.0;

    double gimbalAuthority01 = 1.0;

    // 0 = steady exhaust, 1 = maximum pre-failure intermittency.
    double thrustIrregularity01 = 0.0;

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

    const auto thrustFunction =
        game::equipment::evaluateEquipmentFunction(damage);

    game::equipment::EquipmentDamageLayer gimbalLayer = damage;
    gimbalLayer.sensitivity = descriptor.gimbalDamage;
    gimbalLayer.failure = descriptor.gimbalFailure;
    const auto gimbalFunction =
        game::equipment::evaluateEquipmentFunction(gimbalLayer);

    out.thrustFailed = thrustFunction.failed;
    out.gimbalFailed = gimbalFunction.failed;
    out.efficiencyAuthority01 = thrustFunction.authority01;
    out.gimbalAuthority01 = gimbalFunction.authority01;

    const double instabilityStart =
        std::clamp(
            positiveFiniteOr(
                descriptor.irregularThrustStartsAtDamage01,
                0.70
            ),
            0.0,
            1.0
        );
    const double thrustFailureDamage =
        std::clamp(
            damage.failure.failureAtDamage01,
            instabilityStart,
            1.0
        );

    if (!out.thrustFailed &&
        thrustFunction.damage01 > instabilityStart &&
        thrustFailureDamage > instabilityStart + 1.0e-12)
    {
        out.thrustIrregularity01 =
            std::clamp(
                (thrustFunction.damage01 - instabilityStart) /
                    (thrustFailureDamage - instabilityStart),
                0.0,
                1.0
            );
    }

    const double nominalEfficiency =
        std::clamp(
            positiveFiniteOr(descriptor.nominalEfficiency01, 1.0),
            1.0e-6,
            1.0
        );

    out.efficiency01 = out.thrustFailed
        ? 0.0
        : nominalEfficiency * out.efficiencyAuthority01;

    // A functional thrust failure does not delete the engine module. It zeros
    // useful thrust, electrical demand and generated waste heat. The TVC
    // mechanism remains an independently evaluated function.
    out.maximumThrustNewtons = out.thrustFailed
        ? 0.0
        : positiveFiniteOr(
              descriptor.nominalThrustNewtons,
              0.0
          ) * out.efficiencyAuthority01;

    out.gimbalRangeRad = out.gimbalFailed
        ? 0.0
        : positiveFiniteOr(
              descriptor.nominalGimbalRangeRad,
              0.0
          ) * out.gimbalAuthority01;
    out.gimbalRateRadPerSec = out.gimbalFailed
        ? 0.0
        : positiveFiniteOr(
              descriptor.nominalGimbalRateRadPerSec,
              0.0
          ) * out.gimbalAuthority01;

    if (out.thrustFailed)
    {
        out.powerCostMultiplier = 0.0;
        out.heatMultiplier = 0.0;
        return out;
    }

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
    if (state.thrustFailed)
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
    if (state.thrustFailed)
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
