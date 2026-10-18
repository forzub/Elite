#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/physics/UniverseThermalRadiation.h"
#include "src/game/physics/UniverseMaterials.h"

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

// Installed nozzle-side hot emitter. Every engine type authors its own
// annulus and material; the universal Stefan-Boltzmann / gameplay multiplier
// is NOT copied here. A missing emitter (outer diameter zero) is valid.
struct EngineThermalEmitterDescriptor
{
    double outerDiameterM = 0.0;
    double apertureDiameterM = 0.0;
    double surfaceTemperatureK = 2500.0;
    double emissivity01 = 0.9;
    double viewFactor01 = 1.0;
    // Empty ID is backward-compatible legacy fixture. Production presets
    // resolve emissivity and damage resistance from the universe catalog.
    std::string_view materialId {};
    std::string_view meshPartId {};

    // The hot annulus is a replace-only consumable, NOT a repairable engine
    // subsystem. 8k hours is the replacement recommendation; continued use
    // wears it out at 12k equivalent full-power hours (gameplay values).
    double replaceAfterEquivalentHours = 8000.0;
    double exhaustedAfterEquivalentHours = 12000.0;
    double wearAtReplacement01 = 0.25;
    enum class ServicePolicy { ReplaceOnly };
    ServicePolicy servicePolicy = ServicePolicy::ReplaceOnly;

    // Five damage channels live in EngineEmitterOperatingState; laser and
    // kinetic hits affect geometry, wear ages the emitting surface, while
    // thermal/radiation are recorded with zero functional effect here.
};

struct EngineUnitDescriptor
{
    // Manufacturer/type identity is DATA, not an engine subclass or
    // duplicated ship-specific implementation. Future shipyards and markets
    // can enumerate presets using these stable IDs.
    std::string_view engineTypeId {};
    std::string_view engineDisplayName {};
    std::string_view manufacturerName {};

    PropulsionPrinciple propulsionPrinciple =
        PropulsionPrinciple::FieldCoupling;

    // SAME material passport for the thrust housing and nozzle/TVC hardware.
    // Material-specific impact/laser conversion is deliberately deferred;
    // functional damage percentages and actuator laws stay unchanged.
    std::string_view structureMaterialId {};

    double nominalThrustNewtons = 0.0;

    // INDEPENDENT rated input at full thrust, in MW. A field drive is a
    // science-fiction actuator; F*v is instantaneous mechanical work against
    // velocity, NOT a valid universal input-power rating for acceleration.
    // The reactor will eventually be sized to the SHIP demand.
    double nominalElectricalPowerMW = 0.0;

    // Fraction of rated electrical input NOT deposited as local waste heat.
    // For this fictional drive, useful field work is a budget convention.
    double nominalEfficiency01 = 1.0;

    // Thrust damage is loss of force authority, NOT an identical percentage
    // loss in conversion efficiency. Near-ideal field drives need a separate
    // modest conversion-loss law to avoid gigawatt waste heat at 20% wear.
    double maxRelativeEfficiencyLossAtFullDamage01 = 0.002;

    // Universal engine-only life passport. Thermal/radiation/duration inputs
    // are physical rates converted into the same direct damage percentages.
    // The ship's reactor and cooling are IDEAL during this phase, but an
    // engine retains finite thermal inertia/conductance to its coolant.
    EngineThermalEmitterDescriptor thermalEmitter {};

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

// Maximum MW that the engine's ONE-SIDED nozzle ring may radiate into
// open space. effectiveArea01 allows future *real* hit-volume damage to
// decrease the emitting surface. It does not create damage by itself.
[[nodiscard]] inline double ratedThermalEmitterPowerMW(
    const EngineUnitDescriptor& descriptor,
    double effectiveArea01 = 1.0
) noexcept
{
    const auto& emitter = descriptor.thermalEmitter;
    const double oneSidedArea =
        game::physics::thermal::annulusAreaM2(
            emitter.outerDiameterM,
            emitter.apertureDiameterM
        );
    if (!std::isfinite(effectiveArea01))
        return 0.0;

    return game::physics::thermal::radiatedPowerMW(
        oneSidedArea * std::clamp(effectiveArea01, 0.0, 1.0),
        emitter.surfaceTemperatureK,
        game::physics::materials::findMaterial(emitter.materialId)
            ? game::physics::materials::findMaterial(emitter.materialId)->emissivity01
            : emitter.emissivity01,
        emitter.viewFactor01
    );
}

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

