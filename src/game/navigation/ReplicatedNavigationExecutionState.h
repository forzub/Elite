#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "src/game/simulation/NavigationExecutionSnapshot.h"
#include "src/game/identity/ShipInstanceId.h"
#include "src/game/navigation/NavigationAssetRef.h"
#include "src/scene/EntityID.h"

namespace game::navigation
{

struct ReplicatedNavigationExecution
{
    EntityId entityId {};
    ShipInstanceId shipInstanceId = 0;
    game::simulation::NavigationExecutionSnapshot execution {};
};

// Client read-only mirror of server-executed Navigation v2 command truth.
//
// Presentation/debug code may inspect it. Client planners must not publish into
// it and must not reinterpret it as a locally accepted maneuver.
class ReplicatedNavigationExecutionState final
{
public:
    void replace(std::vector<ReplicatedNavigationExecution> entries)
    {
        m_byEntity.clear();
        m_entityByShipInstance.clear();
        m_byEntity.reserve(entries.size());
        m_entityByShipInstance.reserve(entries.size());

        for (auto& entry : entries)
        {
            if (!entry.execution.valid || entry.entityId.value == 0)
                continue;

            m_byEntity[entry.entityId.value] = entry;
            if (entry.shipInstanceId != 0)
                m_entityByShipInstance[entry.shipInstanceId] =
                    entry.entityId.value;
        }
    }

    const ReplicatedNavigationExecution* find(
        EntityId entityId
    ) const noexcept
    {
        const auto it = m_byEntity.find(entityId.value);
        return it == m_byEntity.end() ? nullptr : &it->second;
    }

    const ReplicatedNavigationExecution* find(
        const NavigationAssetRef& asset
    ) const noexcept
    {
        if (asset.kind != NavigationAssetKind::Ship ||
            asset.shipInstanceId == 0)
        {
            return nullptr;
        }

        const auto entityIt =
            m_entityByShipInstance.find(asset.shipInstanceId);
        if (entityIt == m_entityByShipInstance.end())
            return nullptr;

        const auto it = m_byEntity.find(entityIt->second);
        return it == m_byEntity.end() ? nullptr : &it->second;
    }

    const std::unordered_map<
        std::uint32_t,
        ReplicatedNavigationExecution
    >& all() const noexcept
    {
        return m_byEntity;
    }

    void clear() noexcept
    {
        m_byEntity.clear();
        m_entityByShipInstance.clear();
    }

private:
    std::unordered_map<
        std::uint32_t,
        ReplicatedNavigationExecution
    > m_byEntity;

    std::unordered_map<ShipInstanceId, std::uint32_t>
        m_entityByShipInstance;
};

} // namespace game::navigation
