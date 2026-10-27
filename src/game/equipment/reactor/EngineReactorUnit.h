#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/physics/UniverseThermalRadiation.h"

namespace game::equipment::reactor
{
// Reusable engine-generator TYPE, independent from its Cobra installation.
// Runtime per-installation state must never be shared between reactors.
struct EngineReactorDescriptor
{
    std::string_view typeId {};
    std::string_view displayName {};
    std::string_view manufacturer {};
    double ratedElectricalMW = 5000.0;
    double nominalEfficiency01 = 0.90;
    double hotStandbyThermalMW = 0.0;
    double thermalCapacityMJPerK = 1000.0;
    double padSideM = 10.0;
    double idleTemperatureK = 900.0;
    double startupTimeSeconds = 420.0;
    double operatingTemperatureK = 2500.0;
    double safeTemperatureK = 2750.0;
    double criticalTemperatureK = 3200.0;
    double overhaulEquivalentHours = 30000.0;
    double endOfLifeEquivalentHours = 50000.0;
    double overheatDamagePerHourAt100K = 0.05;
    double maxEfficiencyLossAtFailure01 = 0.025;
    // One equipment object, one common cause-of-damage representation.
    game::equipment::EquipmentFailureProfile failure {0.95};
    game::equipment::EquipmentMaintenanceProfile maintenance {};
};

struct EngineReactorState
{
    double temperatureK = game::physics::thermal::DeepSpaceTemperatureK;
    double equivalentOperatingHours = 0.0;
    double lastElectricalOutputMW = 0.0;
    double lastGeneratedHeatMW = 0.0;
    double lastPadHeatMW = 0.0;
    double lastCoolantHeatMW = 0.0;
    double lastStoredHeatMW = 0.0;
    game::equipment::EquipmentDamageState damage {};
    // Retain sub-quantum thermal damage until the canonical module damage
    // accumulator can commit it without invalidating snapshots each tick.
    double pendingThermalDamage01 = 0.0;
};

struct EngineReactorTick
{
    double electricalOutputMW = 0.0;
    double thermalPowerMW = 0.0;
    double depositedToEnginePadsMW = 0.0;
    double overflowToSharedCoolantMW = 0.0;
    double temperatureK = 0.0;
    double effectiveEfficiency01 = 0.0;
    double availableElectricalMW = 0.0;
    bool failed = false;
};

// Hot standby is continuous and imposes a passive thermal load even when
// output electrical power is zero. No artificial start/stop transition.
// The pad heat sink is supplied by the *installed engine(s)*, not invented
// here: the caller must subtract their own emitted heat before passing
// remainingPadCapacityMW.
[[nodiscard]] inline EngineReactorTick advanceEngineReactor(
    const EngineReactorDescriptor& type,
    EngineReactorState& state,
    double requestedElectricalMW,
    double remainingPadCapacityMW,
    double maxOverflowTransferMW,
    double dtSeconds
) noexcept
{
    EngineReactorTick out;
    const double dt = std::max(0.0,dtSeconds);
    const double damage = game::equipment::totalAccumulatedDamage01(state.damage);
    const auto function = game::equipment::evaluateEquipmentFunctionAtDamage01(
        damage,type.failure);
    out.failed=function.failed;
    const double ageRatio=std::clamp(state.equivalentOperatingHours/
        std::max(1.0,type.endOfLifeEquivalentHours),0.0,1.0);
    const double derating=std::max(0.0,1.0-0.20*ageRatio);
    const double ready=std::clamp(
        (state.temperatureK-game::physics::thermal::DeepSpaceTemperatureK)/
        std::max(1.0,type.idleTemperatureK-
            game::physics::thermal::DeepSpaceTemperatureK),0.0,1.0);
    const double thermalAuthority=state.temperatureK<=type.safeTemperatureK
        ? 1.0 : std::clamp(
            (type.criticalTemperatureK-state.temperatureK)/
            std::max(1.0,type.criticalTemperatureK-type.safeTemperatureK),
            0.0,1.0);
    out.availableElectricalMW=out.failed?0.0:
        type.ratedElectricalMW*function.authority01*derating*
        thermalAuthority*ready;
    out.electricalOutputMW=std::clamp(
        requestedElectricalMW,0.0,out.availableElectricalMW);
    const double efficiencyLoss=std::clamp(
        type.maxEfficiencyLossAtFailure01*damage+
        0.005*ageRatio,0.0,0.20);
    out.effectiveEfficiency01=std::clamp(
        type.nominalEfficiency01-efficiencyLoss,0.05,1.0);
    // Zero electrical output at idle. Heat generation includes only a
    // small fixed standby source; startup warmth is tracked separately.
    out.thermalPowerMW=std::max(0.0,type.hotStandbyThermalMW)+
        out.electricalOutputMW*(1.0/out.effectiveEfficiency01-1.0);
    const double idle=type.idleTemperatureK;
    const double design=type.safeTemperatureK;
    const double area=std::max(0.0,type.padSideM*type.padSideM);
    const double rated=game::physics::thermal::propulsionPadCoolingMW(
        area,1.0,design,idle,design);
    // Headroom is supplied as actual damage/availability from the installed
    // pad. The engine pads are independent sinks and are not shared.
    const double realPadCapacity=std::min(rated,
        std::max(0.0,remainingPadCapacityMW));
    const double cooling=game::physics::thermal::propulsionPadCoolingMW(
        area,rated>0.0?realPadCapacity/rated:0.0,
        state.temperatureK,idle,design);
    out.depositedToEnginePadsMW=std::min(cooling,
        out.thermalPowerMW+
        std::max(0.0,(state.temperatureK-idle)*
            type.thermalCapacityMJPerK/std::max(1.0e-6,dt)));
    // An installed emergency exchanger may remove both current surplus
    // and stored heat ABOVE the reactor's idle temperature. Its caller
    // must cap the request by pump power, sink headroom, damage and dT,
    // and add exactly this heat to the receiving coolant.
    // At zero granted transfer the previous reactor model is unchanged.
    const double extractableMW=dt>0.0
        ? std::max(0.0,out.thermalPowerMW-out.depositedToEnginePadsMW+
            std::max(0.0,state.temperatureK-idle)*
                type.thermalCapacityMJPerK/dt)
        : 0.0;
    out.overflowToSharedCoolantMW=std::min(
        std::max(0.0,maxOverflowTransferMW),extractableMW);
    // Startup thermal conditioning: 99% of idle temperature in the
    // specified startup time, independent of propulsion electrical output.
    const double cold=game::physics::thermal::DeepSpaceTemperatureK;
    const double warmTarget=idle;
    if(dt>0.0)
    {
        const double tau=std::max(1.0,type.startupTimeSeconds/4.60517);
        const double base=std::max(cold,state.temperatureK);
        const double warm=base<warmTarget
            ? (warmTarget-base)*(1.0-std::exp(-dt/tau)):0.0;
        state.temperatureK=std::max(cold,
            base+warm+(out.thermalPowerMW-
                out.depositedToEnginePadsMW-
                out.overflowToSharedCoolantMW)*dt/
                std::max(1.0,type.thermalCapacityMJPerK));
        state.equivalentOperatingHours+=dt/3600.0*
            (out.electricalOutputMW/std::max(1.0,type.ratedElectricalMW));
        const double thermalExcess=std::max(0.0,
            state.temperatureK-type.safeTemperatureK);
        game::equipment::addEquipmentDamage(
            state.damage,game::equipment::EquipmentDamageKind::Thermal,
            type.overheatDamagePerHourAt100K*dt/3600.0*
                (thermalExcess/100.0));
    }
    out.temperatureK=state.temperatureK;
    state.lastElectricalOutputMW=out.electricalOutputMW;
    state.lastGeneratedHeatMW=out.thermalPowerMW;
    state.lastPadHeatMW=out.depositedToEnginePadsMW;
    state.lastCoolantHeatMW=out.overflowToSharedCoolantMW;
    state.lastStoredHeatMW=out.thermalPowerMW-
        out.depositedToEnginePadsMW-out.overflowToSharedCoolantMW;
    return out;
}
} // namespace game::equipment::reactor
