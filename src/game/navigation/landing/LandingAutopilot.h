#pragma once

#include <functional>
#include <string>

#include <glm/glm.hpp>

// Landing autopilot + site planner (reusable: J free flight today, any powered
// lander later). Pure guidance - no simulation, no rendering: the caller
// supplies the body kinematics, the ship state and a terrain probe, and applies
// the returned velocity change and attitude.
//
// Modes:
//  - Pad:  land on a designated spot (an aerodrome / port pad) given as a
//          body-fixed direction; the planner only checks it.
//  - Wild: find a flat spot of the hull's size near a body-fixed point: rings
//          of candidates outward, each judged by the height spread under the
//          hull footprint (slope, roughness) and the ground (no water, lava).
//
// Body-fixed means the (prime, north, east) basis of the body (the renderer's
// and the server terrain's orientation): the site turns with the body, so a
// slowly rotating Moon does not carry the pad away during the descent.
//
// The site is chosen AHEAD (planAhead during the approach) and the approach aims
// at the point above it; the lander then lands purposefully. If no acceptable
// site is found the lander does not land: it holds a circular orbit and keeps
// searching under its ground track (wider each pass).
//
// Phases: Planning -> Orbit (no site yet: circular orbit, re-plan every 3 s) ->
// Transit (to the point above the site at a height that grows with speed, J
// drive) -> Descent (vertical speed min(400, sqrt(2 15 h), h / 4), at least
// 1.5 m/s; the last 50 m only straight down over the spot) -> Down (the
// caller's surface contact ends it).
namespace game::navigation::landing
{
enum class LandingMode
{
    Wild,
    Pad
};

enum class LandingPhase
{
    Planning,
    Orbit,
    Transit,
    Descent,
    Down,
    Failed
};

struct LandingRequest
{
    LandingMode mode = LandingMode::Wild;
    glm::dvec3 padBodyDirection {0.0};   // Pad: body-fixed unit direction of the pad
    double searchRadiusM = 6000.0;       // Wild: how far from the start point to look
    double maxSlopeDeg = 6.0;            // acceptable footprint slope
};

// One terrain sample at a body-fixed direction: height above the body radius,
// slope, material (world::celestial::surface::SurfaceMaterial; 100 water, 9 lava).
struct TerrainPoint
{
    double heightM = 0.0;
    double slopeDeg = 0.0;
    int material = -1;
};
using TerrainProbe = std::function<bool(const glm::dvec3& bodyDirection, TerrainPoint& out)>;

struct LandingSite
{
    bool valid = false;
    glm::dvec3 bodyDirection {0.0};
    double heightM = 0.0;       // highest ground under the footprint
    double slopeDeg = 0.0;      // footprint slope
    double roughnessM = 0.0;    // height spread under the footprint
    double offsetM = 0.0;       // distance from where the search started
    int material = -1;
};

// The planner. Returns an invalid site while the terrain is still unknown, and
// (Wild) when nothing within maxSlopeDeg exists in the search radius -
// searched == true then tells the two apart.
LandingSite planWildSite(const glm::dvec3& startBodyDirection, double bodyRadiusM, const glm::dvec3& hullHalfExtentsM,
                         const LandingRequest& request, const TerrainProbe& probe, bool* searched = nullptr);
LandingSite checkPadSite(const glm::dvec3& padBodyDirection, double bodyRadiusM, const glm::dvec3& hullHalfExtentsM,
                         const TerrainProbe& probe);

struct LandingInput
{
    glm::dvec3 bodyCentreM {0.0};
    glm::dvec3 bodyVelocityMps {0.0};
    glm::dmat3 worldFromBody {1.0};      // columns: prime, north, east (world)
    double bodyRadiusM = 0.0;
    double bodyMuM3s2 = 0.0;             // for the orbit hold
    glm::dvec3 shipPositionM {0.0};      // world
    glm::dvec3 shipVelocityMps {0.0};    // world
    glm::dvec3 shipForward {0.0, 0.0, -1.0};
    glm::dvec3 hullHalfExtentsM {8.0, 3.0, 15.0};
    double groundUnderShipM = 0.0;       // hull bottom height above the radius under the ship (caller's contact ground)
    double dt = 0.0;
    // the lander's real authority (main engines, m/s2) and the hull's half height along the
    // local vertical (tail down: half the length); <= 0 = the legacy 40 m/s2 / half.y
    double authorityMps2 = 0.0;
    double touchdownHalfHeightM = 0.0;
};

struct LandingCommand
{
    glm::dvec3 deltaVelocityMps {0.0};   // apply to the ship's world velocity this tick
    glm::dvec3 velocityErrorMps {0.0};   // the unclamped wanted - current velocity (for a thrust-axis lander)
    glm::dvec3 noseWorld {0.0};          // wanted attitude (level: nose to the horizon)
    glm::dvec3 upWorld {0.0};
    double accelerationMps2 = 0.0;
    double heightAboveGroundM = 0.0;
    double distanceToSiteM = 0.0;        // horizontal
};

class LandingAutopilot
{
public:
    void start(const LandingRequest& request, const glm::dvec3& startBodyDirection);
    // Choose the site before arrival (call during the approach; cheap once found,
    // throttled by the caller). true = a site is chosen.
    bool planAhead(const glm::dvec3& startBodyDirection, double bodyRadiusM, const glm::dvec3& hullHalfExtentsM,
                   const TerrainProbe& probe);
    LandingCommand step(const LandingInput& input, const TerrainProbe& probe);

    LandingPhase phase() const { return m_phase; }
    const LandingSite& site() const { return m_site; }
    const LandingRequest& request() const { return m_request; }
    const std::string& status() const { return m_status; }

private:
    LandingRequest m_request;
    glm::dvec3 m_start {0.0};
    LandingSite m_site;
    LandingPhase m_phase = LandingPhase::Planning;
    std::string m_status;
    glm::dvec3 m_heading {0.0};
    double m_replanSeconds = 0.0;
    int m_searchPasses = 0;
    bool m_haveLastSiteTop = false;
    glm::dvec3 m_lastSiteTop {0.0};
};
} // namespace game::navigation::landing
