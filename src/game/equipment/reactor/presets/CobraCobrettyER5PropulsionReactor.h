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
    d.nominalEfficiency01=0.90;
    d.hotStandbyThermalMW=2.0;
    d.thermalCapacityMJPerK=600.0;
    d.padSideM=11.0;
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
