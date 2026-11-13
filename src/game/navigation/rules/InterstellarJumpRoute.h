#pragma once

// The interstellar jump's way around gravity objects (author 2026-11-13: "such a scheme fits"; sizes from known data,
// plausible with allowed exaggerations). JUMP_MECHANICS.md section 2: one charge is one jump, a series of instant steps
// with micro-exits between them; an intermediate point is a course correction. A step is caught by any mass whose
// gravity there is stronger than the Galaxy's own pull - the body's sphere of influence against the galactic field:
//     R = sqrt(mu / g_gal),  g_gal = v_sun^2 / R_sun = (230 km/s)^2 / 8.2 kpc ~ 2.1e-10 m/s^2
// (Sun ~0.084 ly = 5 300 AU, a brown dwarf of 0.06 Msun ~0.02 ly, a Jupiter-mass rogue ~170 AU). The planner walks
// around every charted mass on the line: other catalogue stars (all their stars' masses), brown dwarfs, rogue planets
// and moons, binary rogues, stellar remnants (gameplay_gravity). The start and target systems are not obstacles.
// Shared by the server (authority) and the client (the calculation shown before START).
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial
{
class StarAtlasDatabase;
}

namespace game::navigation::jump
{
constexpr double GalacticFieldMps2 = 2.1e-10;
constexpr double InterstellarDetourFactor = 2.0;   // the correction point: this many capture radii from the mass

struct InterstellarRoute
{
    bool ok = false;                       // false: no way around found within the passes
    std::vector<glm::dvec3> pointsLy;      // from, the correction points, to (galactic ly)
    std::vector<std::string> avoidedNames; // what each correction goes around
    double lengthLy = 0.0;
};

double gravityCaptureRadiusLy(double muM3s2);

// startLy: where the way starts (a ship in a deep-space place or a system's approaches is not at the place's centre);
// nullptr = the start system's position
InterstellarRoute planInterstellarRoute(const world::celestial::StarAtlasDatabase& atlas, int fromSystemId, int toSystemId,
                                        const glm::dvec3* startLy = nullptr);
// the start of an interstellar way: a catalogue system's star, or the ship's own point in an open-space place
glm::dvec3 jumpStartPointLy(const world::celestial::StarAtlasDatabase& atlas, int systemId, const glm::dvec3& shipPositionM);

// ---- sumps between the systems (author 2026-11-13): the interstellar swarms - icy debris fields (ellipsoids, soft edge
// 1 - r^2) and debris groups (spheres of extent_au). Scale-invariant calibration: a light-years-long step sees a swarm
// as a few-AU step sees a belt - crossing the core of a swarm without beacons catches ~1 in 4, as Sol's main belt does
// (JumpSumps.h). Depth = integral of the normalised density (0..1) along the way / (InterstellarSumpScaleLy x
// sqrt(lock)); P = 1 - exp(-depth). A caught ship comes out in the swarm's deep-space place
// (StarAtlasDatabase::deepSpaceIdForObject) at the catch point, at rest in its frame.
constexpr double InterstellarSumpScaleLy = 2.0;

// ---- approach shells (JUMP_MECHANICS 10g/10h, data galaxy/jump_approach_shells.json): an authored spherical layer of
// sumps around a hard-to-reach system - a dense, stirred comet cloud - with a few narrow safe corridors (radial cones)
// that drift with time. Depth along the way = tau x the integral of the profile (triangle inner..peak..outer) x the
// corridor factor (1 - exp(-(angle/halfWidth)^2) per corridor) / the half thickness; one dumb radial crossing without
// beacons is tau. Caught: out in the system's approaches (StarAtlasDatabase::approachIdOf).
struct ApproachCorridor
{
    std::string name;
    glm::dvec3 dir {1.0, 0.0, 0.0};   // galactic axes, at the file's epoch
    glm::dvec3 axis {0.0, 0.0, 1.0};  // the drift turns dir about this axis
    double halfWidthRad = 0.02;
    double driftRadPerS = 0.0;
};

struct ApproachShell
{
    int systemId = -1;
    std::string name;
    std::string cause;
    glm::dvec3 centreLy {0.0};
    double innerLy = 0.0, peakLy = 0.0, outerLy = 0.0;
    double tau = 0.0;
    double epochUniverseS = 0.0;
    std::vector<ApproachCorridor> corridors;
};

const std::vector<ApproachShell>& approachShells(const world::celestial::StarAtlasDatabase& atlas);
const ApproachShell* approachShellOf(const world::celestial::StarAtlasDatabase& atlas, int systemId);
glm::dvec3 corridorDirAt(const ApproachShell& shell, const ApproachCorridor& c, double universeTimeS);
// the shell's normalised density factor (0..1, before tau) at a galactic point
double approachShellFactor(const ApproachShell& shell, const glm::dvec3& pointLy, double universeTimeS);

struct InterstellarSumpCrossing
{
    std::string name;
    int placeId = -1;
    bool shell = false;        // an approach shell (else a swarm)
    double depth = 0.0;        // before the beacons
    double probability = 0.0;  // with the beacons' lock
};

struct InterstellarSumpCatch
{
    bool caught = false;
    bool shell = false;
    int placeId = -1;
    std::string name;
    glm::dvec3 pointLy {0.0};
    glm::dvec3 centreLy {0.0};
};

std::vector<InterstellarSumpCrossing> interstellarSumpCrossings(const world::celestial::StarAtlasDatabase& atlas,
                                                                const InterstellarRoute& route, double lockFactor,
                                                                double universeTimeS = 0.0);
// the catch along the way for an exponential draw `e` (= -ln U)
InterstellarSumpCatch interstellarSumpCatch(const world::celestial::StarAtlasDatabase& atlas, const InterstellarRoute& route,
                                            double lockFactor, double e, double universeTimeS = 0.0);
} // namespace game::navigation::jump
