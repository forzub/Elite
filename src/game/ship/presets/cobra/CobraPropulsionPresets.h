#pragma once

// Cobra hull: the module identities of its six installable loadouts. The
// layouts themselves (slots, generator groups, RCS bank) are the generic
// plant::canonical tables; Cobra's coordinates are the canonical ones.

#include <vector>
#include <string>
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"

namespace game::ship::presets::cobra
{
using game::ship::plant::EngineSlotDefinition;
using game::ship::plant::EngineSlotRole;
using game::ship::plant::EngineSlotFreedom;
using game::ship::plant::PhysicalControlModelId;

// Topology lives in plant::canonical (CanonicalActuatorLayouts.h); the Cobra
// names below are aliases kept for the Cobra hull's own loadout assembly.
using plant::canonical::propulsionSlots;
using plant::canonical::installationSlots;
using plant::canonical::EngineReactorSlotDefinition;
using plant::canonical::ShipServiceSlotDefinition;
using CobraInstallationSlots = plant::canonical::InstallationSlots;

 // Real Cobra hardware identities, independent of diagnostic slot state.
 // All three sets exist as persistent ObjectModuleRuntime equipment;
 // only the selected set contributes forces, electrical draw or telemetry.
 [[nodiscard]] inline std::vector<std::string> installedAssistedEngineModuleIds(
     PhysicalControlModelId model)
 {
     if(model == PhysicalControlModelId::AssistedI)
         return {"ship_engine_L","ship_engine_R",
                 "ship_fore_engine_L","ship_fore_engine_R"};
     if(model != PhysicalControlModelId::AssistedY &&
        model != PhysicalControlModelId::AssistedX)
         return {};
     const char* prefix =
         model == PhysicalControlModelId::AssistedY
             ? "ship_assisted_y_" : "ship_assisted_x_";
     std::vector<std::string> ids;
     for(const auto& slot : propulsionSlots(model))
         ids.push_back(std::string(prefix) + slot.id);
     return ids;
 }

 [[nodiscard]] inline std::vector<std::string> installedNewtonianEngineModuleIds(
     PhysicalControlModelId model)
 {
     const char* prefix=nullptr;
     switch(model)
     {
         case PhysicalControlModelId::NewtonianI: prefix="ship_newtonian_i_"; break;
         case PhysicalControlModelId::NewtonianY: prefix="ship_newtonian_y_"; break;
         case PhysicalControlModelId::NewtonianX: prefix="ship_newtonian_x_"; break;
         default: return {};
     }
     std::vector<std::string> ids;
     for(const auto& slot:propulsionSlots(model))
         if(slot.role==EngineSlotRole::AftMain)
             ids.push_back(std::string(prefix)+slot.id);
         else
             // Cluster capability is the minimum of its x/y/z physical
             // nozzles; this binding is the stable leading ID.
             ids.push_back(std::string(slot.id)+"_x");
     return ids;
 }

 [[nodiscard]] inline std::vector<std::string> installedNewtonianReactorModuleIds(
     PhysicalControlModelId model)
 {
     const char* prefix=nullptr;
     switch(model)
     {
         case PhysicalControlModelId::NewtonianI: prefix="ship_newtonian_i_"; break;
         case PhysicalControlModelId::NewtonianY: prefix="ship_newtonian_y_"; break;
         case PhysicalControlModelId::NewtonianX: prefix="ship_newtonian_x_"; break;
         default: return {};
     }
     std::vector<std::string> ids;
     for(const auto& group:installationSlots(model).engineReactors)
         ids.push_back(std::string(prefix)+group.slotId+"_reactor");
     return ids;
 }

 [[nodiscard]] inline std::vector<std::string> installedAssistedReactorModuleIds(
     PhysicalControlModelId model)
 {
     if(model == PhysicalControlModelId::AssistedI)
         return {"ship_drive_left_reactor","ship_drive_right_reactor"};
     if(model != PhysicalControlModelId::AssistedY &&
        model != PhysicalControlModelId::AssistedX)
         return {};
     const char* prefix =
         model == PhysicalControlModelId::AssistedY
             ? "ship_assisted_y_" : "ship_assisted_x_";
     std::vector<std::string> ids;
     for(const auto& group : installationSlots(model).engineReactors)
         ids.push_back(std::string(prefix) + group.slotId + "_reactor");
     return ids;
 }

} // namespace game::ship::presets::cobra
