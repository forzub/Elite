#pragma once

// Fixed-direction electrical RCS motor. NOT a new damage or heat simulator:
// each nozzle is an ordinary installed EngineUnitDescriptor attached to its
// own ModuleDescriptor and EquipmentDamageLayer / EngineOperatingState.
// ObjectModuleRuntime is the sole source of accumulated hit and wear damage.
//
// Authored GAME-UNIVERSE ratings for field-coupled nozzles, not a
// chemical rocket whose propellant is thrown into vacuum.
#include "src/game/equipment/engine/EngineUnit.h"

namespace game::equipment::engine
{
[[nodiscard]] inline EngineUnitDescriptor cobraManoeuvreNozzlePreset() noexcept
{
    EngineUnitDescriptor out;
    out.engineTypeId = "cobra.rcs.electric.fixed.v1";
    out.engineDisplayName = "Cobra electric manoeuvre nozzle";
    out.manufacturerName = "Cobra";
    out.propulsionPrinciple = PropulsionPrinciple::FieldCoupling;
    out.nominalThrustNewtons = 500000.0; // field-coupled RCS: 0.50 MN per nozzle
    out.nominalElectricalPowerMW = 20.0; // 24/nozzles <= 480 MW total, supplied by ER-5 spare power
    out.nominalEfficiency01 = 0.94;
    out.maxRelativeEfficiencyLossAtFullDamage01 = 0.10;
    out.nominalGimbalRangeRad = 0.0;
    out.nominalGimbalRateRadPerSec = 0.0;
    out.gimbalFailure.failureAtDamage01 = 1.0;
    out.nominalCoolantTemperatureK = 310.0;
    out.nominalOperatingTemperatureK = 390.0;
    out.safeOperatingTemperatureK = 450.0;
    // 0.39m emitter: 0.72MW at 450K under the universe's common 6MW/m2
    // pad law, 60 % of the healthy nozzle's 1.2MW waste at full thrust.
    // Balanced heating: trims and short bursts never warm the nozzle into
    // its protection band; a sustained full burn (~15-20 s from the
    // operating temperature) does, and the nozzle then derates smoothly
    // toward 60 % (about 84 % after one minute, ~70 % after two) and needs
    // minutes to cool - flying on the RCS is not free.
    // Nozzle cooling remains autonomous, with physical emitter damage.
    out.thermalEmitter.outerDiameterM = 0.39;
    out.thermalEmitter.apertureDiameterM = 0.0;
    out.thermalEmitter.surfaceTemperatureK = 1200.0;
    out.thermalEmitter.emissivity01 = 0.90;
    out.thermalEmitter.viewFactor01 = 0.90;
    out.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    out.thermalCapacityMJperK = 2.0;
    out.thermalConductanceMWperK = 0.002; // deprecated; pad-only thermal sink
    out.overheatReferenceExcessK = 40.0;
    out.thermalDamagePerHourAtReferenceExcess01 = 0.08;
    out.overhaulEquivalentHours = 4000.0;
    out.endOfLifeEquivalentHours = 8000.0;
    out.damageAtOverhaul01 = 0.20;
    out.damageAtEndOfLife01 = 0.95;
    out.wearThrottleExponent = 1.5;
    out.irregularThrustStartsAtDamage01 = 0.70;
    return out;
}
} // namespace game::equipment::engine
