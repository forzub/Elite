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
    e.engineTypeId = "elite.engine.cobretty.cr1-vector.v1";
    e.engineDisplayName = "Cobretty CR-1 Vector Main Drive";
    e.manufacturerName = "Cobretty Corporation";
    e.propulsionPrinciple = PropulsionPrinciple::FieldCoupling;
    e.structureMaterialId = "elite.material.engine.tial-ht.v1";
    e.nominalThrustNewtons = 9561483.75;           // 9.56 MN per engine
    // Game-universe TVC: field steering is effectively instantaneous.
    // A deliberately generous angle lets the EXISTING Assisted law retain
    // its 7.5 g lateral response without fabricating a separate side force.
    // Newtonian installs continue using their smaller ShipParams TVC policy.
    e.nominalGimbalRangeRad = 1.3089969389957472; // 75 degrees
    e.nominalGimbalRateRadPerSec = 1000.0; // not simulated as servo motion

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
