#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "src/game/simulation/ShipSnapshot.h"
#include "src/game/simulation/ObjectSnapshot.h"
#include "src/game/simulation/ClientSessionSnapshot.h"
#include "src/world/WorldSignal.h"
#include "src/render/HUD/WorldLabel.h"



struct SimulationSnapshot
{
    double                      serverTime = 0.0;
    std::uint64_t               snapshotTick = 0;  // Authoritative server tick.
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
    std::vector<ObjectSnapshot> objects;
    game::simulation::ClientSessionSnapshot session;
};
