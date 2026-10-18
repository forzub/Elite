#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/equipment/EquipmentDamage.h"

namespace game::equipment::engine
{

using EngineDamageInputs = game::equipment::EquipmentDamageState;

// Field coupling is a fictional propulsion mechanism: no reaction mass is
// expelled by the ship. Conservation of momentum therefore presupposes an
// external field/momentum reservoir, which is not a known real drive.
enum class PropulsionPrinciple
{
    FieldCoupling,
    ReactionMass
};

struct EngineUnitDescriptor
{
    PropulsionPrinciple propulsionPrinciple =
        PropulsionPrinciple::FieldCoupling;

    double nominalThrustNewtons = 0.0;

    // Installed electric input at nominal full thrust, in MW. For the
    // field-coupled Cobra this rating is F_nominal * designSpeed / eta;
    // it is a DESIGN-POINT budget, not an assertion that reactionless thrust
    // is possible under known physics. No fuel/propellant mass flow here.
    double nominalElectricalPowerMW = 0.0;
    double designReferenceSpeedMps = 0.0;

    // Effective conversion into the hypothetical field coupling at design
    // speed. A damaged engine produces less thrust from the same rated input.
    double nominalEfficiency01 = 1.0;

    double nominalGimbalRangeRad = 0.0;
    double nominalGimbalRateRadPerSec = 0.0;

    // SAME total five-cause percentage drives both functions, with distinct
    // output/failure curves, never separate per-cause sensitivity weights.
    // Nozzle control may fail at 90% while axial thrust survives until 95%.
    game::equipment::EquipmentFailureProfile gimbalFailure {0.90};

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

    const double totalDamage01 =
        game::equipment::totalAccumulatedDamage01(damage.accumulated);
    const auto thrustFunction =
        game::equipment::evaluateEquipmentFunctionAtDamage01(
            totalDamage01, damage.failure
        );
    const auto gimbalFunction =
        game::equipment::evaluateEquipmentFunctionAtDamage01(
            totalDamage01, descriptor.gimbalFailure
        );

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

    // For the same achieved fraction of HEALTHY nominal thrust, degradation
    // demands more input power (if the rated supply is available). Use the
    // achieved fraction as requestedElectricalPowerMW's input, not the
    // percentage of the reduced damaged-engine maximum.
    // Waste heat is rated input minus remaining useful field-coupling output.
    out.powerCostMultiplier =
        out.efficiencyAuthority01 > 1.0e-9
            ? 1.0 / out.efficiencyAuthority01
            : 0.0;

    // BITE heatMultiplier is the EXTRA fraction of waste heat compared
    // with a healthy unit at the SAME delivered thrust, not the absolute
    // heat-to-useful-work ratio. At 80% nominal conversion and 50% remaining
    // authority: input doubles for the same useful output; waste heat goes
    // from 0.2 to 1.2 units of healthy rated input (+500%).
    const double healthyWasteFraction =
        1.0 - nominalEfficiency;
    out.heatMultiplier = healthyWasteFraction > 1.0e-9
        ? std::max(
              0.0,
              (out.powerCostMultiplier - 1.0) /
                  healthyWasteFraction
          )
        : std::max(0.0, out.powerCostMultiplier - 1.0);

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

    // Fraction of the HEALTHY nominal thrust actually delivered. Never bill
    // for a force above the damaged engine's physical available maximum.
    const double delivered01 = std::min(
        finiteClamp01(generatedThrust01),
        finiteClamp01(state.efficiencyAuthority01)
    );
    return
        positiveFiniteOr(descriptor.nominalElectricalPowerMW, 0.0) *
        delivered01 *
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

    const double delivered01 = std::min(
        finiteClamp01(generatedThrust01),
        finiteClamp01(state.efficiencyAuthority01)
    );
    const double inputMW =
        requestedElectricalPowerMW(descriptor, state, delivered01);

    // At the reference operating point, nominal useful field-coupling
    // power is F * designSpeed. Energy beyond useful field work is dumped as
    // heat (temporarily sunk by the idealized cooling system).
    const double usefulMW =
        positiveFiniteOr(descriptor.nominalElectricalPowerMW, 0.0) *
        delivered01 *
        std::clamp(
            positiveFiniteOr(descriptor.nominalEfficiency01, 1.0),
            0.0,
            1.0
        );
    return std::max(0.0, inputMW - usefulMW);
}

} // namespace game::equipment::engine
