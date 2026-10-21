#pragma once

// Install each Newtonian Cobra RCS nozzle as a real independently damageable
// equipment module. This is a descriptor assembly helper, NOT a substitute
// for the server actuator allocation. The caller must apply the resulting
// module descriptors BEFORE ObjectModuleRuntime::init and replicate them.
// Eight mount nodes share no simulated failure state: each nozzle has an ID.
#include <algorithm>
#include <string>
#include <vector>

#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"
#include "src/game/equipment/engine/ManoeuvreEnginePreset.h"
#include "src/world/descriptors/ModuleDescriptor.h"

namespace game::ship::presets::cobra
{
inline bool addNewtonianRcsEquipmentModules(
    std::vector<ModuleDescriptor>& installedModules
)
{
    const auto& mounts = newtonianRcsNozzles();
    // Fail before changing the graph if any ID is already installed.
    for (const auto& mount : mounts)
    {
        const bool duplicate = std::any_of(
            installedModules.begin(), installedModules.end(),
            [&mount](const ModuleDescriptor& module)
            {
                return module.moduleId == mount.nozzleId;
            }
        );
        if (duplicate)
            return false;
    }

    installedModules.reserve(installedModules.size() + mounts.size());
    for (const auto& mount : mounts)
    {
        ModuleDescriptor unit;
        unit.moduleId = std::string(mount.nozzleId);
        unit.subsystemId = "propulsion.rcs";
        unit.moduleClass = "manoeuvre_nozzle";
        unit.structuralKind = ModuleStructuralKind::Equipment;
        unit.attachmentType = ModuleAttachmentType::SurfaceMounted;
        unit.enabled = true;
        unit.destructible = true;
        unit.repairable = true;
        unit.salvageable = true;
        unit.detachPolicy = ModuleDetachPolicy::Never;
        unit.destroyPolicy = ModuleDestroyPolicy::DisableOnly;
        // No artificial hardpoint hit volume is fabricated here. A caller
        // must associate the module with its actual hull damage geometry.
        // Hits can already be applied by stable moduleId through runtime.
        unit.engine =
            game::equipment::engine::cobraManoeuvreNozzlePreset();
        unit.equipmentFailure.failureAtDamage01 = 0.95;
        unit.maintenance.maintenanceWearThreshold01 = 0.20;
        installedModules.push_back(std::move(unit));
    }
    return true;
}
} // namespace game::ship::presets::cobra
