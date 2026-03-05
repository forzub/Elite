#pragma once

#include <unordered_map>
#include <deque>


#include "src/game/simulation/GameSimulation.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/network/ClientMessage.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ClientShipCommand.h"

class GameServer
{
public:
    GameServer();
    
    void update(double dt);

    void submitCommand(EntityId shipId, const ShipControlState& control);

    const SimulationSnapshot& snapshot() const;

    EntityId playerId() const;

    WorldParams& world();

    void receiveClientMessage(EntityId playerId, const game::network::ClientMessage& msg);

private:

    GameSimulation                                                          m_simulation;
    
    
    std::unordered_map<uint32_t, std::deque<ShipControlState>>                      m_pendingCommands;
    std::unordered_map<uint32_t, std::deque<ClientShipCommand>>                     m_pendingClientShipCommands;
    uint32_t                                                                        m_serverTick = 0;
    uint32_t                                                                        m_snapshotInterval = 3;
    SimulationSnapshot                                                              m_lastSnapshot;
};
