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
#include <algorithm>
#include <cmath>
#include <limits>

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

} // namespace game::navigation::rules::sensor
