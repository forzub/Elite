#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

/*
    Resolves the player's navigation/presentation domain from one continuous
    galactic position.

    Source convention:
    - sourceSystemId >= 0: sourceWorldPosition is local to that star system.
    - sourceSystemId < 0:  sourceWorldPosition is already galactic-absolute.

    Result convention matches PlayerNavigationState:
    - currentSystemId >= 0: worldPosition/systemLocal* are local to that system.
    - currentSystemId < 0:  worldPosition is galactic-absolute and
      systemLocal* are zero.

    This resolver does not mutate authoritative ShipTransform membership. It
    is intentionally usable by the accelerated diagnostic presentation branch
    without committing diagnostic travel into gameplay state.
*/
struct PlayerSpatialDomainResolution
{
    bool valid = false;
    int currentSystemId = -1;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 systemLocalMeters {0.0};
    glm::dvec3 systemLocalAu {0.0};
    glm::dvec3 galacticPositionLy {0.0};
};

PlayerSpatialDomainResolution resolvePlayerSpatialDomain(
    const std::vector<world::celestial::StarSystemSummary>& systems,
    int sourceSystemId,
    const world::coordinates::WorldPosition& sourceWorldPosition,
    double systemMembershipRadiusAu
);

} // namespace game::navigation
