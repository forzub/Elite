#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "scene/EntityID.h"
#include "world/WorldSignal.h"
#include "src/game/ship/core/ShipRole.h"
#include "render/HUD/WorldLabel.h"
#include "src/game/ship/ShipTypeId.h"
#include "game/ship/core/ShipTransform.h"
#include "src/world/types/SignalReceptionResult.h"

struct ShipSnapshot
{
    EntityId id;
    ShipRole role;
    ShipTypeId typeId;
    
    ShipTransform transform;
    std::vector<SignalReceptionResult> receptions;
};

struct SimulationSnapshot
{
    double                      serverTime = 0.0; 
    uint32_t                    snapshotTick;       //---Это копия m_serverTick в snapshot.---
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
};
