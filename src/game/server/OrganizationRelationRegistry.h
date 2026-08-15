#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "src/game/identity/OrganizationId.h"

namespace game::server
{
/*
    Non-structural links between organizations. Numerical diplomacy, trust,
    reputation and decision-making parameters are intentionally deferred.
*/
enum class OrganizationRelationKind : std::uint8_t
{
    RegisteredIn = 0,
    LicensedBy,
    MemberOfAlliance,
    AlliedWith,
    HostileTo,
    ContractedWith,
    Recognizes
};

struct OrganizationRelationRecord
{
    OrganizationId from {};
    OrganizationId to {};
    OrganizationRelationKind kind = OrganizationRelationKind::RegisteredIn;
};

class OrganizationRelationRegistry
{
public:
    bool add(
        OrganizationId from,
        OrganizationId to,
        OrganizationRelationKind kind
    )
    {
        if (!from || !to || from == to || has(from, to, kind))
            return false;

        m_records.push_back({from, to, kind});
        return true;
    }

    bool has(
        OrganizationId from,
        OrganizationId to,
        OrganizationRelationKind kind
    ) const noexcept
    {
        return std::any_of(
            m_records.begin(),
            m_records.end(),
            [&](const OrganizationRelationRecord& record)
            {
                return record.from == from &&
                       record.to == to &&
                       record.kind == kind;
            }
        );
    }

    const std::vector<OrganizationRelationRecord>& all() const noexcept
    {
        return m_records;
    }

private:
    std::vector<OrganizationRelationRecord> m_records;
};
}
