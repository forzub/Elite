#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "src/game/network/SessionMessage.h"
#include "src/scene/EntityID.h"

namespace game::server
{
/*
    Authoritative connection/session ownership registry.

    Session identity is server-owned and independent from EntityId.  A transport
    connection is bound to a session once; gameplay packets carry intent only
    and are resolved through this registry before they can reach a controlled
    entity.  The registry deliberately uses only standard C++ data structures:
    socket handles, HWNDs, file descriptors and other platform details belong
    in transport adapters, never in authoritative gameplay ownership.
*/
struct ServerSessionState
{
    game::network::ServerSessionId id {};
    EntityId controlledEntityId {0};
    bool connected = false;
};

class ServerSessionRegistry
{
public:
    game::network::ServerSessionId create(EntityId controlledEntityId)
    {
        if (controlledEntityId.value == 0 ||
            isControlledEntity(controlledEntityId))
        {
            return {};
        }

        const game::network::ServerSessionId id {m_nextSessionId++};

        ServerSessionState state;
        state.id = id;
        state.controlledEntityId = controlledEntityId;
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

    EntityId controlledEntity(
        game::network::ServerSessionId id
    ) const noexcept
    {
        const auto* state = find(id);
        if (!state || !state->connected)
            return EntityId{};

        return state->controlledEntityId;
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

        // A disconnected session may be superseded by a new server-side
        // admission/handoff for the same entity. Never let the stale session
        // reconnect and create two live command authorities.
        for (const auto& [otherId, other] : m_sessions)
        {
            if (otherId == id.value)
                continue;

            if (other.connected &&
                other.controlledEntityId == state->controlledEntityId)
            {
                return false;
            }
        }

        state->connected = true;
        return true;
    }

    bool isControlledEntity(EntityId entityId) const noexcept
    {
        if (entityId.value == 0)
            return false;

        for (const auto& [id, state] : m_sessions)
        {
            (void)id;
            if (state.connected &&
                state.controlledEntityId == entityId)
            {
                return true;
            }
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
