#pragma once

#include <cstdint>
#include <string>

#include "src/game/identity/OrganizationId.h"

namespace game::organization
{
/*
    Broad structural classification only. Behavioural traits, budgets,
    diplomacy and policy deliberately do not live here yet.
*/
enum class OrganizationKind : std::uint8_t
{
    Unknown = 0,
    PoliticalUnion,
    State,
    GovernmentBody,
    Company,
    Corporation,
    Military,
    Fleet,
    OperationalGroup,
    PlayerGroup,
    CriminalNetwork,
    AlienPolity,
    Other
};

/*
    parentId means only structural/administrative containment.

    Citizenship, company registration, alliances, contracts and recognition
    are separate typed edges and must never be encoded by parentId.
*/
struct OrganizationRecord
{
    OrganizationId id {};
    OrganizationKind kind = OrganizationKind::Unknown;
    OrganizationId parentId {};
    std::string name;
};
}
