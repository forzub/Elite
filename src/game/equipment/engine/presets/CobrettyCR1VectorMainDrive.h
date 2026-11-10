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
    e.engineDisplayName = "Cobretty Cobra CR-1 Vector Main Drive";
    e.manufacturerName = "Cobretty Corporation";
    // Product identity (A3 wide-vector unit): src/game/catalog/propulsion.
    e.propulsionPrinciple = PropulsionPrinciple::FieldCoupling;
    e.structureMaterialId = "elite.material.engine.tial-ht.v1";
    e.nominalThrustNewtons = 9561483.75;           // 9.56 MN per engine
    // Game-universe TVC: field steering is effectively instantaneous.
    // A deliberately generous angle lets the EXISTING Assisted law retain
    // its 7.5 g lateral response without fabricating a separate side force.
    // Newtonian installs mount a different, narrow-vector unit (N1).
    e.nominalGimbalRangeRad = 1.3089969389957472; // 75 degrees
    e.nominalGimbalRateRadPerSec = 1000.0; // not simulated as servo motion

    e.nominalElectricalPowerMW = 4850.0; // Rated input; not F*v
    // Author 2026-11-10: the Cobra silhouette wins - the drive bay shrinks from
    // dia 5 m to 3.5 m, and the pad becomes a convex cone 1.3 m high on the 3.5 m
    // base (11.98 m2 x 6 MW/m2 = 71.9 MW). With the compact drive's higher
    // efficiency the full-thrust heat margin is x1.65 (was x1.68): waste 43.7 MW,
    // housing settles at ~935 K (safe 1250 K), full thrust holds down to 61 %
    // pad integrity (was 60 %). Before: flat 5 m pad, 0.9855.
    e.nominalEfficiency01 = 0.991;
    e.maxRelativeEfficiencyLossAtFullDamage01 = 0.002;

    e.thermalEmitter.outerDiameterM = 3.5;
    e.thermalEmitter.coneHeightM = 1.3;
    e.thermalEmitter.apertureDiameterM = 0.0;
    e.thermalEmitter.surfaceTemperatureK = 2500.0;
    e.thermalEmitter.emissivity01 = 0.9;
    e.thermalEmitter.viewFactor01 = 0.90;
    e.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    e.thermalEmitter.meshPartId = "engine_hot_emitter_annulus"; // existing mesh ID; physics surface is solid

    e.nominalCoolantTemperatureK = 1050.0;
    e.nominalOperatingTemperatureK = 450.0;
    e.safeOperatingTemperatureK = 1250.0;
    e.thermalCapacityMJperK = 70.0;
    const double wasteMW =
        e.nominalElectricalPowerMW * (1.0 - e.nominalEfficiency01);
    // Keep the finite physical housing→coolant heat path used before
    // conversion to a solid radiating pad. Do not recalculate it as
    // (waste - emitter capacity): that would become zero when healthy and
    // would prevent a damaged emitter from heating the coolant.
    // Calibrated independent housing-to-coolant conductance: not tied to
    // the universe's radiation multiplier or any emitter surface geometry.
    // Otherwise increasing pad radiation could silently remove the
    // damaged engine's only finite thermal path into shared coolant.
    e.thermalConductanceMWperK = 0.14004;
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
