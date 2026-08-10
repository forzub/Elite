#pragma once

#include <glm/glm.hpp>

namespace world::celestial
{
struct GalaxyMapSnapshot;
struct PlayerNavigationState;
}

namespace game::presentation
{
struct GalaxyPlayerMarkerPosition
{
    glm::dvec3 positionLy {0.0};
    bool insideKnownSystem = false;
};

GalaxyPlayerMarkerPosition resolveGalaxyPlayerMarkerPosition(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation
);
}
