#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/coordinates/GalacticGrids.h"
#include "src/world/generation/GenerationVersions.h"
#include "src/world/interstellar/DensityLattice.h"

namespace world::generation
{

/*
    Interstellar content between the star systems, materialised per 25-ly
    interstellar generation cell (world::coordinates::interstellarGenerationGrid)
    for the cells covering the navigation domain - not inside a sphere around
    Sol. Every cell is a pure function of GalacticCellSeed: the same cell
    always gives the same content and a new cell never changes an old one.

    Per cell:
    - compact bodies (rogue planets and moons, binary rogues, brown dwarfs,
      stellar remnants, interstellar comets, debris groups) - only the notable
      members of the physical population, at a low rate;
    - the background interstellar medium: a coarse density / phase lattice of
      the continuous field below. Space is never a binary cloud / vacuum.

    Background field (backgroundMediumAt): the Local Bubble cavity of hot
    ionised gas around Sol, bounded by a supernova-enriched neutral wall,
    warm neutral medium beyond. The cavity radius per galactic direction
    follows the 3D dust maps (Lallement et al. 2014, Pelgrims et al. 2020:
    ~60-100 pc in the plane, the beta CMa tunnel toward l ~ 240, open
    chimneys toward the poles), approximate, with seeded low-order roughness.

    Distinct structures are IrregularDensityVolumes (coarse 3D lattice of
    relative density 0..1 from the region seed: irregular boundary, holes,
    clumps); an ellipsoid is only their bounding volume. Authored: Local
    Interstellar Cloud, G cloud, Blue cloud, Leo cold cloud (Redfield & Linsky
    2008, Peek et al. 2011; geometry approximate). Procedural, per cell and by
    the background phase: warm cloudlets inside the cavity, cold neutral
    clouds and molecular clumps in the wall and beyond.

    Output: galaxy_object documents (objects_details/), so the StarAtlas and
    the Galaxy Map snapshot load them unchanged.
*/

// Lattice contract and phase codes: src/world/interstellar/DensityLattice.h.
using world::interstellar::DensityLattice;
using world::interstellar::latticeJson;
using world::interstellar::mediumPhaseName;

struct MediumSample
{
    char phase = 'h';
    double densityCm3 = 0.0;
    double temperatureK = 0.0;
    double ionizationFraction = 0.0;
    double molecularFraction = 0.0;
    double dustToGas = 0.0;
    double iceMantleFraction = 0.0;
    // 0..1: share of recent supernova ejecta (60Fe-bearing dust of the wall).
    double supernovaEnrichment = 0.0;
};

// The continuous background interstellar medium at an ICRS position (ly).
MediumSample backgroundMediumAt(std::uint64_t universeSeed, const glm::dvec3& positionLy);

struct InterstellarObject
{
    std::string id;
    std::string name;
    std::string objectType;   // rogue_planet | rogue_moon | binary_rogue_planet | brown_dwarf |
                              // stellar_remnant | interstellar_comet | debris_group |
                              // interstellar_medium_region | interstellar_medium_background
    glm::dvec3 positionLy {0.0};
    std::string description;
    std::vector<std::string> tags;
    nlohmann::ordered_json properties = nlohmann::ordered_json::object();
};

struct InterstellarSettings
{
    double cellSizeLy = world::coordinates::InterstellarGenerationCellLy;
    double minDistanceToSystemLy = 2.0;    // never on top of a catalog system
    double surveyCompleteBrownDwarfLy = 20.0; // WISE-complete for warm brown dwarfs
    int backgroundLatticeResolution = 5;   // voxels per cell edge (5 ly)
};

struct InterstellarCell
{
    world::coordinates::GridCell3 cell {0, 0, 0};
    std::uint64_t seed = 0;
    // Background medium of the cell (an interstellar_medium_background object
    // carrying the lattice) followed by the compact bodies.
    std::vector<InterstellarObject> objects;
};

InterstellarCell generateInterstellarCell(
    std::uint64_t universeSeed,
    const world::coordinates::GridCell3& cell,
    const std::vector<glm::dvec3>& catalogSystemPositionsLy,
    const InterstellarSettings& settings = {},
    const UniverseGenerationVersions& versions = CurrentGenerationVersions);

// Distinct medium structures: the authored local clouds plus the procedural
// structures seeded by the given generation cells.
std::vector<InterstellarObject> generateInterstellarMedium(
    std::uint64_t universeSeed,
    const std::vector<world::coordinates::GridCell3>& cells,
    const InterstellarSettings& settings = {},
    const UniverseGenerationVersions& versions = CurrentGenerationVersions);

std::string interstellarCellTag(const world::coordinates::GridCell3& cell);

// Galactic (l, b in degrees, distance in ly) <-> ICRS Cartesian light-years.
glm::dvec3 galacticToIcrsLy(double lDeg, double bDeg, double distanceLy);
void icrsToGalactic(const glm::dvec3& icrsLy, double& lDeg, double& bDeg, double& distanceLy);

} // namespace world::generation
