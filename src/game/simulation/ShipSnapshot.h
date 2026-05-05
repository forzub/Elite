#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include "src/scene/EntityID.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/world/types/ObjectType.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/types/SignalReceptionResult.h"
#include "src/game/equipment/radar/RadarContact.h"
#include "src/game/damage/DamageEvent.h"
#include "src/game/simulation/ShipCoreStatus.h"
// #include "src/game/geometry/MeshData.h"

#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/ObjectAssemblyModuleSnapshot.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"
#include "src/game/simulation/StructuralLinkSnapshot.h"
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"

struct ShipSnapshot
{
    EntityId                                            id;
    ShipRole                                            role;
    ObjectType                                          typeId;
    
    ShipTransform                                       transform;
    std::vector<SignalReceptionResult>                  receptions;
    std::vector<game::RadarContact>                     radarContacts;
    std::vector<game::damage::DamageEvent>              damageEvents;
    
    // std::shared_ptr<game::ship::geometry::MeshData>     mesh;
    
    game::ShipCoreStatus                                shipCoreStatus;
    std::vector<game::simulation::ObjectModuleSnapshot>   modules;
    std::vector<game::simulation::StructuralLinkSnapshot> structuralLinks;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> assemblyModules;
    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> detachedFragments;
    std::vector<game::simulation::ObjectRepairJobSnapshot> repairJobs;
    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;
};