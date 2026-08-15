#include <cstdlib>
#include <iostream>
#include <string>

#include "src/game/server/OrganizationRegistry.h"
#include "src/game/server/OrganizationRelationRegistry.h"
#include "src/game/server/PlayerAffiliationRegistry.h"
#include "src/game/server/ShipOwnershipRegistry.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}
}

int main()
{
    using game::organization::OrganizationKind;
    using game::server::OrganizationRelationKind;
    using game::server::PlayerAffiliationKind;

    game::server::OrganizationRegistry organizations;

    const OrganizationId unionId = organizations.create(
        OrganizationKind::PoliticalUnion,
        "Union"
    );
    const OrganizationId earthState = organizations.create(
        OrganizationKind::State,
        "Earth State",
        unionId
    );
    const OrganizationId marsState = organizations.create(
        OrganizationKind::State,
        "Mars State",
        unionId
    );

    // A private company is not structurally a child of the state in which it
    // is registered. Its own divisions/fleets do form a structural subtree.
    const OrganizationId company = organizations.create(
        OrganizationKind::Company,
        "Horns and Hooves"
    );
    const OrganizationId cargoFleet = organizations.create(
        OrganizationKind::Fleet,
        "Cargo Fleet",
        company
    );
    const OrganizationId securityGroup = organizations.create(
        OrganizationKind::OperationalGroup,
        "Security Group",
        company
    );

    require(unionId && earthState && marsState && company && cargoFleet && securityGroup,
            "organization tree bootstrap failed");
    require(organizations.isDescendantOf(cargoFleet, company),
            "fleet is not structurally contained by its company");
    require(!organizations.isDescendantOf(company, earthState),
            "company registration was incorrectly modeled as structural containment");

    game::server::OrganizationRelationRegistry relations;
    require(relations.add(
                company,
                earthState,
                OrganizationRelationKind::RegisteredIn),
            "company registration relation failed");
    require(relations.has(
                company,
                earthState,
                OrganizationRelationKind::RegisteredIn),
            "company registration relation was not retained");

    game::server::PlayerAffiliationRegistry affiliations;
    const PlayerId playerA {101};
    const PlayerId playerB {202};

    require(affiliations.add(
                playerA,
                earthState,
                PlayerAffiliationKind::Citizenship),
            "player A citizenship failed");
    require(affiliations.add(
                playerB,
                marsState,
                PlayerAffiliationKind::Citizenship),
            "player B citizenship failed");
    require(affiliations.add(
                playerA,
                company,
                PlayerAffiliationKind::Membership),
            "player A company membership failed");
    require(affiliations.add(
                playerB,
                company,
                PlayerAffiliationKind::Membership),
            "player B company membership failed");
    require(affiliations.has(
                playerA,
                earthState,
                PlayerAffiliationKind::Citizenship) &&
            affiliations.has(
                playerB,
                marsState,
                PlayerAffiliationKind::Citizenship) &&
            affiliations.has(
                playerA,
                company,
                PlayerAffiliationKind::Membership) &&
            affiliations.has(
                playerB,
                company,
                PlayerAffiliationKind::Membership),
            "citizenship and shared organization membership are not independent");

    game::server::ShipOwnershipRegistry ownership;
    require(ownership.assign(
                5001,
                game::server::ShipOwnerRef::organization(company),
                cargoFleet),
            "organization-owned ship assignment failed");
    require(ownership.assign(
                5002,
                game::server::ShipOwnerRef::player(playerA)),
            "player-owned ship assignment failed");

    const auto* cargoShip = ownership.find(5001);
    require(cargoShip &&
            cargoShip->owner.kind == game::server::ShipOwnerKind::Organization &&
            cargoShip->owner.organizationId == company &&
            cargoShip->operatorOrganizationId == cargoFleet,
            "ship owner and operator were not kept independent");

    // Gameplay organization hierarchy is intentionally bounded. Lore can
    // describe omitted bureaucracy without materializing every administrative
    // level as an OrganizationRecord.
    game::server::OrganizationRegistry depthBound;
    OrganizationId parent = depthBound.create(OrganizationKind::State, "L1");
    require(static_cast<bool>(parent), "depth-bound root creation failed");

    for (std::size_t depth = 2;
         depth <= game::server::OrganizationRegistry::MaxStructuralDepth;
         ++depth)
    {
        const OrganizationId child = depthBound.create(
            OrganizationKind::GovernmentBody,
            "L" + std::to_string(depth),
            parent
        );
        require(static_cast<bool>(child), "valid organization depth was rejected");
        parent = child;
    }

    require(!depthBound.create(
                OrganizationKind::GovernmentBody,
                "Too deep",
                parent),
            "organization tree exceeded gameplay depth cap");

    // Reparenting to no structural parent is the primitive required for later
    // secession/split mechanics; political consequences are deliberately not
    // part of this structural phase.
    require(organizations.setStructuralParent(securityGroup, {}),
            "organization could not detach from its structural parent");
    require(organizations.structuralDepth(securityGroup) == 1,
            "detached organization did not become a structural root");
    require(!organizations.setStructuralParent(company, cargoFleet),
            "organization cycle was accepted");

    std::cout
        << "[PASS] bounded organization tree + affiliations + relations + ship ownership\n";
    return 0;
}
