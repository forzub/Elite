#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "src/game/player/PlayerState.h"

namespace game::server
{
/*
    Persistent player/character identity registry.

    PlayerId survives transport sessions and runtime entity materialization.
    A player is assigned to a persistent ShipInstanceId; the currently
    materialized EntityId is resolved through ShipInstanceRegistry and control
    authority, never stored here as identity.
*/
class PlayerRegistry
{
public:
    PlayerId create(
        ShipInstanceId currentShipId,
        ActorId actor = 0
    )
    {
        if (currentShipId == 0 || playerForShip(currentShipId))
            return {};

        const PlayerId id {m_nextPlayerId++};

        PlayerState state;
        state.id = id;
        state.actor = actor;
        state.currentShipId = currentShipId;

        m_players.emplace(id.value, state);
        m_shipToPlayer.emplace(currentShipId, id);
        return id;
    }

    const PlayerState* find(PlayerId id) const noexcept
    {
        const auto it = m_players.find(id.value);
        return it == m_players.end() ? nullptr : &it->second;
    }

    PlayerState* find(PlayerId id) noexcept
    {
        const auto it = m_players.find(id.value);
        return it == m_players.end() ? nullptr : &it->second;
    }

    PlayerId playerForShip(ShipInstanceId shipId) const noexcept
    {
        const auto it = m_shipToPlayer.find(shipId);
        return it == m_shipToPlayer.end() ? PlayerId{} : it->second;
    }

    bool assignShip(PlayerId playerId, ShipInstanceId shipId)
    {
        auto* player = find(playerId);
        if (!player || shipId == 0)
            return false;

        const PlayerId existing = playerForShip(shipId);
        if (existing && existing != playerId)
            return false;

        if (player->currentShipId != 0)
            m_shipToPlayer.erase(player->currentShipId);

        player->currentShipId = shipId;
        m_shipToPlayer[shipId] = playerId;
        return true;
    }

    const std::unordered_map<std::uint64_t, PlayerState>& all() const noexcept
    {
        return m_players;
    }

    std::size_t size() const noexcept
    {
        return m_players.size();
    }

private:
    std::uint64_t m_nextPlayerId = 1;
    std::unordered_map<std::uint64_t, PlayerState> m_players;
    std::unordered_map<ShipInstanceId, PlayerId> m_shipToPlayer;
};
}
