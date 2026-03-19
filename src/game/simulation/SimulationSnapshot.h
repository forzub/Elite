#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "src/game/simulation/ShipSnapshot.h"
#include "src/game/simulation/ObjectSnapshot.h"
#include "src/world/WorldSignal.h"
#include "src/render/HUD/WorldLabel.h"



struct SimulationSnapshot
{
    double                      serverTime = 0.0; 
    uint32_t                    snapshotTick;       //---Это копия m_serverTick в snapshot.---
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
    std::vector<ObjectSnapshot> objects;
};
