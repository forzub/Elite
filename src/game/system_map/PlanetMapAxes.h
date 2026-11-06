#pragma once

// Planet-fixed world axes of a Detail-map planet snapshot, shared by the Detail
// map render passes (geometry and planet). Thin wrappers over
// PlanetBodyOrientation.h; no renderer/facade dependency.

#include <glm/glm.hpp>

#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
inline glm::dvec3 planetNorthAxisWorld(
    const world::celestial::DetailMapSnapshot& planet
)
{
    return makePlanetBodyOrientation(
        planet.planetAxialTiltDeg,
        planet.planetAxisNodeDeg
    ).north;
}

inline glm::dvec3 planetPrimeAxisWorld(const glm::dvec3& north)
{
    return planetPrimeAxisFromNorth(north);
}

inline glm::dvec3 planetEastAxisWorld(
    const glm::dvec3& north,
    const glm::dvec3& prime
)
{
    return planetEastAxisFromNorthAndPrime(north, prime);
}
}
