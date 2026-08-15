#pragma once

#include <cstddef>
#include <queue>
#include <utility>

#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/network/ReplicationSnapshotMerge.h"

namespace game::server
{
/*
    Process-local sink/source endpoints for the standalone headless server.

    They deliberately implement only the protocol/debug interfaces.  The
    dedicated process therefore boots the same ServerRuntime as local play
    without importing GameClient, LocalLoopbackTransport, UI or render code.

    This is not the future remote-network transport.  Until a socket transport
    exists, inbound queues are empty and outbound replicated values are retained
    only for diagnostics/self-test.  Replacing this endpoint later must not
    change ServerRuntime/GameServer ownership.
*/
class HeadlessServerTransport final : public IServerTransport
{
public:
    void update(float) override
    {
    }

    bool receiveSessionHello(
        game::network::SessionHello& outHello
    ) override
    {
        if (m_sessionHellos.empty())
            return false;

        outHello = m_sessionHellos.front();
        m_sessionHellos.pop();
        return true;
    }

    bool receiveClientMessage(
        game::network::ClientMessage& outMessage
    ) override
    {
        if (m_clientMessages.empty())
            return false;

        outMessage = std::move(m_clientMessages.front());
        m_clientMessages.pop();
        return true;
    }

    bool receiveMapRequest(
        game::network::MapRequest& outRequest
    ) override
    {
        if (m_mapRequests.empty())
            return false;

        outRequest = std::move(m_mapRequests.front());
        m_mapRequests.pop();
        return true;
    }

    bool receiveTimeSyncRequest(
        game::network::TimeSyncRequest& outRequest
    ) override
    {
        if (m_timeSyncRequests.empty())
            return false;

        outRequest = m_timeSyncRequests.front();
        m_timeSyncRequests.pop();
        return true;
    }

    // Self-test/admission harness injection. In normal standalone server mode
    // nobody calls these methods, so the endpoint remains an empty inbound
    // source until the future real network adapter replaces it.
    void enqueueSessionHello(game::network::SessionHello hello)
    {
        m_sessionHellos.push(hello);
    }

    void enqueueClientMessage(game::network::ClientMessage message)
    {
        m_clientMessages.push(std::move(message));
    }

    void enqueueMapRequest(game::network::MapRequest request)
    {
        m_mapRequests.push(std::move(request));
    }

    void enqueueTimeSyncRequest(
        game::network::TimeSyncRequest request
    )
    {
        m_timeSyncRequests.push(std::move(request));
    }

    void publishSessionWelcomeImmediately(
        const game::network::SessionWelcome& welcome
    ) override
    {
        m_welcome = welcome;
        m_hasWelcome = true;
    }

    void publishSnapshot(
        const SimulationSnapshot& snapshot
    ) override
    {
        retainSnapshot(snapshot);
        ++m_snapshotPublicationCount;
    }

    void publishSnapshotImmediately(
        const SimulationSnapshot& snapshot
    ) override
    {
        retainSnapshot(snapshot);
        m_hasBootstrapSnapshot = true;
        ++m_snapshotPublicationCount;
    }

    void sendMapResponse(
        game::network::MapResponse response
    ) override
    {
        m_latestMapResponse = std::move(response);
        m_hasMapResponse = true;
        ++m_mapResponseCount;
    }

    void sendTimeSyncResponse(
        game::network::TimeSyncResponse response
    ) override
    {
        m_latestTimeSyncResponse = std::move(response);
        m_hasTimeSyncResponse = true;
        ++m_timeSyncResponseCount;
    }

    bool hasSessionWelcome() const noexcept
    {
        return m_hasWelcome;
    }

    bool hasSnapshot() const noexcept
    {
        return m_hasSnapshot;
    }

    bool hasBootstrapSnapshot() const noexcept
    {
        return m_hasBootstrapSnapshot;
    }

    const game::network::SessionWelcome& sessionWelcome() const noexcept
    {
        return m_welcome;
    }

    const SimulationSnapshot& latestSnapshot() const noexcept
    {
        return m_latestSnapshot;
    }

    const SimulationSnapshot& latestCanonicalSnapshot() const noexcept
    {
        return m_latestCanonicalSnapshot;
    }

    std::size_t snapshotPublicationCount() const noexcept
    {
        return m_snapshotPublicationCount;
    }

    bool hasMapResponse() const noexcept
    {
        return m_hasMapResponse;
    }

    const game::network::MapResponse& latestMapResponse() const noexcept
    {
        return m_latestMapResponse;
    }

    std::size_t mapResponseCount() const noexcept
    {
        return m_mapResponseCount;
    }

    bool hasTimeSyncResponse() const noexcept
    {
        return m_hasTimeSyncResponse;
    }

    const game::network::TimeSyncResponse&
    latestTimeSyncResponse() const noexcept
    {
        return m_latestTimeSyncResponse;
    }

    std::size_t timeSyncResponseCount() const noexcept
    {
        return m_timeSyncResponseCount;
    }

private:
    void retainSnapshot(const SimulationSnapshot& snapshot)
    {
        const SimulationSnapshot* previousCanonical =
            m_hasSnapshot ? &m_latestCanonicalSnapshot : nullptr;

        m_latestCanonicalSnapshot =
            game::network::materializeCanonicalReplicationSnapshot(
                previousCanonical,
                snapshot
            );
        m_latestSnapshot = snapshot;
        m_hasSnapshot = true;
    }

    bool m_hasWelcome = false;
    bool m_hasSnapshot = false;
    bool m_hasBootstrapSnapshot = false;
    bool m_hasMapResponse = false;
    bool m_hasTimeSyncResponse = false;

    game::network::SessionWelcome m_welcome;
    SimulationSnapshot m_latestSnapshot;
    SimulationSnapshot m_latestCanonicalSnapshot;
    game::network::MapResponse m_latestMapResponse;
    game::network::TimeSyncResponse m_latestTimeSyncResponse;

    std::queue<game::network::SessionHello> m_sessionHellos;
    std::queue<game::network::ClientMessage> m_clientMessages;
    std::queue<game::network::MapRequest> m_mapRequests;
    std::queue<game::network::TimeSyncRequest> m_timeSyncRequests;

    std::size_t m_snapshotPublicationCount = 0;
    std::size_t m_mapResponseCount = 0;
    std::size_t m_timeSyncResponseCount = 0;
};

class HeadlessDebugChannel final : public game::debug::IServerDebugChannel
{
public:
    bool receiveCommand(
        game::debug::DebugCommand&
    ) override
    {
        return false;
    }

    void publishSnapshot(
        const SimulationSnapshot& snapshot
    ) override
    {
        m_latestSnapshot = snapshot;
        m_hasSnapshot = true;
    }

    void publishState(
        const game::debug::DebugSessionState& state
    ) override
    {
        m_latestState = state;
        m_hasState = true;
    }

    bool hasBootstrapState() const noexcept
    {
        return m_hasSnapshot && m_hasState;
    }

private:
    bool m_hasSnapshot = false;
    bool m_hasState = false;
    SimulationSnapshot m_latestSnapshot;
    game::debug::DebugSessionState m_latestState;
};

} // namespace game::server
