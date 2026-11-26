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
    // field-coupled RCS: 1.0 MN per nozzle at 40 MW (25 kN/MW as before). 2026-11-26 (author: «мощность
    // ньютоновских RCS можно подкорректировать, учтя новую мощность в реакторе»): the 5-GW ER-5 had ~10x the
    // spare the old 0.5 MN / 20 MW bank drew (480 MW); a Newtonian Cobra on its four lift nozzles made 7.7 m/s2 -
    // it could not hold a ring rim's 1 g (the deck's charge is nozzle work now). 4 x 1.0 MN / ~260 t = 15.4 m/s2
    // (1.57 g, the rim's 1.2 g margin cleared), and the 70 % sustained share below still holds 1 g for good.
    out.nominalThrustNewtons = 1000000.0;
    out.nominalElectricalPowerMW = 40.0; // 24 nozzles <= 960 MW total, supplied by ER-5 spare power
    out.nominalEfficiency01 = 0.94;
    out.maxRelativeEfficiencyLossAtFullDamage01 = 0.10;
    out.nominalGimbalRangeRad = 0.0;
    out.nominalGimbalRateRadPerSec = 0.0;
    out.gimbalFailure.failureAtDamage01 = 1.0;
    out.nominalCoolantTemperatureK = 310.0;
    out.nominalOperatingTemperatureK = 390.0;
    out.safeOperatingTemperatureK = 450.0;
    // 0.60m emitter: 1.70MW at 450K under the universe's common 6MW/m2
    // pad law, 70 % of the healthy nozzle's 2.4MW waste at full thrust
    // (a hover on the lift nozzles - a ring rim's 1 g at ~64 % - stays
    // inside it; the capacity keeps the old ~15-20 s to the band).
    // Balanced heating: trims and short bursts never warm the nozzle into
    // its protection band; a sustained full burn (~15-20 s from the
    // operating temperature) does, and the nozzle then derates smoothly
    // toward 70 % and needs
    // minutes to cool - flying on the RCS is not free.
    // Nozzle cooling remains autonomous, with physical emitter damage.
    out.thermalEmitter.outerDiameterM = 0.60;
    out.thermalEmitter.apertureDiameterM = 0.0;
    out.thermalEmitter.surfaceTemperatureK = 1200.0;
    out.thermalEmitter.emissivity01 = 0.90;
    out.thermalEmitter.viewFactor01 = 0.90;
    out.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    out.thermalCapacityMJperK = 3.0;
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
