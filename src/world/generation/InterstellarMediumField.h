#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::generation
{

/*
    Physical substrate of the interstellar layer: a causal snapshot model of
    the present-day medium, a pure function of the universe seed and an ICRS
    position (light-years). No hydrodynamics - the state is what a few Myr of
    supernova-driven cavities would leave:

    - CAVITIES (hot, tenuous, X-ray gas) blown by supernovae / OB winds,
      each bounded by a swept-up, compressed, supernova-enriched WALL.
      Observed anchors: the Local Bubble (radius per galactic direction after
      Lallement et al. 2014 / Pelgrims et al. 2020) and the two Loop I shells
      S1 / S2 (Wolleben 2007; the Sun lies near the S1 rim). Beyond 300 ly
      (the volume the observed anchors describe) neighbouring superbubbles are
      procedural, per 500-ly superbubble cell, position dependent and
      reproducible.
    - A wall that lies inside ANOTHER cavity is immersed in hot gas / UV: a
      thin warm, partly ionised sheet - the causal origin of the warm
      cloudlets around the Sun (the LIC / G cloud complex sits where the Local
      Bubble meets the Loop I S1 shell).
    - Where walls of different cavities overlap (shell intersection) the gas
      is compressed twice, shielded and cools: the richest cold / molecular
      material.
    - Elsewhere: warm neutral medium.

    The field gives the local physical state; CloudFormationPotential turns it
    into the probability of distinct structures. A star position by itself is
    never a cause of a cloud.
*/

struct MediumSample
{
    char phase = 'h';                 // world::interstellar::mediumPhaseName codes
    double densityCm3 = 0.0;
    double temperatureK = 0.0;
    double ionizationFraction = 0.0;
    double molecularFraction = 0.0;
    double dustToGas = 0.0;
    double iceMantleFraction = 0.0;
    // 0..1: share of recent supernova ejecta (60Fe-bearing dust of a wall).
    double supernovaEnrichment = 0.0;

    double pressureKcm3 = 0.0;        // n T
    double turbulenceKms = 0.0;       // non-thermal velocity dispersion
    double shieldingAv = 0.0;         // visual extinction through the local structure (mag)
    double densityGradientPerLy = 0.0;// |d ln n / d r|
    int cavityCount = 0;              // hot cavities containing the point
    int wallCount = 0;                // overlapping walls not immersed in a cavity
    bool immersedWall = false;        // a wall inside another cavity
    int immersedWallCount = 0;        // overlapping immersed walls (interaction zone)
};

// Probability-shaping potential (0..1 each) of distinct structures at a
// point, derived only from the physical state: cooling efficiency (density,
// temperature, metals), pressure / compression, turbulence, shielding, density
// gradient, shell overlap and immersion.
struct CloudFormationPotential
{
    double warm = 0.0;        // warm partly ionised cloudlets
    double cold = 0.0;        // cold neutral clouds
    double molecular = 0.0;   // molecular / dust-rich clumps and filaments
};

struct CavityShell
{
    std::string id;
    std::string name;
    glm::dvec3 centreLy {0.0};
    double radiusLy = 0.0;        // mean cavity radius
    double wallLy = 0.0;          // mean wall thickness
    double ageMyr = 0.0;
    int supernovae = 0;
    double wallCompression = 0.0; // peak density gain of the wall
    bool localBubble = false;     // radius per galactic direction (observed table)
    std::string provenance;       // observed | procedural_generated
    std::string source;
    std::uint64_t seed = 0;
};

// The cavities that can influence `positionLy` (observed anchors + the
// procedural superbubbles of the surrounding superbubble cells).
std::vector<CavityShell> cavitiesNear(std::uint64_t universeSeed, const glm::dvec3& positionLy);

// Every cavity whose influence reaches a box (for reports / materialisation).
std::vector<CavityShell> cavitiesInBox(std::uint64_t universeSeed, const glm::dvec3& minLy, const glm::dvec3& maxLy);

// The continuous interstellar medium at an ICRS position (ly).
MediumSample backgroundMediumAt(std::uint64_t universeSeed, const glm::dvec3& positionLy);

CloudFormationPotential cloudFormationPotential(const MediumSample& m);

// Galactic (l, b in degrees, distance in ly) <-> ICRS Cartesian light-years.
glm::dvec3 galacticToIcrsLy(double lDeg, double bDeg, double distanceLy);
void icrsToGalactic(const glm::dvec3& icrsLy, double& lDeg, double& bDeg, double& distanceLy);

} // namespace world::generation
