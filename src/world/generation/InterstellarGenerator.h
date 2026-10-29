#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/generation/GenerationVersions.h"

namespace world::generation
{

/*
    Interstellar content between the star systems.

    Compact bodies (rogue planets and moons, binary rogues, brown dwarfs,
    stellar remnants, interstellar comets, debris groups) are generated per
    25-ly galactic cell from GalacticCellSeed: the same cell always gives the
    same objects and a new cell never changes an old one. Only the notable
    members of the physical population are materialised, at a low rate; the
    rest stays statistical.

    Interstellar medium regions are volumetric descriptors (ellipsoids) of
    the real Local Bubble environment (Local Bubble, Local Interstellar Cloud,
    G cloud, Blue cloud, Leo cold cloud; Redfield & Linsky 2008, Frisch 2011,
    Peek et al. 2011; geometry approximate) plus rare procedural warm
    cloudlets. Composition is H/He with trace species; resources derive from
    the physical descriptor. Diffuse regions carry no gameplay gravity.

    Output uses the existing galaxy_object schema (objects_details/), so the
    StarAtlas and the Galaxy Map snapshot load it unchanged.
*/

struct InterstellarObject
{
    std::string id;
    std::string name;
    std::string objectType;   // rogue_planet | rogue_moon | binary_rogue_planet | brown_dwarf |
                              // stellar_remnant | interstellar_comet | debris_group |
                              // interstellar_medium_region
    glm::dvec3 positionLy {0.0};
    std::string description;
    std::vector<std::string> tags;
    nlohmann::ordered_json properties = nlohmann::ordered_json::object();
};

struct InterstellarSettings
{
    double radiusLy = 120.0;               // materialised sphere around Sol
    double cellSizeLy = 25.0;
    double minDistanceToSystemLy = 2.0;    // never on top of a catalog system
    double surveyCompleteBrownDwarfLy = 20.0; // WISE-complete for warm brown dwarfs
};

std::vector<InterstellarObject> generateInterstellarObjects(
    std::uint64_t universeSeed,
    const std::vector<glm::dvec3>& catalogSystemPositionsLy,
    const InterstellarSettings& settings = {},
    const UniverseGenerationVersions& versions = CurrentGenerationVersions);

std::vector<InterstellarObject> generateInterstellarMedium(
    std::uint64_t universeSeed,
    const InterstellarSettings& settings = {},
    const UniverseGenerationVersions& versions = CurrentGenerationVersions);

// Galactic (l, b in degrees, distance in ly) -> ICRS Cartesian light-years.
glm::dvec3 galacticToIcrsLy(double lDeg, double bDeg, double distanceLy);

} // namespace world::generation
