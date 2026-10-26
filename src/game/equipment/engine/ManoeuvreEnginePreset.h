#pragma once

// Fixed-direction electrical RCS motor. NOT a new damage or heat simulator:
// each nozzle is an ordinary installed EngineUnitDescriptor attached to its
// own ModuleDescriptor and EquipmentDamageLayer / EngineOperatingState.
// ObjectModuleRuntime is the sole source of accumulated hit and wear damage.
//
// These are authored GAME-UNIVERSE ratings, not a real gas rocket.
// not measurements of a real thruster. Ship tuning may override the passport.
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
    out.thermalCapacityMJperK = 0.20;
    out.thermalConductanceMWperK = 0.002;
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
