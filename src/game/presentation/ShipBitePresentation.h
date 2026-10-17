#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "src/game/bite/BiteApi.h"
#include "src/game/equipment/engine/EngineUnit.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"

namespace game::presentation
{

// Boundary adapter: this file is deliberately outside src/game/bite.
// It is the only place in this presentation path allowed to translate rich
// replicated ship/module state into the compact BITE public DTO.
inline game::bite::State buildShipBiteState(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules,
    std::uint64_t sourceRevision = 0
)
{
    std::vector<game::bite::EquipmentSample> samples;

    for (const auto& definition : descriptor.modules)
    {
        if (!definition.engine)
            continue;

        const auto it = std::find_if(
            modules.begin(),
            modules.end(),
            [&](const auto& module)
            {
                return module.moduleId == definition.moduleId;
            }
        );
        if (it == modules.end())
            continue;

        game::equipment::EquipmentDamageLayer damage;
        damage.sensitivity =
            definition.equipmentDamageSensitivity;
        damage.accumulated.laser01 = it->laserDamage01;
        damage.accumulated.impact01 = it->impactDamage01;
        damage.accumulated.thermal01 = it->thermalDamage01;
        damage.accumulated.radiation01 = it->radiationDamage01;
        damage.accumulated.wear01 = it->wearDamage01;

        const auto effective =
            game::equipment::engine::evaluateEngineUnit(
                *definition.engine,
                damage
            );

        game::bite::EquipmentSample sample;
        sample.equipmentId = definition.moduleId;
        sample.displayName = definition.moduleId;
        sample.functionalAuthority01 =
            effective.efficiencyAuthority01;
        sample.secondaryAuthority01 =
            effective.gimbalAuthority01;
        sample.powerCostMultiplier =
            effective.powerCostMultiplier;
        sample.heatMultiplier =
            effective.heatMultiplier;
        sample.wear01 =
            damage.accumulated.wear01;
        sample.maintenanceWearThreshold01 =
            definition.maintenance.maintenanceWearThreshold01;
        sample.operational =
            effective.operational &&
            it->health > 0.0f;

        samples.push_back(std::move(sample));
    }

    return game::bite::BiteApi{}.evaluate(
        sourceRevision,
        samples
    );
}

inline bool biteHudAttentionForEquipment(
    const game::bite::State& state,
    const std::string& equipmentId
) noexcept
{
    return std::any_of(
        state.faults.begin(),
        state.faults.end(),
        [&](const auto& fault)
        {
            return fault.equipmentId == equipmentId &&
                   fault.hudAttention;
        }
    );
}

} // namespace game::presentation
