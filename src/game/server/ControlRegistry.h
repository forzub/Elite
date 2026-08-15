#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "src/game/identity/PlayerId.h"
#include "src/scene/EntityID.h"

namespace game::server
{
enum class ControllerKind : std::uint8_t
{
    None = 0,
    Human,
    AI,
    Autopilot
};

struct ControlAuthority
{
    EntityId entityId {0};
    ControllerKind kind = ControllerKind::None;
    PlayerId playerId {};
};

/*
    Current control ownership is independent from ship identity and from the
    transport session carrying commands. Today only Human binding is wired into
    GameServer; AI/Autopilot are explicit controller classes reserved for the
    same authority axis rather than being encoded as ShipRole::NPC.
*/
class ControlRegistry
{
public:
    bool bindHuman(PlayerId playerId, EntityId entityId)
    {
        if (!playerId || entityId.value == 0)
            return false;

        const auto entityIt = m_byEntity.find(entityId.value);
        if (entityIt != m_byEntity.end() &&
            entityIt->second.playerId != playerId)
        {
            return false;
        }

        const auto playerIt = m_humanEntityByPlayer.find(playerId.value);
        if (playerIt != m_humanEntityByPlayer.end() &&
            playerIt->second != entityId)
        {
            return false;
        }

        ControlAuthority authority;
        authority.entityId = entityId;
        authority.kind = ControllerKind::Human;
        authority.playerId = playerId;

        m_byEntity[entityId.value] = authority;
        m_humanEntityByPlayer[playerId.value] = entityId;
        return true;
    }

    EntityId controlledEntity(PlayerId playerId) const noexcept
    {
        const auto it = m_humanEntityByPlayer.find(playerId.value);
        return it == m_humanEntityByPlayer.end() ? EntityId{} : it->second;
    }

    PlayerId humanPlayerForEntity(EntityId entityId) const noexcept
    {
        const auto it = m_byEntity.find(entityId.value);
        if (it == m_byEntity.end() ||
            it->second.kind != ControllerKind::Human)
        {
            return {};
        }
        return it->second.playerId;
    }

    const ControlAuthority* find(EntityId entityId) const noexcept
    {
        const auto it = m_byEntity.find(entityId.value);
        return it == m_byEntity.end() ? nullptr : &it->second;
    }

    std::size_t size() const noexcept
    {
        return m_byEntity.size();
    }

private:
    std::unordered_map<std::uint32_t, ControlAuthority> m_byEntity;
    std::unordered_map<std::uint64_t, EntityId> m_humanEntityByPlayer;
};
}
