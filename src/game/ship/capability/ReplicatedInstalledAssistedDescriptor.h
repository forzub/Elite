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
replicatedInstalledAssistedDescriptor(
    const ShipDescriptor& native,
    plant::PhysicalControlModelId model)
{
    if (model == native.physicalControlModel ||
        plant::familyOf(model) != plant::PhysicalControlFamily::Assisted)
        return std::nullopt;
    const auto engineIds =
        presets::cobra::installedAssistedEngineModuleIds(model);
    const auto reactorIds =
        presets::cobra::installedAssistedReactorModuleIds(model);
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
    return installed;
}
} // namespace game::ship::capability
