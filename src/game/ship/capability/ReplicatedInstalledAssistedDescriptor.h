#pragma once

#include <algorithm>
#include <optional>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"

namespace game::ship::capability
{
// Client snapshots carry per-instance selected model, but their type registry
// still points to the original immutable ship descriptor. Only materialize a
// per-layout view when ALL real engine and generator passports are present.
// Never substitute healthy synthetic motors for a missing installed unit.
[[nodiscard]] inline std::optional<ShipDescriptor>
replicatedInstalledCobraDescriptor(
    const ShipDescriptor& native,
    plant::PhysicalControlModelId model)
{
    if(model==native.physicalControlModel)
        return std::nullopt;
    const bool assisted=
        plant::familyOf(model)==plant::PhysicalControlFamily::Assisted;
    const auto engineIds=assisted
        ? presets::cobra::installedAssistedEngineModuleIds(model)
        : presets::cobra::installedNewtonianEngineModuleIds(model);
    const auto reactorIds=assisted
        ? presets::cobra::installedAssistedReactorModuleIds(model)
        : presets::cobra::installedNewtonianReactorModuleIds(model);
    if (engineIds.size() != plant::physicalControlSlots(model).size() ||
        reactorIds.size() != presets::cobra::installationSlots(model)
            .engineReactors.size())
        return std::nullopt;

    for (const auto& id : engineIds)
    {
        const auto def=std::find_if(native.modules.begin(),native.modules.end(),
            [&](const ModuleDescriptor& m){return m.moduleId==id;});
        if(def==native.modules.end() || !def->engine)
            return std::nullopt;
    }
    for (const auto& id : reactorIds)
    {
        const auto def=std::find_if(native.modules.begin(),native.modules.end(),
            [&](const ModuleDescriptor& m){return m.moduleId==id;});
        if(def==native.modules.end() || !def->engineReactor)
            return std::nullopt;
    }

    ShipDescriptor installed=native;
    installed.physicalControlModel=model;
    installed.localFlightControlLaw=plant::nativeFlightLaw(model);
    installed.physicalControl.slotModuleIds=engineIds;
    installed.powerThermalInstallation.engineReactorModuleIds=reactorIds;
    installed.mainPropulsion.aftEngineModuleIds.clear();
    installed.mainPropulsion.foreEngineModuleIds.clear();
    const auto& topology=plant::physicalControlSlots(model);
    for(std::size_t i=0;i<topology.size();++i)
    {
        if(topology[i].role==plant::EngineSlotRole::AftMain)
            installed.mainPropulsion.aftEngineModuleIds.push_back(engineIds[i]);
        else if(topology[i].role==plant::EngineSlotRole::ForeMain)
            installed.mainPropulsion.foreEngineModuleIds.push_back(engineIds[i]);
    }
    return installed;
}

// Older callers use this name. It now also reconstructs native installed
// Newtonian profiles; do not revive the synthetic healthy-ship fallback.
[[nodiscard]] inline std::optional<ShipDescriptor>
replicatedInstalledAssistedDescriptor(
    const ShipDescriptor& native,
    plant::PhysicalControlModelId model)
{
    return replicatedInstalledCobraDescriptor(native,model);
}
} // namespace game::ship::capability
