#pragma once

// Private helpers shared by the ShipCore translation units
// (ShipCore*.cpp). Not a public interface.

#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"
#include "src/game/ship/ShipLoadoutTopology.h"
#include "ShipCore.h"
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "core/log.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/ShipPropulsionState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/NewtonianRcsAllocator.h"
#include "src/game/ship/plant/NewtonianRcsWorkingFluid.h"
#include "src/game/equipment/radar/RadarDesc.h"
#include "src/game/equipment/data/radars.h"

#include "src/world/modules/ObjectHitBuilder.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"

#include "src/game/geometry/ObjLoader.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/game/ship/view/ShipAttachmentResolver.h"
#include "src/game/ship/ShipAttachmentUtils.h"

#include "src/world/navigation/NavigationBodyAdapter.h"

namespace ship_core_detail
{
// A hull that carries all SIX installable loadouts (today Cobrett; any hull
// may) publishes one immutable installed descriptor per loadout. Module
// definitions are identical across these profiles; only the active
// engine/reactor bindings and model differ, each from the hull's own loadout.
// Profiles are cached per hull descriptor (descriptors are never destroyed).
[[nodiscard]] inline bool hasAllInstalledProfiles(const ShipDescriptor& d)
{
    for(std::size_t k=0;k<6;++k)
        if(!d.findLoadout(static_cast<game::ship::plant::PhysicalControlModelId>(k)))
            return false;
    return true;
}

inline const std::array<ShipDescriptor,6>& installedProfiles(
    const ShipDescriptor& original)
{
    static std::mutex mutex;
    static auto* cache=new std::map<const ShipDescriptor*,
        std::unique_ptr<const std::array<ShipDescriptor,6>>>();
    std::lock_guard lock(mutex);
    auto& slot=(*cache)[&original];
    if(slot)
        return *slot;
    slot=std::make_unique<const std::array<ShipDescriptor,6>>([&] {
        std::array<ShipDescriptor,6> installed {
            original,original,original,original,original,original};
        for(std::size_t k=0;k<installed.size();++k)
        {
            const auto model=static_cast<
                game::ship::plant::PhysicalControlModelId>(k);
            auto& desc=installed[k];
            const auto* loadout=original.findLoadout(model);
            if(!loadout)
                throw std::logic_error(
                    "Invalid installed loadout profile: missing loadout");
            game::ship::requireLoadoutMatchesModelStructure(*loadout);
            desc.physicalControlModel=model;
            desc.localFlightControlLaw=loadout->flightLaw;
            desc.physicalControl.slotModuleIds=loadout->propulsionModuleIds;
            desc.powerThermalInstallation.engineReactorModuleIds.clear();
            for(const auto& group:loadout->engineReactorGroups)
                desc.powerThermalInstallation.engineReactorModuleIds.push_back(
                    group.reactorModuleId);
            // Construction is a mandatory installed equipment mapping, not
            // optional damage telemetry. Detect missing/duplicate IDs and
            // absent hardware passports before this profile can fly.
            const auto& installedIds=desc.physicalControl.slotModuleIds;
            const auto& reactorIds=
                desc.powerThermalInstallation.engineReactorModuleIds;
            const auto& topology=loadout->propulsionSlots;
            const auto& groups=loadout->engineReactorGroups;
            if(installedIds.size()!=topology.size() ||
               reactorIds.size()!=groups.size())
                throw std::logic_error(
                    "Invalid installed loadout profile: slot count");
            const auto requirePassport=[&](const std::string& id,
                                            bool engine) {
                if(id.empty())
                    throw std::logic_error(
                        "Invalid installed loadout profile: empty module ID");
                const auto it=std::find_if(desc.modules.begin(),desc.modules.end(),
                    [&](const ModuleDescriptor& module) {
                        return module.moduleId==id;
                    });
                if(it==desc.modules.end() ||
                   (engine ? !it->engine : !it->engineReactor))
                    throw std::logic_error(
                        "Invalid installed loadout profile: missing passport "+id);
            };
            for(std::size_t i=0;i<installedIds.size();++i)
            {
                if(std::find(installedIds.begin(),installedIds.begin()+i,
                             installedIds[i])!=installedIds.begin()+i)
                    throw std::logic_error(
                        "Invalid installed loadout profile: duplicate engine ID");
                requirePassport(installedIds[i],true);
            }
            for(std::size_t i=0;i<reactorIds.size();++i)
            {
                if(std::find(reactorIds.begin(),reactorIds.begin()+i,
                             reactorIds[i])!=reactorIds.begin()+i)
                    throw std::logic_error(
                        "Invalid installed loadout profile: duplicate reactor ID");
                requirePassport(reactorIds[i],false);
            }
            desc.mainPropulsion.aftEngineModuleIds.clear();
            desc.mainPropulsion.foreEngineModuleIds.clear();
            for(std::size_t i=0;i<topology.size();++i)
            {
                const auto role=topology[i].role;
                if(role==game::ship::plant::EngineSlotRole::AftMain)
                    desc.mainPropulsion.aftEngineModuleIds.push_back(
                        desc.physicalControl.slotModuleIds[i]);
                else if(role==game::ship::plant::EngineSlotRole::ForeMain)
                    desc.mainPropulsion.foreEngineModuleIds.push_back(
                        desc.physicalControl.slotModuleIds[i]);
            }
        }
        return installed;
    }());
    return *slot;
}
} // namespace ship_core_detail

using namespace ship_core_detail;
