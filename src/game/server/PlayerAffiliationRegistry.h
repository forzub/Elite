#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "src/game/identity/OrganizationId.h"
#include "src/game/identity/PlayerId.h"

namespace game::server
{
enum class PlayerAffiliationKind : std::uint8_t
{
    Citizenship = 0,
    Membership,
    Employment
};

struct PlayerAffiliationRecord
{
    PlayerId playerId {};
    OrganizationId organizationId {};
    PlayerAffiliationKind kind = PlayerAffiliationKind::Membership;
};

/*
    Citizenship and voluntary/contractual organization membership are separate
    typed edges. A player may have several of either and they do not change the
    structural organization tree.
*/
class PlayerAffiliationRegistry
{
public:
    bool add(
        PlayerId playerId,
        OrganizationId organizationId,
        PlayerAffiliationKind kind
    )
    {
        if (!playerId || !organizationId || has(playerId, organizationId, kind))
            return false;

        m_records.push_back({playerId, organizationId, kind});
        return true;
    }

    bool has(
        PlayerId playerId,
        OrganizationId organizationId,
        PlayerAffiliationKind kind
    ) const noexcept
    {
        return std::any_of(
            m_records.begin(),
            m_records.end(),
            [&](const PlayerAffiliationRecord& record)
            {
                return record.playerId == playerId &&
                       record.organizationId == organizationId &&
                       record.kind == kind;
            }
        );
    }

    const std::vector<PlayerAffiliationRecord>& all() const noexcept
    {
        return m_records;
    }

private:
    std::vector<PlayerAffiliationRecord> m_records;
};
}
