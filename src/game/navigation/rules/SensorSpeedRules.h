#pragma once

// Sensor-limited speed (docs/SENSOR_LIMITED_SPEED.md, author 2026-11-19, the idea from Objects in Space): in uncharted
// space the autopilot keeps the speed at which it still sees an obstacle, decides and stops inside the space its
// sensors have surely swept:
//     v t_r + v^2 / (2 a_b) <= k R   ->   v_max = a_b (-t_r + sqrt(t_r^2 + 2 k R / a_b))
// R - the radar's detection range (no radar: optics only), k - the margin (a small target is not seen at the full
// range), t_r - the reaction (the radar's sweep + the computer + the decision; worse with a failed nav computer),
// a_b - the braking the current mode has (the J field's level now). Charted space (a green corridor, a hub's radar
// zone, a catalogue system's surveyed shell, the approaches of a system whose lane record the ship carries) lifts it.
// Manual flight is not limited (the pilot's risk). Pure rules: the client's route pilot and J autopilot use them.
// Author 2026-11-20 (variant 1): interplanetary space is almost empty - in UNCHARTED space the hard bound holds only
// in / near a known or suspected hazard (a belt / debris disk / trojan swarm, a planet's rings, a deep-space debris
// field, the space near a body); empty uncharted space has no bound (the pilot's / autopilot's own risk). "Near" =
// within the ship's stopping run at its current speed, and on the way in the speed follows the braking envelope
// sqrt(v_max^2 + 2 a d), so the ship reaches the hazard's edge already at v_max.
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::rules::sensor
{
constexpr double ConfidenceK = 0.8;
constexpr double ReactionS = 1.5;
// a failed / missing nav computer (DynamicMotionState::navComputerState == 2): the decision is the pilot's by hand
constexpr double ReactionNavComputerDownS = 3.0;
// no radar, or a radar shorter than this (a docking radar): optics / the passive sensors
constexpr double OpticsRangeM = 150000.0;
// a catalogue system's surveyed shell: this many times its farthest body's distance from the star
constexpr double SurveyShellFactor = 1.5;

// v_max (m/s) for the range R (m), the reaction t_r (s) and the braking a_b (m/s2); the rationalised form (no loss of
// precision when 2 k R / a_b is small against t_r^2). a_b <= 0 (no braking at all): 0
inline double sensorSpeedLimitMps(double rangeM, double reactionS, double brakeMps2, double k = ConfidenceK)
{
    const double d = k * std::max(0.0, rangeM);
    if (d <= 0.0 || brakeMps2 <= 0.0)
        return 0.0;
    const double t = std::max(0.0, reactionS);
    return 2.0 * d / (t + std::sqrt(t * t + 2.0 * d / brakeMps2));
}

inline double sensorRangeM(double radarRangeM)
{
    return std::max(radarRangeM, OpticsRangeM);   // a short docking radar (the legacy 1 km FRN-U) never sees less than the optics
}

inline double reactionSeconds(bool navComputerFailed)
{
    return navComputerFailed ? ReactionNavComputerDownS : ReactionS;
}

// what makes a point charted (the first that holds); None - uncharted: the limit applies
enum class ChartedBy : int
{
    None = 0,
    Corridor = 1,      // inside a green corridor (route_zones.json)
    HubZone = 2,       // inside a hub's radar / beacon zone (route_zones.json watch.hub_radar_km)
    SurveyShell = 3,   // inside an INHABITED catalogue system's surveyed shell (SurveyShellFactor x its farthest body);
                       // author 2026-11-20: only inhabited systems are well surveyed, the rest is known in fragments
    LaneRecord = 4     // a system's approaches with a usable lane record of its shell (readable: the nav computer works)
};

struct ChartedQuery
{
    bool catalogueSystem = false;   // a local catalogue system (not a deep-space place, not approaches, not a distant one)
    bool inhabited = false;         // it has world places / hubs (beacons): surveyed; an empty system is not
    double distanceFromStarM = 0.0;
    double farthestBodyM = 0.0;     // the farthest body's distance from the star now
    double nearestHubM = std::numeric_limits<double>::infinity();
    double hubZoneM = 0.0;
    bool inGreenCorridor = false;
    bool approachLaneRecord = false;
};

inline ChartedBy chartedBy(const ChartedQuery& q)
{
    if (q.inGreenCorridor)
        return ChartedBy::Corridor;
    if (q.nearestHubM <= q.hubZoneM)
        return ChartedBy::HubZone;
    if (q.catalogueSystem && q.inhabited && q.distanceFromStarM <= SurveyShellFactor * q.farthestBodyM)
        return ChartedBy::SurveyShell;
    if (q.approachLaneRecord)
        return ChartedBy::LaneRecord;
    return ChartedBy::None;
}

// ---- hazards (variant 1, author 2026-11-20) --------------------------------------------------------------------------
// near a body: this many radii. 10 R covers the low orbits up to and past geostationary (6.6 R at the Earth), where
// natural / artificial debris and the traffic concentrate, a giant's inner moons and rings (Io 5.9 R, Saturn's E
// ring ~8 R) - and stops well short of the Hill sphere (Jupiter: ~740 R), which would cap the whole system.
constexpr double NearBodyRadii = 10.0;
// a belt's height: its gaussian scale height (JumpSumps: peak x sin(inclination)) x this; 2 -> 2 % of the mid-plane density
constexpr double BeltHeightSigmas = 2.0;
// a trojan swarm's arc either side of L4 / L5: 2 x its 17 deg gaussian half-width (JumpSumps)
constexpr double TrojanHalfArcRad = 2.0 * 17.0 * 3.14159265358979323846 / 180.0;
// a planetary ring's half-thickness: real main rings are 10 m - 1 km thick
constexpr double RingHalfThicknessM = 1000.0;
// the braking the autopilots plan with (RouteLegPilot / BodyAutopilot: 0.4 of the full field) - the margin and the
// braking envelope use it, the same number on both sides (no on / off chatter at the margin's edge)
constexpr double HazardBrakeFraction = 0.4;

enum class HazardKind : int
{
    None = 0,
    Belt = 1,          // an asteroid / Kuiper belt, a debris disk, a trojan swarm (generated reservoirs, JumpSumps)
    Ring = 2,          // a planet's rings (the body's ring bands)
    Body = 3,          // near a body (NearBodyRadii, or its atmosphere if higher)
    DebrisField = 4    // a deep-space place's swarm (icy_debris_field / debris_group)
};

struct HazardZone
{
    HazardKind kind = HazardKind::None;
    std::string name;
    glm::dvec3 centreM {0.0};
    glm::dvec3 normal {0.0};        // an annulus' plane normal (unit); zero - a sphere of outerM around the centre
    double innerM = 0.0, outerM = 0.0;
    double halfThicknessM = 0.0;    // an annulus' half-height above / below its plane
    glm::dvec3 arcCentreDir {0.0};  // a swarm on an arc (trojans): the arc's centre direction in the plane; zero - all round
    double arcHalfRad = 0.0;
};

// distance from p to the zone (0 inside): an annulus - the radial, vertical and along-arc excesses in quadrature
inline double hazardDistanceM(const HazardZone& z, const glm::dvec3& p)
{
    const glm::dvec3 local = p - z.centreM;
    if (glm::dot(z.normal, z.normal) <= 0.0)
        return std::max(0.0, glm::length(local) - z.outerM);
    const double h = glm::dot(local, z.normal);
    const glm::dvec3 radial = local - z.normal * h;
    const double r = glm::length(radial);
    const double dr = std::max({0.0, z.innerM - r, r - z.outerM});
    const double dh = std::max(0.0, std::abs(h) - z.halfThicknessM);
    double da = 0.0;
    if (z.arcHalfRad > 0.0 && r > 0.0)
    {
        const double phi = std::acos(std::clamp(glm::dot(radial / r, z.arcCentreDir), -1.0, 1.0));
        da = std::min(r, z.outerM) * std::max(0.0, phi - z.arcHalfRad);
    }
    return std::sqrt(dr * dr + dh * dh + da * da);
}

// the nearest zone (its index, the distance); none - -1, infinity
inline int nearestHazard(const std::vector<HazardZone>& zones, const glm::dvec3& p, double& distanceM)
{
    int best = -1;
    distanceM = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < zones.size(); ++i)
        if (const double d = hazardDistanceM(zones[i], p); d < distanceM)
        {
            distanceM = d;
            best = static_cast<int>(i);
        }
    return best;
}

// the run to a stop from v: the delay (reaction + the field's ramp to full) at v, then braking at a_b; a_b <= 0 - never
inline double stoppingDistanceM(double speedMps, double delayS, double brakeMps2)
{
    const double v = std::max(0.0, speedMps);
    if (brakeMps2 <= 0.0)
        return v > 0.0 ? std::numeric_limits<double>::infinity() : 0.0;
    return v * std::max(0.0, delayS) + v * v / (2.0 * brakeMps2);
}

// in or near a hazard: inside it, or it is closer than the stopping run at the current speed
inline bool inHazard(double hazardDistanceM, double speedMps, double delayS, double brakeMps2)
{
    return hazardDistanceM <= 0.0 || hazardDistanceM <= stoppingDistanceM(speedMps, delayS, brakeMps2);
}

// the speed bound on the way into a hazard d away: braking at a_b down to v_max at its edge (v_max inside)
inline double hazardApproachSpeedMps(double sensorLimitMps, double hazardDistanceM, double brakeMps2)
{
    return std::sqrt(sensorLimitMps * sensorLimitMps + 2.0 * std::max(0.0, brakeMps2) * std::max(0.0, hazardDistanceM));
}

} // namespace game::navigation::rules::sensor
