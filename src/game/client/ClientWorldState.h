#pragma once

#include <unordered_map>
#include "src/scene/EntityID.h"
#include "src/game/ship/core/ShipControlState.h"
#include <glm/glm.hpp>
#include <deque>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/ship/core/ShipRole.h"
#include "render/HUD/WorldLabel.h"

#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"
#include "src/game/ship/ShipDescriptor.h"

struct PendingCommand
{
    uint32_t tick;
    ShipControlState control;
};


struct ClientShipState
{
    EntityId id;
    ShipRole role;

    ShipTransform transform;      // 🔥 единственный источник sim state
    ShipTransform renderTransform;
    const ShipDescriptor* descriptor = nullptr;

    std::vector<WorldLabel> labels;
};

class ClientWorldState
{
public:

    void applySnapshot(const SimulationSnapshot& snapshot);
    void update(float dt);
    const std::unordered_map<uint32_t, ClientShipState>& ships() const;
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

    std::unordered_map<uint32_t, ClientShipState> m_ships;
    std::deque<SimulationSnapshot> m_snapshotBuffer;
    
};
