#pragma once

#include <cstdint>
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/descriptors/ModuleDescriptor.h"
#include "src/world/modules/ObjectModuleState.h"

namespace game::simulation
{
// Single canonical builder for ship and object runtime module snapshots.
// Material ratings and geometry remain immutable local descriptor data;
// only per-installation state is copied to the replication data plane.
[[nodiscard]] inline ObjectModuleSnapshot makeObjectModuleSnapshot(
    const ModuleDescriptor& descMod,
    const world::modules::ObjectModuleState* rt
)
{
    ObjectModuleSnapshot ms;
    ms.moduleId = descMod.moduleId;
    if (!rt)
    {
        ms.state = 0;
        ms.health = descMod.maxHealth;
        return ms;
    }

    ms.state = static_cast<std::uint8_t>(rt->state);
    ms.health = rt->health;
    ms.laserDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.laser01);
    ms.impactDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.impact01);
    ms.thermalDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.thermal01);
    ms.radiationDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.radiation01);
    ms.wearDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.wear01);
    ms.internalFailureDamage01 = static_cast<float>(
        rt->equipmentDamage.accumulated.internalFailure01);
    ms.aliveSupportCount = rt->aliveSupportCount;

    if (descMod.engine)
    {
        ms.hasEngineThermalState = true;
        const auto& engine = rt->engineOperating;
        const auto& emitter = engine.emitter;
        ms.engineTemperatureK =
            static_cast<float>(engine.temperatureK);
        ms.engineCoolantTemperatureK=
            static_cast<float>(engine.coolantTemperatureK);
        ms.engineImpactShock01=
            static_cast<float>(engine.impactShock01);
        ms.engineImpactShockSecondsRemaining=
            static_cast<float>(engine.impactShockSecondsRemaining);
        ms.emitterIntactArea01 =
            static_cast<float>(emitter.surface.intactArea01);
        ms.emitterMicrocrack01 =
            static_cast<float>(emitter.surface.microcrack01);
        ms.emitterLaserDamage01 =
            static_cast<float>(emitter.damage.laser01);
        ms.emitterImpactDamage01 =
            static_cast<float>(emitter.damage.impact01);
        ms.emitterThermalDamage01 =
            static_cast<float>(emitter.damage.thermal01);
        ms.emitterRadiationDamage01 =
            static_cast<float>(emitter.damage.radiation01);
        ms.emitterWearDamage01 =
            static_cast<float>(emitter.damage.wear01);
        ms.emitterEquivalentOperatingHours =
            static_cast<float>(emitter.equivalentOperatingHours);
    }

    return ms;
}
} // namespace game::simulation