    // Rated-input model for a fictional drive. Its useful field-work budget
    // is a descriptor conversion fraction, independent of ship velocity.
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

// A self-contained, replaceable hot-emitter unit mounted on one engine.
// Mechanical engine history survives replacing this consumable component.
struct EngineEmitterOperatingState
{
    game::physics::materials::SurfaceIntegrity surface {};
    game::equipment::EquipmentDamageState damage {};
    double equivalentOperatingHours = 0.0;
};

// Operating history belongs to each INSTALLED engine module, not the
// ShipDescriptor and not a propulsion slot. These values are independent
// from accumulated functional damage, which remains in EquipmentDamageState.
struct EngineOperatingState
{
    double temperatureK = 0.0;
    double equivalentOperatingHours = 0.0;

    // Sub-quantum damage accrual is retained locally. It becomes canonical
    // EquipmentDamage only when a measurable 0.001%-point increment is ready,
    // keeping expensive module revision/snapshot work out of every frame.
    double pendingWearDamage01 = 0.0;
    double pendingThermalDamage01 = 0.0;

    // The mounted third component has its own replace-only life/damage.
    EngineEmitterOperatingState emitter {};
};

// Only a hit resolved to the emitter surface may enter here. Callers supply
// actual incident MJ and footprint m2; never reinterpret DamageEvent.energy.
[[nodiscard]] inline game::physics::materials::SurfaceIntegrity
applyEngineEmitterExposure(
    const EngineUnitDescriptor& descriptor,
    game::physics::materials::SurfaceIntegrity previous,
    const game::physics::materials::SurfaceExposure& hit
) noexcept
{
    const auto* material = game::physics::materials::findMaterial(
        descriptor.thermalEmitter.materialId
    );
    if (!material)
        return previous;
    const double area = game::physics::thermal::annulusAreaM2(
        descriptor.thermalEmitter.outerDiameterM,
        descriptor.thermalEmitter.apertureDiameterM
    );
    return game::physics::materials::exposeSurface(
        *material, area, previous, hit
    );
}

// Effective emitting surface: holes/cracks from hits * gradual wear of the
// emissive ceramic. No double subtraction of Laser/Impact channel history.
[[nodiscard]] inline double effectiveEmitterArea01(
    const EngineOperatingState& operating
) noexcept
{
    return finiteClamp01(operating.emitter.surface.intactArea01) *
        (1.0 - finiteClamp01(operating.emitter.damage.wear01));
}

// Quasi-steady protective thrust cap with an IDEAL fixed coolant boundary.
// A damaged annulus cannot be replaced by an imaginary cooling system.
// This is independent from mechanical thrust and gimbal damage. For real
// transient thermal abuse (emergency override), the existing temperature
// integrator continues to calculate accumulated thermal damage.
[[nodiscard]] inline double safeEmitterThrustFraction01(
    const EngineUnitDescriptor& descriptor,
    const EngineUnitEffectiveState& effective,
    const EngineOperatingState& operating
) noexcept
{
    const auto* material = game::physics::materials::findMaterial(
        descriptor.thermalEmitter.materialId
    );
    if (!material || effective.thrustFailed)
        return effective.thrustFailed ? 0.0 : effective.efficiencyAuthority01;

    const double availability = finiteClamp01(effective.efficiencyAuthority01);
    if (availability <= 0.0)
        return 0.0;
    const double wasteAtAvailableMW =
        generatedWasteHeatMW(descriptor, effective, availability);
    const double wastePerHealthyThrustMW =
        wasteAtAvailableMW / availability;
    if (wastePerHealthyThrustMW <= 0.0)
        return availability;

    const double directRadiationMW = ratedThermalEmitterPowerMW(
        descriptor, effectiveEmitterArea01(operating)
    );
    const double safeBulkMW = std::max(
        0.0, descriptor.thermalConductanceMWperK *
            (descriptor.safeOperatingTemperatureK -
             descriptor.nominalCoolantTemperatureK)
    );
    const double steadySafe = std::min(
        availability,
        finiteClamp01(
            (directRadiationMW + safeBulkMW) / wastePerHealthyThrustMW
        )
    );

    // Gameplay drama without heavy simulation: the damaged engine still
    // delivers thrust when cool. At 50 K below housing's safe temperature,
    // protection progressively reduces maximum power. At the limit it has
    // reached the steady-state thermal ceiling. Temperature may briefly
    // overshoot during a hard burn (existing thermal damage handles this).
    const double currentTemperatureK =
        std::isfinite(operating.temperatureK) &&
        operating.temperatureK > 0.0
            ? operating.temperatureK
            : descriptor.nominalOperatingTemperatureK;
    constexpr double ProtectionBandK = 50.0;
    const double protection01 = std::clamp(
        (currentTemperatureK -
         (descriptor.safeOperatingTemperatureK - ProtectionBandK)) /
            ProtectionBandK,
        0.0, 1.0
    );
    return availability -
        (availability - steadySafe) * protection01;
}


struct EngineLifeStep
{
    EngineOperatingState next;
    double wearDamageIncrement01 = 0.0;
    double thermalDamageIncrement01 = 0.0;
    double emitterWearDamageIncrement01 = 0.0;
    // Full generated waste heat, directly radiated hot-emitter component and
    // remaining heat that must pass through the bulk engine into coolant.
    double dissipatedHeatMW = 0.0;
    double emitterRadiatedHeatMW = 0.0;
    double coolantHeatMW = 0.0;
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
    double coolantTemperatureK,
    double emitterEffectiveArea01 = 1.0
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
        delivered > 0.0
            ? (deltaSeconds / 3600.0) *
                  std::pow(
                      delivered,
                      std::max(0.0, descriptor.wearThrottleExponent)
                  )
            : 0.0;
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

