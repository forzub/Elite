#include "LocalLoopbackTransport.h"

#include <cstdlib>
#include <utility>

bool LocalLoopbackTransport::receiveSnapshot(
    SimulationSnapshot& outSnapshot)
{
    if (m_incoming.empty())
        return false;

    outSnapshot = std::move(m_incoming.front());
    m_incoming.pop();
    return true;
}

void LocalLoopbackTransport::publishSnapshotImmediately(
    const SimulationSnapshot& snapshot)
{
    m_incoming.push(snapshot);
    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snapshot.metadata.serverTick;
    m_lastQueuedServerTime = snapshot.metadata.serverTimeSeconds;
}

void LocalLoopbackTransport::publishSnapshot(
    const SimulationSnapshot& snapshot)
{
    const bool snapshotChanged =
        !m_hasLastQueuedSnapshot ||
        snapshot.metadata.serverTick != m_lastQueuedSnapshotTick ||
        snapshot.metadata.serverTimeSeconds != m_lastQueuedServerTime;

    // Do not enqueue the same publication more than once. Duplicate
    // serverTime/serverTick samples make interpolation of moving reference
    // frames look stepped even though the authoritative state is unchanged.
    if (!snapshotChanged)
        return;

    m_latencyBuffer.push_back({
        snapshot,
        m_fakeLatency
    });

    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snapshot.metadata.serverTick;
    m_lastQueuedServerTime = snapshot.metadata.serverTimeSeconds;
}

void LocalLoopbackTransport::update(float dt)
{
    for (auto& snapshot : m_latencyBuffer)
        snapshot.delay -= dt;

    while (!m_latencyBuffer.empty() &&
           m_latencyBuffer.front().delay <= 0.0f)
    {
        if ((rand() / static_cast<float>(RAND_MAX)) < m_packetLoss)
        {
            m_latencyBuffer.erase(m_latencyBuffer.begin());
            continue;
        }

        m_incoming.push(std::move(m_latencyBuffer.front().snapshot));
        m_latencyBuffer.erase(m_latencyBuffer.begin());
    }

    // Existing responses traverse the return leg first. Responses created by
    // requests that arrive during this update start their return latency on
    // the next transport step; one dt must never be charged twice.
    for (auto& delayed : m_timeSyncResponseBuffer)
        delayed.delay -= dt;

    while (!m_timeSyncResponseBuffer.empty() &&
           m_timeSyncResponseBuffer.front().delay <= 0.0f)
    {
        m_timeSyncResponses.push(
            std::move(m_timeSyncResponseBuffer.front().response)
        );
        m_timeSyncResponseBuffer.erase(
            m_timeSyncResponseBuffer.begin()
        );
    }

    for (auto& delayed : m_timeSyncRequestBuffer)
        delayed.delay -= dt;

    while (!m_timeSyncRequestBuffer.empty() &&
           m_timeSyncRequestBuffer.front().delay <= 0.0f)
    {
        m_serverTimeSyncRequests.push(
            m_timeSyncRequestBuffer.front().request
        );
        m_timeSyncRequestBuffer.erase(m_timeSyncRequestBuffer.begin());
    }
}

void LocalLoopbackTransport::sendClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    m_clientMessages.emplace(playerId, msg);
}

bool LocalLoopbackTransport::receiveClientMessage(
    EntityId& outPlayerId,
    game::network::ClientMessage& outMessage)
{
    if (m_clientMessages.empty())
        return false;

    outPlayerId = m_clientMessages.front().first;
    outMessage = std::move(m_clientMessages.front().second);
    m_clientMessages.pop();
    return true;
}

void LocalLoopbackTransport::sendMapRequest(
    const game::network::MapRequest& request)
{
    m_mapRequests.push(request);
}

bool LocalLoopbackTransport::receiveMapRequest(
    game::network::MapRequest& outRequest)
{
    if (m_mapRequests.empty())
        return false;

    outRequest = std::move(m_mapRequests.front());
    m_mapRequests.pop();
    return true;
}

void LocalLoopbackTransport::sendMapResponse(
    game::network::MapResponse response)
{
    m_mapResponses.push(std::move(response));
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
    m_presentationRequests.push(request);
}

bool LocalLoopbackTransport::receivePresentationDataRequest(
    game::network::PresentationDataRequest& outRequest)
{
    if (m_presentationRequests.empty())
        return false;

    outRequest = std::move(m_presentationRequests.front());
    m_presentationRequests.pop();
    return true;
}

void LocalLoopbackTransport::sendPresentationDataResponse(
    game::network::PresentationDataResponse response)
{
    m_presentationResponses.push(std::move(response));
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

void LocalLoopbackTransport::sendTimeSyncRequest(
    const game::network::TimeSyncRequest& request)
{
    m_timeSyncRequestBuffer.push_back({
        request,
        m_fakeLatency
    });
}

bool LocalLoopbackTransport::receiveTimeSyncRequest(
    game::network::TimeSyncRequest& outRequest)
{
    if (m_serverTimeSyncRequests.empty())
        return false;

    outRequest = m_serverTimeSyncRequests.front();
    m_serverTimeSyncRequests.pop();
    return true;
}

void LocalLoopbackTransport::sendTimeSyncResponse(
    game::network::TimeSyncResponse response)
{
    m_timeSyncResponseBuffer.push_back({
        std::move(response),
        m_fakeLatency
    });
}

bool LocalLoopbackTransport::receiveTimeSyncResponse(
    game::network::TimeSyncResponse& outResponse)
{
    if (m_timeSyncResponses.empty())
        return false;

    outResponse = std::move(m_timeSyncResponses.front());
    m_timeSyncResponses.pop();
    return true;
}
