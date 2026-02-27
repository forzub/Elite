#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "ShipSnapshot.h"
#include "world/WorldSignal.h"
#include "render/HUD/WorldLabel.h"



struct SimulationSnapshot
{
    double                      serverTime = 0.0; 
    uint32_t                    snapshotTick;       //---Это копия m_serverTick в snapshot.---
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
};
