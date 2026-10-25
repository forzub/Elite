#pragma once

// Cobra family: six immutable propulsion installation presets.
// These are the SAME native slots formerly embedded in the flight-law files.
// Body coordinates are normalized mount coordinates, later scaled to hull
// extents and evaluated against the actual current centre of mass.
// Every Newtonian RCS slot is an individual independently damageable
// maneuvering UNIT (currently an omnidirectional cluster, not a single
// directional nozzle). Do not mistake a cluster for a resolved nozzle.
// The hull/engine descriptor supplies the installed module identity and
// equipment passport; this file defines only topology.

#include <vector>
#include <string>
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::presets::cobra
{
using game::ship::plant::EngineSlotDefinition;
using game::ship::plant::EngineSlotRole;
using game::ship::plant::EngineSlotFreedom;
using game::ship::plant::PhysicalControlModelId;

inline const std::vector<EngineSlotDefinition>& propulsionSlots(
    PhysicalControlModelId model)
{
    constexpr double Sqrt3Over2 = 0.86602540378443864676;
    constexpr double InvSqrt2 = 0.70710678118654752440;
    constexpr double NewtonMainRadialScale = 0.12;
    (void)Sqrt3Over2;
    (void)InvSqrt2;
    (void)NewtonMainRadialScale;

static const std::vector<EngineSlotDefinition> AssistedI = {
    {"aft_left",  EngineSlotRole::AftMain, {-1.0, 0.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_right", EngineSlotRole::AftMain, { 1.0, 0.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_left",  EngineSlotRole::ForeMain, {-1.0, 0.0, -1.0}, {0.0, 0.0,  1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_right", EngineSlotRole::ForeMain, { 1.0, 0.0, -1.0}, {0.0, 0.0,  1.0}, EngineSlotFreedom::VectoringMain},
};

static const std::vector<EngineSlotDefinition> AssistedY = {
    {"aft_top",         EngineSlotRole::AftMain, {0.0,          1.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_left",  EngineSlotRole::AftMain, {-Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_right", EngineSlotRole::AftMain, { Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_top",         EngineSlotRole::ForeMain, {0.0,          1.0, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_left",  EngineSlotRole::ForeMain, {-Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_right", EngineSlotRole::ForeMain, { Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
};

static const std::vector<EngineSlotDefinition> AssistedX = {
    {"aft_upper_left",  EngineSlotRole::AftMain, {-InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_upper_right", EngineSlotRole::AftMain, { InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_left",  EngineSlotRole::AftMain, {-InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_right", EngineSlotRole::AftMain, { InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_upper_left",  EngineSlotRole::ForeMain, {-InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_upper_right", EngineSlotRole::ForeMain, { InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_left",  EngineSlotRole::ForeMain, {-InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_right", EngineSlotRole::ForeMain, { InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
};


static const std::vector<EngineSlotDefinition> NewtonianI = {
    {"main_center", EngineSlotRole::AftMain, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_left",  EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_right", EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_left",   EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_right",  EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

static const std::vector<EngineSlotDefinition> NewtonianY = {
    {"main_top",         EngineSlotRole::AftMain, {0.0,          NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-Sqrt3Over2 * NewtonMainRadialScale, -0.5 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { Sqrt3Over2 * NewtonMainRadialScale, -0.5 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_top",         EngineSlotRole::ManoeuvreCluster, {0.0,          1.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_top",          EngineSlotRole::ManoeuvreCluster, {0.0,          1.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

static const std::vector<EngineSlotDefinition> NewtonianX = {
    {"main_upper_left",  EngineSlotRole::AftMain, {-InvSqrt2 * NewtonMainRadialScale,  InvSqrt2 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_upper_right", EngineSlotRole::AftMain, { InvSqrt2 * NewtonMainRadialScale,  InvSqrt2 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-InvSqrt2 * NewtonMainRadialScale, -InvSqrt2 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { InvSqrt2 * NewtonMainRadialScale, -InvSqrt2 * NewtonMainRadialScale, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_upper_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_upper_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

    switch (model)
    {
        case PhysicalControlModelId::AssistedI: return AssistedI;
        case PhysicalControlModelId::AssistedY: return AssistedY;
        case PhysicalControlModelId::AssistedX: return AssistedX;
        case PhysicalControlModelId::NewtonianI: return NewtonianI;
        case PhysicalControlModelId::NewtonianY: return NewtonianY;
        case PhysicalControlModelId::NewtonianX: return NewtonianX;
    }
    return AssistedI;
}

 // Propulsion layout is also the installation skeleton for independent
 // engine generators and the ship's common thermal/power services.
 // Coordinates are normalized and evaluated relative to the actual CoM.
 // Engine IDs refer to the physical main-engine slot names above, not to
 // separate ship-specific equipment instances.
 struct EngineReactorSlotDefinition
 {
     const char* slotId;
     glm::dvec3 normalizedPositionBody;
     std::vector<const char*> poweredEngineSlots;
 };
 struct ShipServiceSlotDefinition
 {
     const char* slotId;
     glm::dvec3 normalizedPositionBody;
 };
 struct CobraInstallationSlots
 {
     std::vector<EngineReactorSlotDefinition> engineReactors;
     ShipServiceSlotDefinition shipReactor;
     ShipServiceSlotDefinition commonCooling;
 };

 [[nodiscard]] inline const CobraInstallationSlots& installationSlots(
     PhysicalControlModelId model)
 {
     // Assisted: one 5 GW generator shared by each fore/aft pair.
     // Newtonian: one generator per aft main engine. RCS and ordinary
     // equipment draw from the separate ship power/cooling buses.
     static const CobraInstallationSlots assistedI {
         {{"drive_left",{-0.70,0.0,0.0},{"aft_left","fore_left"}},
          {"drive_right",{0.70,0.0,0.0},{"aft_right","fore_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const CobraInstallationSlots assistedY {
         {{"drive_top",{0.0,0.70,0.0},{"aft_top","fore_top"}},
          {"drive_lower_left",{-0.61,-0.35,0.0},{"aft_lower_left","fore_lower_left"}},
          {"drive_lower_right",{0.61,-0.35,0.0},{"aft_lower_right","fore_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const CobraInstallationSlots assistedX {
         {{"drive_upper_left",{-0.50,0.50,0.0},{"aft_upper_left","fore_upper_left"}},
          {"drive_upper_right",{0.50,0.50,0.0},{"aft_upper_right","fore_upper_right"}},
          {"drive_lower_left",{-0.50,-0.50,0.0},{"aft_lower_left","fore_lower_left"}},
          {"drive_lower_right",{0.50,-0.50,0.0},{"aft_lower_right","fore_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const CobraInstallationSlots newtonianI {
         {{"drive_center",{0.0,0.0,0.65},{"main_center"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const CobraInstallationSlots newtonianY {
         {{"drive_top",{0.0,0.12,0.65},{"main_top"}},
          {"drive_lower_left",{-0.104,-0.06,0.65},{"main_lower_left"}},
          {"drive_lower_right",{0.104,-0.06,0.65},{"main_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const CobraInstallationSlots newtonianX {
         {{"drive_upper_left",{-0.085,0.085,0.65},{"main_upper_left"}},
          {"drive_upper_right",{0.085,0.085,0.65},{"main_upper_right"}},
          {"drive_lower_left",{-0.085,-0.085,0.65},{"main_lower_left"}},
          {"drive_lower_right",{0.085,-0.085,0.65},{"main_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     switch(model)
     {
         case PhysicalControlModelId::AssistedI: return assistedI;
         case PhysicalControlModelId::AssistedY: return assistedY;
         case PhysicalControlModelId::AssistedX: return assistedX;
         case PhysicalControlModelId::NewtonianI: return newtonianI;
         case PhysicalControlModelId::NewtonianY: return newtonianY;
         case PhysicalControlModelId::NewtonianX: return newtonianX;
     }
     return assistedI;
 }

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
             ids.push_back({}); // Legacy virtual-cluster slot, not an engine.
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
