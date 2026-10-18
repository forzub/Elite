#pragma once

#include "src/game/equipment/engine/EngineUnit.h"

namespace game::equipment::engine::presets
{
// COBRETTY CORPORATION — CR-1 Vector Main Drive.
// Immutable, reusable engine TYPE passport. Installation-specific damage,
// emitter wear and temperature live only in ObjectModuleRuntime.
// Other ships can mount this same type without copying its physical constants.
[[nodiscard]] inline EngineUnitDescriptor CobrettyCR1VectorMainDrive() noexcept
{
    EngineUnitDescriptor e;
    e.propulsionPrinciple = PropulsionPrinciple::FieldCoupling;
    e.structureMaterialId = "elite.material.engine.tial-ht.v1";
    e.nominalThrustNewtons = 9561483.75;           // 9.56 MN per engine
    e.nominalGimbalRangeRad = 0.122173048;        // 7 degrees
    e.nominalGimbalRateRadPerSec = 0.139626340;  // 8 deg/s

    e.nominalElectricalPowerMW = 4850.0; // Rated input; not F*v
    e.nominalEfficiency01 = 0.9855;
    e.maxRelativeEfficiencyLossAtFullDamage01 = 0.002;

    e.thermalEmitter.outerDiameterM = 4.5;
    e.thermalEmitter.apertureDiameterM = 2.5;
    e.thermalEmitter.surfaceTemperatureK = 2500.0;
    e.thermalEmitter.emissivity01 = 0.9;
    e.thermalEmitter.viewFactor01 = 0.90;
    e.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    e.thermalEmitter.meshPartId = "engine_hot_emitter_annulus";

    e.nominalCoolantTemperatureK = 1050.0;
    e.nominalOperatingTemperatureK = 1200.0;
    e.safeOperatingTemperatureK = 1300.0;
    e.thermalCapacityMJperK = 12.0;
    const double wasteMW =
        e.nominalElectricalPowerMW * (1.0 - e.nominalEfficiency01);
    const double radiatorMW = std::min(
        wasteMW, ratedThermalEmitterPowerMW(e)
    );
    e.thermalConductanceMWperK =
        (wasteMW - radiatorMW) /
        (e.nominalOperatingTemperatureK - e.nominalCoolantTemperatureK);
    e.overheatReferenceExcessK = 100.0;
    e.thermalDamagePerHourAtReferenceExcess01 = 0.05;

    e.overhaulEquivalentHours = 30000.0;
    e.endOfLifeEquivalentHours = 50000.0;
    e.damageAtOverhaul01 = 0.20;
    e.damageAtEndOfLife01 = 0.95;
    e.wearThrottleExponent = 1.5;
    e.gimbalFailure.failureAtDamage01 = 0.90;
    return e;
}
} // namespace game::equipment::engine::presets