    // The annulus has its OWN replacement interval and wear percentage.
    // Wear increases from operating hours, not elapsed idle time. Recording
    // thermal/radiation channels never consumes annulus surface capacity.
    if (game::physics::materials::findMaterial(
            descriptor.thermalEmitter.materialId))
    {
        const double beforeEmitterHours =
            std::max(0.0, previous.emitter.equivalentOperatingHours);
        step.next.emitter.equivalentOperatingHours =
            beforeEmitterHours + equivalentDeltaHours;
        const auto emitterWearAt = [&](double hours)
        {
            const double service = std::max(
                1.0, descriptor.thermalEmitter.replaceAfterEquivalentHours
            );
            const double exhausted = std::max(
                service + 1.0,
                descriptor.thermalEmitter.exhaustedAfterEquivalentHours
            );
            const double atService = finiteClamp01(
                descriptor.thermalEmitter.wearAtReplacement01
            );
            return hours <= service
                ? atService * std::clamp(hours / service, 0.0, 1.0)
                : atService + (1.0 - atService) *
                      std::clamp(
                          (hours - service) / (exhausted - service),
                          0.0, 1.0
                      );
        };
        step.emitterWearDamageIncrement01 = std::max(
            0.0,
            emitterWearAt(step.next.emitter.equivalentOperatingHours) -
                emitterWearAt(beforeEmitterHours)
        );
        step.next.emitter.damage.wear01 = std::clamp(
            finiteClamp01(previous.emitter.damage.wear01) +
                step.emitterWearDamageIncrement01,
            0.0, 1.0
        );
    }

    const double heat =
        generatedWasteHeatMW(descriptor, current, delivered);
    step.dissipatedHeatMW = std::max(0.0, heat);
    // The hot nozzle emits FIRST; any excess loads the engine's conductive
    // path and later the shared coolant. No double-counting of emission.
    step.emitterRadiatedHeatMW = std::min(
        step.dissipatedHeatMW,
        ratedThermalEmitterPowerMW(
            descriptor,
            finiteClamp01(emitterEffectiveArea01) *
                effectiveEmitterArea01(previous)
        )
    );
    step.coolantHeatMW =
        step.dissipatedHeatMW - step.emitterRadiatedHeatMW;

    const double capacity =
        std::max(1.0e-9, descriptor.thermalCapacityMJperK);
    const double conductance =
        std::max(1.0e-9, descriptor.thermalConductanceMWperK);
    const double startT =
        std::isfinite(previous.temperatureK) &&
        previous.temperatureK > 0.0
            ? previous.temperatureK
            : coolant;
    const double equilibriumT = coolant + step.coolantHeatMW / conductance;
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
