#include "LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>
#include <type_traits>
#include <utility>


LocalLoopbackTransport::LocalLoopbackTransport(GameServer& server)
    : m_server(server)
{
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
    const SimulationSnapshot snap = m_server.snapshot();

    m_incoming.push(snap);
    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snap.metadata.serverTick;
    m_lastQueuedServerTime = snap.metadata.serverTimeSeconds;
}


void LocalLoopbackTransport::update(float dt)
{
    const SimulationSnapshot snap = m_server.snapshot();

    const bool snapshotChanged =
        !m_hasLastQueuedSnapshot ||
        snap.metadata.serverTick != m_lastQueuedSnapshotTick ||
        snap.metadata.serverTimeSeconds != m_lastQueuedServerTime;

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
        m_lastQueuedSnapshotTick = snap.metadata.serverTick;
        m_lastQueuedServerTime = snap.metadata.serverTimeSeconds;
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
    while (m_server.popMapResponse(response))
        m_mapResponses.push(std::move(response));

    game::network::PresentationDataResponse presentationResponse;
    while (m_server.popPresentationDataResponse(presentationResponse))
        m_presentationResponses.push(std::move(presentationResponse));
}


void LocalLoopbackTransport::sendClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    m_server.receiveClientMessage(playerId, msg);
}


void LocalLoopbackTransport::sendMapRequest(
    const game::network::MapRequest& request)
{
    m_server.enqueueMapRequest(request);
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

void LocalLoopbackTransport::sendPresentationDataRequest(
    const game::network::PresentationDataRequest& request)
{
    m_server.enqueuePresentationDataRequest(request);
}

bool LocalLoopbackTransport::receivePresentationDataResponse(
    game::network::PresentationDataResponse& outResponse)
{
    if (m_presentationResponses.empty())
        return false;

    outResponse = std::move(m_presentationResponses.front());
    m_presentationResponses.pop();
    return true;
}
