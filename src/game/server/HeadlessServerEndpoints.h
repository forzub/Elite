#pragma once

#include <cstddef>
#include <utility>

#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"

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

    bool receiveClientMessage(
        game::network::ClientMessage&
    ) override
    {
        return false;
    }

    bool receiveMapRequest(
        game::network::MapRequest&
    ) override
    {
        return false;
    }

    bool receiveTimeSyncRequest(
        game::network::TimeSyncRequest&
    ) override
    {
        return false;
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
        m_latestSnapshot = snapshot;
        m_hasSnapshot = true;
        ++m_snapshotPublicationCount;
    }

    void publishSnapshotImmediately(
        const SimulationSnapshot& snapshot
    ) override
    {
        m_latestSnapshot = snapshot;
        m_hasSnapshot = true;
        m_hasBootstrapSnapshot = true;
        ++m_snapshotPublicationCount;
    }

    void sendMapResponse(
        game::network::MapResponse
    ) override
    {
        ++m_discardedMapResponseCount;
    }

    void sendTimeSyncResponse(
        game::network::TimeSyncResponse
    ) override
    {
        ++m_discardedTimeSyncResponseCount;
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

    std::size_t snapshotPublicationCount() const noexcept
    {
        return m_snapshotPublicationCount;
    }

private:
    bool m_hasWelcome = false;
    bool m_hasSnapshot = false;
    bool m_hasBootstrapSnapshot = false;

    game::network::SessionWelcome m_welcome;
    SimulationSnapshot m_latestSnapshot;

    std::size_t m_snapshotPublicationCount = 0;
    std::size_t m_discardedMapResponseCount = 0;
    std::size_t m_discardedTimeSyncResponseCount = 0;
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
