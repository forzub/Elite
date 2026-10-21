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
} // namespace game::ship::presets::cobra
