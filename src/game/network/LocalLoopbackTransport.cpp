#include "LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>
#include <type_traits>
#include <utility>


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


void LocalLoopbackTransport::enqueueCurrentSnapshotImmediately()
{
    const SimulationSnapshot snap = m_server->snapshot();

    m_incoming.push(snap);
    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snap.snapshotTick;
    m_lastQueuedServerTime = snap.serverTime;
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

    game::network::MapResponse response;
    while (m_server->popMapResponse(response))
        m_mapResponses.push(std::move(response));
}


void LocalLoopbackTransport::sendClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    m_server->receiveClientMessage(playerId, msg);
}


void LocalLoopbackTransport::sendMapRequest(
    const game::network::MapRequest& request)
{
    m_server->enqueueMapRequest(request);
}


bool LocalLoopbackTransport::receiveMapResponse(
    game::network::MapResponse& outResponse)
{
    if (m_mapResponses.empty())
        return false;

    outResponse = std::move(m_mapResponses.front());
    m_mapResponses.pop();
    return true;
}

void LocalLoopbackTransport::requestStarAtlas()
{
    game::network::StarAtlasResponse response;
    response.metadata.catalogRevision = m_server->catalogRevision();
    response.atlas = m_server->starAtlas();
    m_starAtlasResponses.push(std::move(response));
}

bool LocalLoopbackTransport::receiveStarAtlas(
    game::network::StarAtlasResponse& outResponse)
{
    if (m_starAtlasResponses.empty())
        return false;

    outResponse = std::move(m_starAtlasResponses.front());
    m_starAtlasResponses.pop();
    return true;
}

void LocalLoopbackTransport::requestCelestialSnapshot()
{
    game::network::CelestialSnapshotResponse response;
    response.metadata = m_server->protocolMetadata();
    response.snapshot = m_server->celestialSnapshot();
    m_celestialResponses.push(std::move(response));
}

bool LocalLoopbackTransport::receiveCelestialSnapshot(
    game::network::CelestialSnapshotResponse& outResponse)
{
    if (m_celestialResponses.empty())
        return false;

    outResponse = std::move(m_celestialResponses.front());
    m_celestialResponses.pop();
    return true;
}
