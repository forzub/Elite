#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "src/game/simulation/NavigationExecutionSnapshot.h"
#include "src/scene/EntityID.h"

namespace game::navigation
{

struct ReplicatedNavigationExecution
{
    EntityId entityId {};
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
        m_byEntity.reserve(entries.size());

        for (auto& entry : entries)
        {
            if (!entry.execution.valid || entry.entityId.value == 0)
                continue;

            m_byEntity[entry.entityId.value] = entry.execution;
        }
    }

    const game::simulation::NavigationExecutionSnapshot* find(
        EntityId entityId
    ) const noexcept
    {
        const auto it = m_byEntity.find(entityId.value);
        return it == m_byEntity.end() ? nullptr : &it->second;
    }

    const std::unordered_map<
        std::uint32_t,
        game::simulation::NavigationExecutionSnapshot
    >& all() const noexcept
    {
        return m_byEntity;
    }

    void clear() noexcept
    {
        m_byEntity.clear();
    }

private:
    std::unordered_map<
        std::uint32_t,
        game::simulation::NavigationExecutionSnapshot
    > m_byEntity;
};

} // namespace game::navigation
