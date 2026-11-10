#pragma once

#include "src/game/equipment/reactor/EngineReactorUnit.h"

namespace game::equipment::reactor::presets
{
// COBRA family — Cobretty ER-5 dedicated hot propulsion generator.
// Rated electric output is SHARED by one Assisted fore/aft pair or
// exclusively assigned to one Newtonian main engine.
[[nodiscard]] inline EngineReactorDescriptor
CobraCobrettyER5PropulsionReactor() noexcept
{
    EngineReactorDescriptor d;
    d.typeId="elite.reactor.cobra.cobretty.er5.v1";
    d.displayName="Cobretty Cobra ER-5 Propulsion Reactor";
    d.manufacturer="Cobretty Corporation";
    d.ratedElectricalMW=5000.0;
    // author 2026-11-10: the 11 x 11 m pad (121 m2 for 556 MW of waste heat at 0.90) was out of proportion for the
    // Cobra. Like the CR-1 (0.991), the ER-5 converts 98.5 %: ~76 MW of waste -> 16.4 m2 of pad at the same x1.31
    // margin, built as two plates 3.5 x 2.35 m on the top and the bottom of the drive bay (along the drive group).
    d.nominalEfficiency01=0.985;
    d.hotStandbyThermalMW=2.0;
    d.thermalCapacityMJPerK=80.0;   // scaled with the waste heat (556 -> 76 MW): the same ~20 min to overheat without a pad
    d.padSideM=4.05;   // equivalent square side: 16.4 m2 (two plates 3.5 x 2.35 m)
    d.idleTemperatureK=900.0;
    d.startupTimeSeconds=420.0;
    d.operatingTemperatureK=900.0;
    d.safeTemperatureK=2100.0;
    d.criticalTemperatureK=2400.0;
    d.overhaulEquivalentHours=30000.0;
    d.endOfLifeEquivalentHours=50000.0;
    d.overheatDamagePerHourAt100K=0.05;
    d.maxEfficiencyLossAtFailure01=0.025;
    d.failure.failureAtDamage01=0.95;
    d.maintenance.wearPerOperatingHour=1.0/50000.0;
    d.maintenance.maintenanceWearThreshold01=0.20;
    return d;
}
} // namespace game::equipment::reactor::presets
