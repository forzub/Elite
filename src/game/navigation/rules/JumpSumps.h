#pragma once

// Jump sumps at asteroid and debris concentrations (author 2026-11-13, JUMP_MECHANICS.md section 4).
// Currents and sumps are one picture: gravity acts on the jump; a sump is where the pull on the jump is stronger, and a
// step crossing it may fall into it. Plausible cause (unproven, not absurd): a swarm of bodies makes a "rough"
// potential of countless small wells; the jump field snags on them like a wave over shallows full of stones - a single
// smooth mass (a star) does not hold a step this way, it is visible and gone around (InterstellarJumpRoute.h).
//
// Depth = the column of swarm mass the step crosses, sigma (kg/m^2). Capture is a random process along the line: the
// step is caught at the first point where the crossed column, in units of SumpColumnKgM2 x sqrt(beacon lock), exceeds an
// exponential draw - P(catch) = 1 - exp(-sigma / (SumpColumnKgM2 sqrt(lock))). Calibrated on Sol's main belt: a radial
// crossing in its plane without beacons ~ 1 in 4; with Sol's beacons (lock 200) ~ 2 %. A caught ship comes out at the
// catch point with the swarm's local motion (a circular orbit around the star there); the step's charge is spent.
//
// Geometry from the generated reservoirs (elite.generated_reservoir.v1): asteroid_belt / kuiper_belt / debris_disk =
// an annulus around the parent between inner_au and outer_au, radial density peaking at peak_au, gaussian height
// h = peak x sin(inclination_mean); trojans = two clouds at the parent planet's L4 / L5 (+-60 deg), 17 deg along the
// orbit. comet_cloud (thousands of AU) matters for the interstellar steps only - not here yet.
// The belt plane is the system frame's x-z plane (the flight renderer's reservoir convention).
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial
{
struct CelestialSystemDefinition;
}

namespace game::navigation::jump
{
constexpr double SumpColumnKgM2 = 0.028;

struct JumpSump
{
    std::string id;
    std::string name;
    std::string kind;          // asteroid_belt | kuiper_belt | debris_disk | trojans
    std::string centreBodyId;  // the star / barycentre the swarm orbits
    std::string planetId;      // trojans: the planet whose L4 / L5 hold them
    double innerM = 0.0, outerM = 0.0, peakM = 0.0, halfThicknessM = 0.0;
    double massKg = 0.0;
    double densityScale = 0.0; // kg/m^3 at the profile's peak (from the mass)
};

// the sumps of a system (cached per system id)
const std::vector<JumpSump>& jumpSumpsOfSystem(const world::celestial::CelestialSystemDefinition& system);

// "the system has a beacon" for the lock (version 1: any world site in it - no beacon runtime yet)
bool systemHasBeacons(int systemId);

// the swarm mass density at a point (kg/m^3); positions of the bodies by id (system frame, metres)
double sumpDensityKgM3(const JumpSump& sump, const glm::dvec3& p, const std::unordered_map<std::string, glm::dvec3>& centres);

struct SumpCrossing
{
    std::string sumpId;
    std::string name;
    double columnKgM2 = 0.0;
    double probability = 0.0;
};

// the column each sump puts on the straight step from -> to, and the catch probability with the beacon lock
std::vector<SumpCrossing> sumpCrossings(const std::vector<JumpSump>& sumps, const std::unordered_map<std::string, glm::dvec3>& centres,
                                        const glm::dvec3& from, const glm::dvec3& to, double lockFactor);

// the catch along the step for an exponential draw `e` (= -ln U): true + the point and the sump when caught
bool sumpCatch(const std::vector<JumpSump>& sumps, const std::unordered_map<std::string, glm::dvec3>& centres,
               const glm::dvec3& from, const glm::dvec3& to, double lockFactor, double e, glm::dvec3& point, const JumpSump*& sump);
} // namespace game::navigation::jump
