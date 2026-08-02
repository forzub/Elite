#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "src/game/simulation/ShipSnapshot.h"
#include "src/game/simulation/ObjectSnapshot.h"
#include "src/game/simulation/ClientSessionSnapshot.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/WorldSignal.h"
#include "src/render/HUD/WorldLabel.h"



struct SimulationSnapshot
{
    game::network::SnapshotMetadata metadata;

    // Legacy interpolation fields. They mirror metadata and remain until
    // ClientWorldState is migrated to consume SnapshotMetadata directly.
    double                      serverTime = 0.0;
    std::uint64_t               snapshotTick = 0;  // Authoritative server tick.
    std::vector<ShipSnapshot>   ships;
    std::vector<WorldSignal>    signals;
    std::vector<ObjectSnapshot> objects;
    game::simulation::ClientSessionSnapshot session;
};
