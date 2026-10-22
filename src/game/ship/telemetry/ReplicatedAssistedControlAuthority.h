#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ReplicatedInstalledAssistedDescriptor.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/modules/ObjectModuleRuntime.h"

namespace game::ship::telemetry
{

// Client-side view of the EXACT installed motor basis used by ShipCore.
// Reconstructs installed Engine inputs from replicated per-module damage,
// emitter and thermal state. It never substitutes a descriptor's ideal
// strafe/angular capability when a real actuator state is unavailable.
//
// cache is per ship, not per frame; matches() avoids rebuilding the 12
// expensive six-axis basis maneuvers until motor availability changes.
[[nodiscard]] inline ShipControlAuthorityTelemetry
readReplicatedAssistedControlAuthority(
    const ShipDescriptor& descriptor,
    game::ship::plant::PhysicalControlModelId model,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules,
    const ShipParams& physicalParams,
    game::ship::plant::AssistedManeuverPrograms& cache
)
{
    using namespace game::ship::plant;
    using namespace game::equipment;
    using namespace game::equipment::engine;

    if (familyOf(model) != PhysicalControlFamily::Assisted)
        return {};

    const auto& topology = physicalControlSlots(model);
    if (topology.empty() || topology.size() > AssistedManeuverPrograms::Max)
        return {};

    std::array<EquipmentDamageLayer, AssistedManeuverPrograms::Max> damage {};
    std::array<EngineOperatingState, AssistedManeuverPrograms::Max> operating {};
    std::array<EngineUnitDescriptor, AssistedManeuverPrograms::Max> diagnostic {};
    std::vector<MountedEngine> mounted(topology.size());

    const auto installed =
        game::ship::capability::replicatedInstalledAssistedDescriptor(
            descriptor,model);
    const ShipDescriptor& selected =
        installed ? *installed : descriptor;

    if (model != selected.physicalControlModel)
    {
        // Noninstalled diagnostic layouts use the same synthetic motor
        // passports as ShipCore::updateTranslationControl. Never synthesize
        // such motors for a normal installed (potentially damaged) ship.
        for (std::size_t i = 0; i < topology.size(); ++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            diagnostic[i] = presets::CobrettyCR1VectorMainDrive();
            damage[i].failure.failureAtDamage01 = 0.95;
            operating[i].temperatureK =
                diagnostic[i].nominalOperatingTemperatureK;
            mounted[i] = {
                &diagnostic[i], &damage[i], &operating[i],
                topology[i].id, true
            };
        }
    }
    else
    {
        const auto& ids = selected.physicalControl.slotModuleIds;
        if (ids.size() != topology.size() || modules.empty())
            return {};

        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            const auto definition = std::find_if(
                selected.modules.begin(), selected.modules.end(),
                [&](const auto& m) { return m.moduleId == ids[i]; }
            );
            const auto state = std::find_if(
                modules.begin(), modules.end(),
                [&](const auto& m) { return m.moduleId == ids[i]; }
            );
            if (definition == selected.modules.end() ||
                !definition->engine || state == modules.end() ||
                !state->hasEngineThermalState)
            {
                return {}; // Never claim healthy authority from missing data.
            }

            damage[i].sensitivity = definition->equipmentDamageSensitivity;
            damage[i].failure = definition->equipmentFailure;
            damage[i].accumulated.laser01 = state->laserDamage01;
            damage[i].accumulated.impact01 = state->impactDamage01;
            damage[i].accumulated.thermal01 = state->thermalDamage01;
            damage[i].accumulated.radiation01 = state->radiationDamage01;
            damage[i].accumulated.wear01 = state->wearDamage01;
            damage[i].accumulated.internalFailure01 =
                state->internalFailureDamage01;

            operating[i].temperatureK = state->engineTemperatureK;
            operating[i].coolantTemperatureK =
                state->engineCoolantTemperatureK;
            operating[i].impactShock01 = state->engineImpactShock01;
            operating[i].impactShockSecondsRemaining =
                state->engineImpactShockSecondsRemaining;
            operating[i].emitter.surface.intactArea01 =
                state->emitterIntactArea01;
            operating[i].emitter.surface.microcrack01 =
                state->emitterMicrocrack01;
            operating[i].emitter.damage.laser01 =
                state->emitterLaserDamage01;
            operating[i].emitter.damage.impact01 =
                state->emitterImpactDamage01;
            operating[i].emitter.damage.thermal01 =
                state->emitterThermalDamage01;
            operating[i].emitter.damage.radiation01 =
                state->emitterRadiationDamage01;
            operating[i].emitter.damage.wear01 =
                state->emitterWearDamage01;
            operating[i].emitter.equivalentOperatingHours =
                state->emitterEquivalentOperatingHours;

            mounted[i] = {
                &*definition->engine, &damage[i], &operating[i], ids[i],
                state->state == static_cast<std::uint8_t>(
                    world::modules::ObjectModuleRuntimeState::Attached) &&
                state->health > 0.0f
            };
        }
    }

    if (!cache.matches(model, mounted, physicalParams))
        cache.build(model, mounted, physicalParams);

    // Snapshot projection is reconstructed, not a native ShipCore actuator
    // revision. Zero deliberately does not pretend to be the server revision.
    return readAssistedControlAuthority(cache, 0);
}

} // namespace game::ship::telemetry
