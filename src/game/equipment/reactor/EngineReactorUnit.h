#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include "src/game/equipment/EquipmentDamage.h"

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
    double thermalCapacityMJPerK = 100.0;
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
    double temperatureK = 2500.0;
    double equivalentOperatingHours = 0.0;
    game::equipment::EquipmentDamageState damage {};
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
    out.failed = function.failed;
    const double ageRatio = std::clamp(
        state.equivalentOperatingHours/
            std::max(1.0,type.endOfLifeEquivalentHours),0.0,1.0);
    const double lifeDerating = 1.0-0.20*ageRatio;
    out.availableElectricalMW = out.failed ? 0.0 :
        std::max(0.0,type.ratedElectricalMW)*function.authority01*lifeDerating;
    out.electricalOutputMW = std::clamp(
        std::max(0.0,requestedElectricalMW),0.0,out.availableElectricalMW);
    const double loss = std::clamp(
        type.maxEfficiencyLossAtFailure01*damage +
        0.005*ageRatio,0.0,0.20);
    out.effectiveEfficiency01 = std::clamp(
        type.nominalEfficiency01-loss,0.05,1.0);
    const double generated = out.electricalOutputMW*
        (1.0/out.effectiveEfficiency01-1.0);
    out.thermalPowerMW = std::max(0.0,type.hotStandbyThermalMW) + generated;
    // Heat exchange is bounded, never silently disappears when a pad fails.
    out.depositedToEnginePadsMW = std::min(
        out.thermalPowerMW,std::max(0.0,remainingPadCapacityMW));
    const double overflow = out.thermalPowerMW-out.depositedToEnginePadsMW;
    out.overflowToSharedCoolantMW = std::min(
        overflow,std::max(0.0,maxOverflowTransferMW));
    const double storedMW = overflow-out.overflowToSharedCoolantMW;

    // Surplus heat is stored in the installed reactor's finite thermal mass.
    // When demand falls, previously stored heat must be able to escape using
    // ONLY the unused pad/coolant capacity. Never cool below the operating
    // baseline and never spend the same sink capacity twice.
    const double heatCapacity = std::max(1.0,type.thermalCapacityMJPerK);
    if (dt > 0.0)
    {
        state.temperatureK += storedMW*dt/heatCapacity;
        const double storedExcessMW = std::max(
            0.0,(state.temperatureK-type.operatingTemperatureK)*
                heatCapacity/dt);
        const double sparePadMW = std::max(
            0.0,remainingPadCapacityMW-out.depositedToEnginePadsMW);
        const double spareCoolantMW = std::max(
            0.0,maxOverflowTransferMW-out.overflowToSharedCoolantMW);
        const double recoveredToPadMW = std::min(storedExcessMW,sparePadMW);
        const double recoveredToCoolantMW = std::min(
            storedExcessMW-recoveredToPadMW,spareCoolantMW);
        out.depositedToEnginePadsMW += recoveredToPadMW;
        out.overflowToSharedCoolantMW += recoveredToCoolantMW;
        state.temperatureK -=
            (recoveredToPadMW+recoveredToCoolantMW)*dt/heatCapacity;
    }
    if(dt>0.0)
    {
        state.equivalentOperatingHours +=
            dt/3600.0*(out.electricalOutputMW/
                std::max(1.0,type.ratedElectricalMW));
        const double thermalExcess = std::max(0.0,
            state.temperatureK-type.safeTemperatureK);
        game::equipment::addEquipmentDamage(
            state.damage,game::equipment::EquipmentDamageKind::Thermal,
            type.overheatDamagePerHourAt100K*dt/3600.0*
            (thermalExcess/100.0));
        const double wear = dt/3600.0/
            std::max(1.0,type.endOfLifeEquivalentHours)*
            out.electricalOutputMW/std::max(1.0,type.ratedElectricalMW);
        game::equipment::addEquipmentDamage(
            state.damage,game::equipment::EquipmentDamageKind::Wear,wear);
    }
    out.temperatureK = state.temperatureK;
    return out;
}
} // namespace game::equipment::reactor
