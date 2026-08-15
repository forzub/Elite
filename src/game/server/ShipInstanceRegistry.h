#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "src/game/ship/ShipRegistry.h"
#include "src/game/ship/ShipRoleType.h"
#include "src/scene/EntityID.h"
#include "src/world/types/ObjectType.h"

namespace game::server
{
struct ShipInstanceRecord
{
    ShipInstanceId instanceId = 0;
    EntityId materializedEntityId {0};
    ObjectType typeId = ObjectType::None;
    ShipRoleType roleType = ShipRoleType::Unknown;
    ActorId ownerActor = 0;
    std::string name;
    std::string registrationId;

    bool materialized() const noexcept
    {
        return materializedEntityId.value != 0;
    }
};

/*
    Persistent universe registry for concrete ship instances.

    ShipInstanceId is stable world identity. EntityId is only the current
    materialized simulation handle and may change after dematerialization /
    rematerialization when Active/Prewarm/Coarse/Scheduled runtime tiers are
    introduced.
*/
class ShipInstanceRegistry
{
public:
    bool registerMaterialized(ShipInstanceRecord record)
    {
        if (record.instanceId == 0 || record.materializedEntityId.value == 0)
            return false;

        const auto entityIt = m_entityToInstance.find(
            record.materializedEntityId.value
        );
        if (entityIt != m_entityToInstance.end() &&
            entityIt->second != record.instanceId)
        {
            return false;
        }

        const auto existingIt = m_ships.find(record.instanceId);
        if (existingIt != m_ships.end() &&
            existingIt->second.materialized() &&
            existingIt->second.materializedEntityId !=
                record.materializedEntityId)
        {
            return false;
        }

        if (existingIt != m_ships.end() &&
            existingIt->second.materializedEntityId.value != 0)
        {
            m_entityToInstance.erase(
                existingIt->second.materializedEntityId.value
            );
        }

        m_entityToInstance[record.materializedEntityId.value] =
            record.instanceId;
        m_ships[record.instanceId] = std::move(record);
        return true;
    }

    bool markDematerialized(ShipInstanceId instanceId)
    {
        auto* record = find(instanceId);
        if (!record || !record->materialized())
            return false;

        m_entityToInstance.erase(record->materializedEntityId.value);
        record->materializedEntityId = EntityId{};
        return true;
    }

    const ShipInstanceRecord* find(ShipInstanceId instanceId) const noexcept
    {
        const auto it = m_ships.find(instanceId);
        return it == m_ships.end() ? nullptr : &it->second;
    }

    ShipInstanceRecord* find(ShipInstanceId instanceId) noexcept
    {
        const auto it = m_ships.find(instanceId);
        return it == m_ships.end() ? nullptr : &it->second;
    }

    ShipInstanceId instanceForEntity(EntityId entityId) const noexcept
    {
        const auto it = m_entityToInstance.find(entityId.value);
        return it == m_entityToInstance.end() ? 0 : it->second;
    }

    EntityId materializedEntity(ShipInstanceId instanceId) const noexcept
    {
        const auto* record = find(instanceId);
        return record ? record->materializedEntityId : EntityId{};
    }

    const std::unordered_map<ShipInstanceId, ShipInstanceRecord>&
    all() const noexcept
    {
        return m_ships;
    }

    std::size_t size() const noexcept
    {
        return m_ships.size();
    }

private:
    std::unordered_map<ShipInstanceId, ShipInstanceRecord> m_ships;
    std::unordered_map<std::uint32_t, ShipInstanceId> m_entityToInstance;
};
}
