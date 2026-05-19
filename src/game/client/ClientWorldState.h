#pragma once

#include <unordered_map>
#include "src/scene/EntityID.h"
#include "src/game/ship/core/ShipControlState.h"
#include <glm/glm.hpp>
#include <deque>

#include "src/game/simulation/SimulationSnapshot.h"

#include "render/HUD/WorldLabel.h"
#include "src/world/WorldParams.h"

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/sensors/ShipSignalPresentation.h"

#include "src/world/types/SignalReceptionResult.h"
#include "src/game/equipment/radar/RadarContact.h"
#include "src/game/simulation/ShipCoreStatus.h"

#include "scene/EntityID.h"
#include "src/world/types/ObjectType.h"


#include "src/game/geometry/ObjectAssembly.h"

#include <unordered_set>
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"

#include "src/game/visual/VisualShip.h"

struct PendingCommand
{
    uint32_t tick;
    ShipControlState control;
};


struct ClientShipState
{
    EntityId                                        id;
    ShipRole                                        role;
    ObjectType                                      typeId;

    ShipTransform                                   transform;      // 🔥 единственный источник sim state
    ShipTransform                                   renderTransform;
    const ShipDescriptor*                           descriptor = nullptr;

    ShipSignalPresentation                          signalPresentation; 
    std::vector<SignalReceptionResult>              receptions;
    std::vector<game::RadarContact>                 radarContacts;
    
    const game::ship::geometry::ObjectAssembly*     assembly = nullptr;
    
    game::ShipCoreStatus                            shipCoreStatus;

    std::vector<game::simulation::ObjectModuleSnapshot> modules;
    std::vector<game::simulation::StructuralLinkSnapshot> structuralLinks;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> assemblyModules;
    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> detachedFragments;
    std::vector<game::simulation::ObjectRepairJobSnapshot> repairJobs;
    std::unordered_set<std::string>                     hiddenPartIds;
    std::unordered_map<std::string, float> detachedVisualAge;
    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;

    
    
};



struct ClientObjectState
{
    EntityId                                        id;
    ObjectType                                      type;
    glm::vec3                                       position;
    
    // glm::vec3                                       rotation;
    // glm::vec3                                       renderRotation;
    glm::mat4 orientation {1.0f};
    glm::mat4 renderOrientation {1.0f}; 
    
    // текущие (для рендера)
    glm::vec3                                       renderPosition;
    const game::ship::geometry::ObjectAssembly*     assembly = nullptr;

    const IObjectDescriptor*                                        descriptor = nullptr;
    std::vector<game::simulation::ObjectModuleSnapshot>             modules;
    std::vector<game::simulation::StructuralLinkSnapshot> structuralLinks;
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot>     assemblyModules;
    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> detachedFragments;
    std::unordered_set<std::string>                                 hiddenPartIds;
    std::unordered_map<std::string, float> detachedVisualAge;
    std::vector<game::simulation::DebugHitVolumeSnapshot>           debugHitVolumes;
};



class ClientWorldState
{
public:

    void applySnapshot(const SimulationSnapshot& snapshot);
    void update(float dt);

    const std::unordered_map<uint32_t, ClientShipState>& ships() const {return m_ships;}
    const std::unordered_map<uint32_t, ClientObjectState>& objects() const {return m_objects;}

    std::unordered_map<uint32_t, ClientShipState>& ships()
    {
        return m_ships;
    }

    std::vector<game::visual::VisualShip>& visualShips()
    {
        return m_visualShips;
    }

    const std::vector<game::visual::VisualShip>& visualShips() const
    {
        return m_visualShips;
    }

    void clearVisualShips()
    {
        m_visualShips.clear();
    }
    
    void predict(
        EntityId id,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );
    void forceState(const SimulationSnapshot& snapshot);

    void applySoftCorrection(
        EntityId id,
        const glm::vec3& authoritativePos
    );

private:

    std::unordered_map<uint32_t, ClientShipState>   m_ships;
    std::unordered_map<uint32_t, ClientObjectState> m_objects;
    std::vector<game::visual::VisualShip> m_visualShips;
    std::deque<SimulationSnapshot>                  m_snapshotBuffer;
    ShipSignalPresentation                          signalPresentation;

    
    double m_renderDelay = 0.1; // 100 ms
    double m_clientTime = 0.0;

};
