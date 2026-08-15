#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "src/game/organization/OrganizationRecord.h"

namespace game::server
{
/*
    Persistent structural organization tree.

    The lore may imply arbitrarily deep bureaucracy, but the gameplay model
    intentionally stores only meaningful structural levels. One node has one
    structural parent, with a bounded depth and bounded direct fan-out.
*/
class OrganizationRegistry
{
public:
    static constexpr std::size_t MaxStructuralDepth = 5;
    static constexpr std::size_t MaxDirectChildren = 16;

    OrganizationId create(
        game::organization::OrganizationKind kind,
        std::string name,
        OrganizationId parentId = {}
    )
    {
        if (parentId)
        {
            if (!find(parentId) ||
                structuralDepth(parentId) >= MaxStructuralDepth ||
                directChildCount(parentId) >= MaxDirectChildren)
            {
                return {};
            }
        }

        const OrganizationId id {m_nextOrganizationId++};

        game::organization::OrganizationRecord record;
        record.id = id;
        record.kind = kind;
        record.parentId = parentId;
        record.name = std::move(name);

        m_organizations.emplace(id.value, std::move(record));
        return id;
    }

    const game::organization::OrganizationRecord* find(
        OrganizationId id
    ) const noexcept
    {
        const auto it = m_organizations.find(id.value);
        return it == m_organizations.end() ? nullptr : &it->second;
    }

    game::organization::OrganizationRecord* find(
        OrganizationId id
    ) noexcept
    {
        const auto it = m_organizations.find(id.value);
        return it == m_organizations.end() ? nullptr : &it->second;
    }

    std::size_t structuralDepth(OrganizationId id) const noexcept
    {
        std::size_t depth = 0;
        OrganizationId cursor = id;

        while (cursor)
        {
            const auto* record = find(cursor);
            if (!record)
                return 0;

            ++depth;
            if (depth > m_organizations.size())
                return 0;

            cursor = record->parentId;
        }

        return depth;
    }

    bool isDescendantOf(
        OrganizationId candidate,
        OrganizationId ancestor
    ) const noexcept
    {
        if (!candidate || !ancestor || candidate == ancestor)
            return false;

        const auto* record = find(candidate);
        std::size_t guard = 0;

        while (record && record->parentId)
        {
            if (record->parentId == ancestor)
                return true;

            record = find(record->parentId);
            if (++guard > m_organizations.size())
                return false;
        }

        return false;
    }

    bool setStructuralParent(
        OrganizationId childId,
        OrganizationId newParentId
    )
    {
        auto* child = find(childId);
        if (!child || childId == newParentId)
            return false;

        if (newParentId && !find(newParentId))
            return false;

        if (newParentId && isDescendantOf(newParentId, childId))
            return false;

        if (child->parentId == newParentId)
            return true;

        if (newParentId && directChildCount(newParentId) >= MaxDirectChildren)
            return false;

        const std::size_t newRootDepth =
            newParentId ? structuralDepth(newParentId) + 1 : 1;
        const std::size_t subtreeHeightValue = subtreeHeight(childId);

        if (newRootDepth == 0 || subtreeHeightValue == 0 ||
            newRootDepth + subtreeHeightValue - 1 > MaxStructuralDepth)
        {
            return false;
        }

        child->parentId = newParentId;
        return true;
    }

    std::size_t directChildCount(OrganizationId parentId) const noexcept
    {
        std::size_t count = 0;
        for (const auto& [id, record] : m_organizations)
        {
            (void)id;
            if (record.parentId == parentId)
                ++count;
        }
        return count;
    }

    const std::unordered_map<
        std::uint64_t,
        game::organization::OrganizationRecord
    >& all() const noexcept
    {
        return m_organizations;
    }

    std::size_t size() const noexcept
    {
        return m_organizations.size();
    }

private:
    std::size_t subtreeHeight(OrganizationId rootId) const noexcept
    {
        if (!find(rootId))
            return 0;

        std::size_t maxChildHeight = 0;
        for (const auto& [id, record] : m_organizations)
        {
            (void)id;
            if (record.parentId == rootId)
            {
                const std::size_t childHeight = subtreeHeight(record.id);
                if (childHeight > maxChildHeight)
                    maxChildHeight = childHeight;
            }
        }

        return 1 + maxChildHeight;
    }

    std::uint64_t m_nextOrganizationId = 1;
    std::unordered_map<
        std::uint64_t,
        game::organization::OrganizationRecord
    > m_organizations;
};
}
