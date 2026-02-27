#include "GameServer.h"
#include <iostream>


GameServer::GameServer(){
    m_simulation.update(0.0);
    m_simulation.setTick(0);

    m_lastSnapshot = m_simulation.snapshot();
}






void GameServer::update(double dt)
{
   
    m_serverTick++;

    // 1. Применяем команды
    for (auto& [id, shipPtr] : m_simulation.ships())
    {
        Ship& ship = *shipPtr;
        auto it = m_pendingCommands.find(id.value);
        if (it == m_pendingCommands.end())
            continue;

        auto& queue = it->second;

        while (!queue.empty())
        {
            const auto& cmd = queue.front();

            ship.setControlState(cmd);
            queue.pop_front();
        }
    }


    m_simulation.update(dt);
    m_simulation.setTick(m_serverTick);

    if (m_serverTick % m_snapshotInterval == 0)
    {
        m_lastSnapshot = m_simulation.snapshot();
    }
}





void GameServer::submitCommand(EntityId id,const ShipControlState& control)
{
    m_pendingCommands[id.value].push_back(control);
}






const SimulationSnapshot& GameServer::snapshot() const
{
    // return m_simulation.snapshot();
    return m_lastSnapshot;
}

EntityId GameServer::playerId() const
{
    return m_simulation.playerId();
}

WorldParams& GameServer::world()
{
    return m_simulation.world();
}
