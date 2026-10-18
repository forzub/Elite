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

    // Thrust damage is loss of force authority, NOT an identical percentage
    // loss in conversion efficiency. Near-ideal field drives need a separate
    // modest conversion-loss law to avoid gigawatt waste heat at 20% wear.
    double maxRelativeEfficiencyLossAtFullDamage01 = 0.002;

    // Universal engine-only life passport. Thermal/radiation/duration inputs
    // are physical rates converted into the same direct damage percentages.
    // The ship's reactor and cooling are IDEAL during this phase, but an
    // engine retains finite thermal inertia/conductance to its coolant.
    double nominalCoolantTemperatureK = 1050.0;
    double nominalOperatingTemperatureK = 1200.0;
    double safeOperatingTemperatureK = 1300.0;
    double thermalCapacityMJperK = 12.0;
    double thermalConductanceMWperK = 0.0;
    double overheatReferenceExcessK = 100.0;
    double thermalDamagePerHourAtReferenceExcess01 = 0.05;

    double overhaulEquivalentHours = 30000.0;
    double endOfLifeEquivalentHours = 50000.0;
    double damageAtOverhaul01 = 0.20;
    double damageAtEndOfLife01 = 0.95;
    double wearThrottleExponent = 1.5;

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

    const double conversionLoss =
        std::clamp(
            positiveFiniteOr(
                descriptor.maxRelativeEfficiencyLossAtFullDamage01, 0.0
            ), 0.0, 0.95
        ) * totalDamage01;
    out.efficiency01 = out.thrustFailed
        ? 0.0
        : nominalEfficiency * (1.0 - conversionLoss);

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

    // Damaged force authority caps maximum thrust independently of the
    // small conversion-efficiency loss. For a given DELIVERED force, input
    // rises by eta_nominal / eta_effective, without turning every percent
    // of mechanical damage into one percent of energetic efficiency loss.
    out.powerCostMultiplier =
        out.efficiency01 > 1.0e-9
            ? nominalEfficiency / out.efficiency01
            : 0.0;

    // Extra waste-heat fraction relative to the healthy same-output heat.
    // The denominator must be finite even for eta_nominal close to 1.
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

// Operating history belongs to each INSTALLED engine module, not the
// ShipDescriptor and not a propulsion slot. These values are independent
// from accumulated functional damage, which remains in EquipmentDamageState.
struct EngineOperatingState
{
    double temperatureK = 0.0;
    double equivalentOperatingHours = 0.0;
};

struct EngineLifeStep
{
    EngineOperatingState next;
    double wearDamageIncrement01 = 0.0;
    double thermalDamageIncrement01 = 0.0;
    double dissipatedHeatMW = 0.0;
};

inline double engineWearDamageAtHours(
    const EngineUnitDescriptor& descriptor,
    double hours
) noexcept
{
    if (!std::isfinite(hours) || hours <= 0.0)
        return 0.0;

    const double overhaul =
        std::max(1.0, descriptor.overhaulEquivalentHours);
    const double end =
        std::max(overhaul + 1.0, descriptor.endOfLifeEquivalentHours);
    const double atOverhaul =
        std::clamp(descriptor.damageAtOverhaul01, 0.0, 1.0);
    const double atEnd =
        std::clamp(descriptor.damageAtEndOfLife01, atOverhaul, 1.0);

    if (hours <= overhaul)
        return atOverhaul * (hours / overhaul);

    const double progress =
        std::clamp((hours - overhaul) / (end - overhaul), 0.0, 1.0);
    return atOverhaul +
        (atEnd - atOverhaul) * progress * progress;
}

inline EngineLifeStep advanceEngineOperatingState(
    const EngineUnitDescriptor& descriptor,
    const EngineUnitEffectiveState& current,
    const EngineOperatingState& previous,
    double deliveredNominalThrust01,
    double deltaSeconds,
    double coolantTemperatureK
) noexcept
{
    EngineLifeStep step;
    step.next = previous;

    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
        return step;

    const double delivered = std::min(
        std::clamp(deliveredNominalThrust01, 0.0, 1.0),
        std::clamp(current.efficiencyAuthority01, 0.0, 1.0)
    );
    const double coolant =
        std::isfinite(coolantTemperatureK) && coolantTemperatureK > 0.0
            ? coolantTemperatureK
            : descriptor.nominalCoolantTemperatureK;

    // Wear is based on DELIVERED force: no time accumulation for a failed
    // engine or for an installed but inactive fore/aft main engine.
    const double equivalentDeltaHours =
        (deltaSeconds / 3600.0) *
        std::pow(delivered,
                 std::max(0.0, descriptor.wearThrottleExponent));
    const double beforeHours =
        std::max(0.0, previous.equivalentOperatingHours);
    step.next.equivalentOperatingHours =
        beforeHours + equivalentDeltaHours;
    step.wearDamageIncrement01 = std::max(
        0.0,
        engineWearDamageAtHours(
            descriptor, step.next.equivalentOperatingHours
        ) - engineWearDamageAtHours(descriptor, beforeHours)
    );

    const double heat =
        generatedWasteHeatMW(descriptor, current, delivered);
    step.dissipatedHeatMW = std::max(0.0, heat);

    const double capacity =
        std::max(1.0e-9, descriptor.thermalCapacityMJperK);
    const double conductance =
        std::max(1.0e-9, descriptor.thermalConductanceMWperK);
    const double startT =
        std::isfinite(previous.temperatureK) &&
        previous.temperatureK > 0.0
            ? previous.temperatureK
            : coolant;
    const double equilibriumT = coolant + heat / conductance;
    const double decay =
        std::exp(-conductance * deltaSeconds / capacity);
    step.next.temperatureK =
        equilibriumT + (startT - equilibriumT) * decay;

    const auto heatDamageRate = [&](double temperatureK)
    {
        const double excess =
            std::max(0.0,
                temperatureK - descriptor.safeOperatingTemperatureK);
        const double normalized =
            excess / std::max(1.0, descriptor.overheatReferenceExcessK);
        return std::max(
            0.0,
            descriptor.thermalDamagePerHourAtReferenceExcess01
        ) * normalized * normalized;
    };

    // Trapezoidal integration: stable even at the coarser systems cadence.
    step.thermalDamageIncrement01 =
        (deltaSeconds / 3600.0) * 0.5 *
        (heatDamageRate(startT) +
         heatDamageRate(step.next.temperatureK));
    return step;
}


} // namespace game::equipment::engine
