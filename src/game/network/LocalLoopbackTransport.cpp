#include "LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>


LocalLoopbackTransport::LocalLoopbackTransport(GameServer* server)
    : m_server(server)
{
}



void LocalLoopbackTransport::sendInput(
    EntityId id,
    const ShipControlState& control)
{
    m_server->submitCommand(id, control);
}




bool LocalLoopbackTransport::receiveSnapshot(
    SimulationSnapshot& outSnapshot)
{
    if (m_incoming.empty())
        return false;

    outSnapshot = m_incoming.front();
    m_incoming.pop();
    return true;
}





void LocalLoopbackTransport::update(float dt)
{
    const SimulationSnapshot snap = m_server->snapshot();

    const bool snapshotChanged =
        !m_hasLastQueuedSnapshot ||
        snap.snapshotTick != m_lastQueuedSnapshotTick ||
        snap.serverTime != m_lastQueuedServerTime;

    // ВАЖНО:
    // Не кладём в latency buffer один и тот же snapshot много раз.
    // Иначе ClientWorldState получает дубли serverTime/snapshotTick,
    // а интерполяция вращения станции превращается в ступеньки.
    if (snapshotChanged)
    {
        m_latencyBuffer.push_back({
            snap,
            m_fakeLatency
        });

        m_hasLastQueuedSnapshot = true;
        m_lastQueuedSnapshotTick = snap.snapshotTick;
        m_lastQueuedServerTime = snap.serverTime;
    }

    for (auto& s : m_latencyBuffer)
    {
        s.delay -= dt;
    }

    while (!m_latencyBuffer.empty() &&
           m_latencyBuffer.front().delay <= 0.0f)
    {
        if ((rand() / static_cast<float>(RAND_MAX)) < m_packetLoss)
        {
            m_latencyBuffer.erase(m_latencyBuffer.begin());
            continue;
        }

        m_incoming.push(m_latencyBuffer.front().snapshot);
        m_latencyBuffer.erase(m_latencyBuffer.begin());
    }
}





void LocalLoopbackTransport::sendClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    m_server->receiveClientMessage(playerId, msg);
}