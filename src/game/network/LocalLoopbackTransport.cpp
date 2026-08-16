#include "LocalLoopbackTransport.h"

#include <utility>

void LocalLoopbackTransport::sendSessionHello(
    const game::network::SessionHello& hello)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessionHello.push(hello);
}

bool LocalLoopbackTransport::receiveSessionHello(
    game::network::SessionHello& outHello)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sessionHello.empty())
        return false;
    outHello = m_sessionHello.front();
    m_sessionHello.pop();
    return true;
}

bool LocalLoopbackTransport::receiveSessionReject(
    game::network::SessionReject& outReject)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_sessionReject.empty())
        return false;

    outReject = m_sessionReject.front();
    m_sessionReject.pop();
    return true;
}

void LocalLoopbackTransport::publishSessionRejectImmediately(
    const game::network::SessionReject& reject)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessionReject.push(reject);
}

bool LocalLoopbackTransport::receiveSessionWelcome(
    game::network::SessionWelcome& outWelcome)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_sessionWelcome.empty())
        return false;

    outWelcome = m_sessionWelcome.front();
    m_sessionWelcome.pop();
    return true;
}

void LocalLoopbackTransport::publishSessionWelcomeImmediately(
    const game::network::SessionWelcome& welcome)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sessionWelcome.push(welcome);
}

bool LocalLoopbackTransport::receiveSnapshot(
    SimulationSnapshot& outSnapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_incoming.empty())
        return false;

    outSnapshot = std::move(m_incoming.front());
    m_incoming.pop();
    return true;
}

void LocalLoopbackTransport::publishSnapshotImmediately(
    const SimulationSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_incoming.push(snapshot);
    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snapshot.metadata.serverTick;
    m_lastQueuedServerTime = snapshot.metadata.serverTimeSeconds;
}

void LocalLoopbackTransport::publishSnapshot(
    const SimulationSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);

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
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& snapshot : m_latencyBuffer)
        snapshot.delay -= dt;

    while (!m_latencyBuffer.empty() &&
           m_latencyBuffer.front().delay <= 0.0f)
    {
        if (m_packetLoss > 0.0f)
        {
            std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
            if (distribution(m_packetLossRng) < m_packetLoss)
            {
                m_latencyBuffer.erase(m_latencyBuffer.begin());
                continue;
            }
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
    const game::network::ClientMessage& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_clientMessages.push(msg);
}

bool LocalLoopbackTransport::receiveClientMessage(
    game::network::ClientMessage& outMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_clientMessages.empty())
        return false;

    outMessage = std::move(m_clientMessages.front());
    m_clientMessages.pop();
    return true;
}

void LocalLoopbackTransport::sendMapRequest(
    const game::network::MapRequest& request)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mapRequests.push(request);
}

bool LocalLoopbackTransport::receiveMapRequest(
    game::network::MapRequest& outRequest)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_mapRequests.empty())
        return false;

    outRequest = std::move(m_mapRequests.front());
    m_mapRequests.pop();
    return true;
}

void LocalLoopbackTransport::sendMapResponse(
    game::network::MapResponse response)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mapResponses.push(std::move(response));
}

bool LocalLoopbackTransport::receiveMapResponse(
    game::network::MapResponse& outResponse)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_mapResponses.empty())
        return false;

    outResponse = std::move(m_mapResponses.front());
    m_mapResponses.pop();
    return true;
}

void LocalLoopbackTransport::sendTimeSyncRequest(
    const game::network::TimeSyncRequest& request)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_timeSyncRequestBuffer.push_back({
        request,
        m_fakeLatency
    });
}

bool LocalLoopbackTransport::receiveTimeSyncRequest(
    game::network::TimeSyncRequest& outRequest)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_serverTimeSyncRequests.empty())
        return false;

    outRequest = m_serverTimeSyncRequests.front();
    m_serverTimeSyncRequests.pop();
    return true;
}

void LocalLoopbackTransport::sendTimeSyncResponse(
    game::network::TimeSyncResponse response)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_timeSyncResponseBuffer.push_back({
        std::move(response),
        m_fakeLatency
    });
}

bool LocalLoopbackTransport::receiveTimeSyncResponse(
    game::network::TimeSyncResponse& outResponse)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_timeSyncResponses.empty())
        return false;

    outResponse = std::move(m_timeSyncResponses.front());
    m_timeSyncResponses.pop();
    return true;
}
