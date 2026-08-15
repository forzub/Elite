#pragma once

#include <cstdint>
#include <unordered_map>

#include "src/game/identity/OrganizationId.h"
#include "src/game/identity/PlayerId.h"
#include "src/game/ship/ShipRegistry.h"

namespace game::server
{
enum class ShipOwnerKind : std::uint8_t
{
    None = 0,
    Player,
    Organization
};

struct ShipOwnerRef
{
    ShipOwnerKind kind = ShipOwnerKind::None;
    PlayerId playerId {};
    OrganizationId organizationId {};

    static ShipOwnerRef player(PlayerId id) noexcept
    {
        ShipOwnerRef out;
        out.kind = ShipOwnerKind::Player;
        out.playerId = id;
        return out;
    }

    static ShipOwnerRef organization(OrganizationId id) noexcept
    {
        ShipOwnerRef out;
        out.kind = ShipOwnerKind::Organization;
        out.organizationId = id;
        return out;
    }

    bool valid() const noexcept
    {
        return (kind == ShipOwnerKind::Player && playerId) ||
               (kind == ShipOwnerKind::Organization && organizationId);
    }
};

struct ShipOwnershipRecord
{
    ShipInstanceId shipId = 0;
    ShipOwnerRef owner;
    OrganizationId operatorOrganizationId {};
};

/*
    Legal ownership and operational assignment are independent. The owner may
    be a player or an organization; the operator is an organization and may be
    absent. Runtime control remains exclusively in ControlRegistry.
*/
class ShipOwnershipRegistry
{
public:
    bool assign(
        ShipInstanceId shipId,
        ShipOwnerRef owner,
        OrganizationId operatorOrganizationId = {}
    )
    {
        if (shipId == 0 || !owner.valid())
            return false;

        ShipOwnershipRecord record;
        record.shipId = shipId;
        record.owner = owner;
        record.operatorOrganizationId = operatorOrganizationId;
        m_records[shipId] = record;
        return true;
    }

    const ShipOwnershipRecord* find(ShipInstanceId shipId) const noexcept
    {
        const auto it = m_records.find(shipId);
        return it == m_records.end() ? nullptr : &it->second;
    }

    ShipOwnershipRecord* find(ShipInstanceId shipId) noexcept
    {
        const auto it = m_records.find(shipId);
        return it == m_records.end() ? nullptr : &it->second;
    }

    const std::unordered_map<ShipInstanceId, ShipOwnershipRecord>&
    all() const noexcept
    {
        return m_records;
    }

private:
    std::unordered_map<ShipInstanceId, ShipOwnershipRecord> m_records;
};
}
