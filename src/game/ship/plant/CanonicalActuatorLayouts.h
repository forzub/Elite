#pragma once

// The six fixed ordinary-flight actuator layouts (Assisted / Newtonian x
// I / Y / X): slot ids, roles, force directions and freedoms, plus their
// CANONICAL normalized mount coordinates, the eight-node x three-nozzle
// Newtonian RCS bank and the generator / ship-service installation skeleton.
//
// Structure (count, ids, roles, directions, generator wiring) is the model.
// Coordinates here are only the canonical defaults: a real ship's loadout
// carries its own positions from its design passport and registers them as
// an ActuatorTopology (ActuatorTopology.h). Every hull - Cobrett included -
// uses these tables through the same path; nothing here is ship-specific.

#include <array>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::plant::canonical
{
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


// All THREE Newtonian layouts have eight IDENTICAL physical RCS mount
// nodes; each owns three fixed, independently damageable nozzle modules.
// This conservative capability aggregate must never invent a fourth force
// axis in any module. Native execution uses the 24 nozzle geometry directly.
static const std::vector<EngineSlotDefinition> NewtonianRcsNodes = {
        {"rcs_fore_upper_left",EngineSlotRole::ManoeuvreCluster,{-1.0,1.0,-1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_fore_upper_right",EngineSlotRole::ManoeuvreCluster,{1.0,1.0,-1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_fore_lower_left",EngineSlotRole::ManoeuvreCluster,{-1.0,-1.0,-1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_fore_lower_right",EngineSlotRole::ManoeuvreCluster,{1.0,-1.0,-1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_aft_upper_left",EngineSlotRole::ManoeuvreCluster,{-1.0,1.0,1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_aft_upper_right",EngineSlotRole::ManoeuvreCluster,{1.0,1.0,1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_aft_lower_left",EngineSlotRole::ManoeuvreCluster,{-1.0,-1.0,1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
        {"rcs_aft_lower_right",EngineSlotRole::ManoeuvreCluster,{1.0,-1.0,1.0},{0.0,0.0,0.0},EngineSlotFreedom::OmnidirectionalCluster},
};
static const std::vector<EngineSlotDefinition> NewtonianI = [] {
    std::vector<EngineSlotDefinition> slots {
        {"main_center",EngineSlotRole::AftMain,{0.0,0.0,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain}
    };
    slots.insert(slots.end(),NewtonianRcsNodes.begin(),NewtonianRcsNodes.end());
    return slots;
}();
static const std::vector<EngineSlotDefinition> NewtonianY = [] {
    std::vector<EngineSlotDefinition> slots {
        {"main_top",EngineSlotRole::AftMain,{0.0,NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain},
        {"main_lower_left",EngineSlotRole::AftMain,{-Sqrt3Over2*NewtonMainRadialScale,-0.5*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain},
        {"main_lower_right",EngineSlotRole::AftMain,{Sqrt3Over2*NewtonMainRadialScale,-0.5*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain}
    };
    slots.insert(slots.end(),NewtonianRcsNodes.begin(),NewtonianRcsNodes.end());
    return slots;
}();
static const std::vector<EngineSlotDefinition> NewtonianX = [] {
    std::vector<EngineSlotDefinition> slots {
        {"main_upper_left",EngineSlotRole::AftMain,{-InvSqrt2*NewtonMainRadialScale,InvSqrt2*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain},
        {"main_upper_right",EngineSlotRole::AftMain,{InvSqrt2*NewtonMainRadialScale,InvSqrt2*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain},
        {"main_lower_left",EngineSlotRole::AftMain,{-InvSqrt2*NewtonMainRadialScale,-InvSqrt2*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain},
        {"main_lower_right",EngineSlotRole::AftMain,{InvSqrt2*NewtonMainRadialScale,-InvSqrt2*NewtonMainRadialScale,1.0},{0.0,0.0,-1.0},EngineSlotFreedom::VectoringMain}
    };
    slots.insert(slots.end(),NewtonianRcsNodes.begin(),NewtonianRcsNodes.end());
    return slots;
}();

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
 struct InstallationSlots
 {
     std::vector<EngineReactorSlotDefinition> engineReactors;
     ShipServiceSlotDefinition shipReactor;
     ShipServiceSlotDefinition commonCooling;
 };

 [[nodiscard]] inline const InstallationSlots& installationSlots(
     PhysicalControlModelId model)
 {
     // Assisted: one 5 GW generator shared by each fore/aft pair.
     // Newtonian: one generator per aft main engine. RCS and ordinary
     // equipment draw from the separate ship power/cooling buses.
     static const InstallationSlots assistedI {
         {{"drive_left",{-0.70,0.0,0.0},{"aft_left","fore_left"}},
          {"drive_right",{0.70,0.0,0.0},{"aft_right","fore_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const InstallationSlots assistedY {
         {{"drive_top",{0.0,0.70,0.0},{"aft_top","fore_top"}},
          {"drive_lower_left",{-0.61,-0.35,0.0},{"aft_lower_left","fore_lower_left"}},
          {"drive_lower_right",{0.61,-0.35,0.0},{"aft_lower_right","fore_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const InstallationSlots assistedX {
         {{"drive_upper_left",{-0.50,0.50,0.0},{"aft_upper_left","fore_upper_left"}},
          {"drive_upper_right",{0.50,0.50,0.0},{"aft_upper_right","fore_upper_right"}},
          {"drive_lower_left",{-0.50,-0.50,0.0},{"aft_lower_left","fore_lower_left"}},
          {"drive_lower_right",{0.50,-0.50,0.0},{"aft_lower_right","fore_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const InstallationSlots newtonianI {
         {{"drive_center",{0.0,0.0,0.65},{"main_center"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const InstallationSlots newtonianY {
         {{"drive_top",{0.0,0.12,0.65},{"main_top"}},
          {"drive_lower_left",{-0.104,-0.06,0.65},{"main_lower_left"}},
          {"drive_lower_right",{0.104,-0.06,0.65},{"main_lower_right"}}},
         {"ship_reactor",{0.0,0.0,0.15}},
         {"shared_cooling",{0.0,0.0,0.0}}
     };
     static const InstallationSlots newtonianX {
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

struct RcsNozzleMount
{
    std::string_view nozzleId;
    std::string_view nodeId;
    glm::dvec3 positionBody;
    glm::dvec3 forceDirectionBody;
};

inline const std::array<RcsNozzleMount,24>& newtonianRcsNozzles() noexcept
{
    static const std::array<RcsNozzleMount,24> nozzles {{
        {"rcs_fore_upper_left_x", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_fore_upper_left_y", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {0.0, 1.0, 0.0}},
        {"rcs_fore_upper_left_z", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_upper_right_x", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {1.0, 0.0, 0.0}},
        {"rcs_fore_upper_right_y", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {0.0, 1.0, 0.0}},
        {"rcs_fore_upper_right_z", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_lower_left_x", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_fore_lower_left_y", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}},
        {"rcs_fore_lower_left_z", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_lower_right_x", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}},
        {"rcs_fore_lower_right_y", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}},
        {"rcs_fore_lower_right_z", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_aft_upper_left_x", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_aft_upper_left_y", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {0.0, 1.0, 0.0}},
        {"rcs_aft_upper_left_z", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_upper_right_x", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}},
        {"rcs_aft_upper_right_y", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {0.0, 1.0, 0.0}},
        {"rcs_aft_upper_right_z", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_lower_left_x", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_aft_lower_left_y", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {0.0, -1.0, 0.0}},
        {"rcs_aft_lower_left_z", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_lower_right_x", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {1.0, 0.0, 0.0}},
        {"rcs_aft_lower_right_y", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {0.0, -1.0, 0.0}},
        {"rcs_aft_lower_right_z", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {0.0, 0.0, 1.0}},
    }};
    return nozzles;
}
} // namespace game::ship::plant::canonical
