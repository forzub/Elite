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

#include "src/game/geometry/MeshData.h"
#include "src/game/geometry/MeshGPU.h"

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
    
    std::shared_ptr<game::ship::geometry::MeshData> mesh;
    render::MeshGPU*                                gpuMesh = nullptr;
    
    game::ShipCoreStatus                            shipCoreStatus;
};



struct ClientObjectState
{
    EntityId                                        id;
    ObjectType                                      type;
    glm::vec3                                       position;
    glm::vec3                                       rotation;

    // текущие (для рендера)
    glm::vec3                                       renderPosition;
    glm::vec3                                       renderRotation;
    render::MeshGPU*                                gpuMesh = nullptr;
};



class ClientWorldState
{
public:

    void applySnapshot(const SimulationSnapshot& snapshot);
    void update(float dt);

    const std::unordered_map<uint32_t, ClientShipState>& ships() const {return m_ships;}
    const std::unordered_map<uint32_t, ClientObjectState>& objects() const {return m_objects;}
    
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
    std::deque<SimulationSnapshot>                  m_snapshotBuffer;
    ShipSignalPresentation                          signalPresentation;

    
    double m_renderDelay = 0.1; // 100 ms
    double m_clientTime = 0.0;
};
