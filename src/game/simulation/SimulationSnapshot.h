#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "scene/EntityID.h"
#include "world/WorldSignal.h"
#include "src/game/ship/core/ShipRole.h"
#include "render/HUD/WorldLabel.h"
#include "src/game/ship/ShipTypeId.h"
#include "game/ship/core/ShipTransform.h"

struct ShipSnapshot
{
    EntityId id;
    ShipRole role;
    ShipTypeId typeId;
    
    ShipTransform transform;
    std::vector<WorldLabel> labels;
};

struct SimulationSnapshot
{
    uint32_t                    snapshotTick;       //---Это копия m_serverTick в snapshot.---
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
};
