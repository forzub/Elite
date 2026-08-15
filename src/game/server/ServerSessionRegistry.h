#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "src/game/identity/PlayerId.h"
#include "src/game/network/SessionMessage.h"

namespace game::server
{
/*
    Authoritative transport/session -> persistent player identity registry.

    SessionId is transient connection identity. PlayerId is persistent character
    identity. The controlled runtime EntityId is intentionally NOT stored here:
    GameServer resolves session -> PlayerId -> ControlRegistry -> EntityId.
    This prevents a transport session from becoming the identity of a ship.
*/
struct ServerSessionState
{
    game::network::ServerSessionId id {};
    PlayerId playerId {};
    bool connected = false;
};

class ServerSessionRegistry
{
public:
    game::network::ServerSessionId create(PlayerId playerId)
    {
        if (!playerId || isConnectedPlayer(playerId))
            return {};

        const game::network::ServerSessionId id {m_nextSessionId++};

        ServerSessionState state;
        state.id = id;
        state.playerId = playerId;
        state.connected = true;

        m_sessions.emplace(id.value, state);
        return id;
    }

    const ServerSessionState* find(
        game::network::ServerSessionId id
    ) const noexcept
    {
        const auto it = m_sessions.find(id.value);
        return it == m_sessions.end() ? nullptr : &it->second;
    }

    ServerSessionState* find(
        game::network::ServerSessionId id
    ) noexcept
    {
        const auto it = m_sessions.find(id.value);
        return it == m_sessions.end() ? nullptr : &it->second;
    }

    PlayerId player(
        game::network::ServerSessionId id
    ) const noexcept
    {
        const auto* state = find(id);
        if (!state || !state->connected)
            return {};

        return state->playerId;
    }

    bool disconnect(game::network::ServerSessionId id) noexcept
    {
        auto* state = find(id);
        if (!state || !state->connected)
            return false;

        state->connected = false;
        return true;
    }

    bool reconnect(game::network::ServerSessionId id) noexcept
    {
        auto* state = find(id);
        if (!state || state->connected)
            return false;

        if (isConnectedPlayer(state->playerId))
            return false;

        state->connected = true;
        return true;
    }

    bool isConnectedPlayer(PlayerId playerId) const noexcept
    {
        if (!playerId)
            return false;

        for (const auto& [id, state] : m_sessions)
        {
            (void)id;
            if (state.connected && state.playerId == playerId)
                return true;
        }

        return false;
    }

    std::size_t connectedCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto& [id, state] : m_sessions)
        {
            (void)id;
            if (state.connected)
                ++count;
        }
        return count;
    }

    std::size_t size() const noexcept
    {
        return m_sessions.size();
    }

private:
    std::uint64_t m_nextSessionId = 1;
    std::unordered_map<std::uint64_t, ServerSessionState> m_sessions;
};
}
