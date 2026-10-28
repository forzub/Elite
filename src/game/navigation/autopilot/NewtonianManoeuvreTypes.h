#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/NewtonianFlightPolicy.h"

namespace game::navigation::autopilot
{


// Newtonian manoeuvre program.
//
// Compiled from the accepted route geometry and its speed profile (both owned
// by navigation) and the craft's real capability. The program says, along the
// route, WHAT the craft does: where the main engine works and in which
// direction, where it coasts on RCS, and where the hull turns so that the
// thrust axis is already there when a burn begins. The executor flies the
// current phase; it corrects the execution but never re-chooses the strategy
// per tick.
//
// Required acceleration along the route: a = a_t * T + v^2 * kappa * N. Where
// RCS alone carries it the craft coasts; elsewhere the main engine burns
// along a/|a| (braking and curving combined into ONE vector). The planned
// hull direction is that thrust direction, rate-limited in time with
// anticipation (a backward pass: turn early enough; a forward pass: never
// faster than the calm preferred rate).

enum class ManoeuvrePhaseKind : int
{
    Coast = 0,   // main off, RCS keeps the trajectory
    Orient = 1,  // main off, hull turns calmly onto the next burn axis
    Flip = 2,    // Orient by more than 90 degrees
    Burn = 3,    // main along the motion (accelerate)
    Brake = 4,   // main against the motion
    Curve = 5,   // main with a substantial path-normal component
    Settle = 6,  // final low-speed precision on RCS
};

[[nodiscard]] inline const char* manoeuvrePhaseName(
    ManoeuvrePhaseKind kind
) noexcept
{
    switch (kind)
    {
        case ManoeuvrePhaseKind::Coast: return "COAST";
        case ManoeuvrePhaseKind::Orient: return "ORIENT";
        case ManoeuvrePhaseKind::Flip: return "FLIP";
        case ManoeuvrePhaseKind::Burn: return "BURN";
        case ManoeuvrePhaseKind::Brake: return "BRAKE";
        case ManoeuvrePhaseKind::Curve: return "CURVE";
        case ManoeuvrePhaseKind::Settle: return "SETTLE";
    }
    return "?";
}

[[nodiscard]] inline bool manoeuvrePhaseUsesMain(
    ManoeuvrePhaseKind kind
) noexcept
{
    return kind == ManoeuvrePhaseKind::Burn ||
        kind == ManoeuvrePhaseKind::Brake ||
        kind == ManoeuvrePhaseKind::Curve;
}

// One route station handed over by navigation.
struct ManoeuvreRouteSample
{
    double progressMeters = 0.0;
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 tangentMap {0.0, 0.0, -1.0};
    // Unit turn normal (toward the centre of curvature); 0 on straights.
    glm::dvec3 normalMap {0.0};
    double curvaturePerMeter = 0.0;
    double speedMps = 0.0;
    double longitudinalAccelerationMps2 = 0.0;
    // Narrow zone (Step 3): the hull must cross this station with its nose
    // along +/-narrowAxisMap and its up along +/-narrowUpMap, without
    // turning, within narrowToleranceMeters laterally.
    bool narrow = false;
    glm::dvec3 narrowAxisMap {0.0, 0.0, -1.0};
    glm::dvec3 narrowUpMap {0.0, 1.0, 0.0};
    double narrowToleranceMeters = 0.0;
};

struct ManoeuvreCapability
{
    double mainAccelerationMps2 = 0.0;
    // Share of RCS the trajectory may use (the rest stays for attitude).
    double rcsTrajectoryAccelerationMps2 = 0.0;
    double gimbalLimitRad = 0.0;
    // Calm planned hull rate.
    double plannedTurnRateRadPerSec = 0.0;
    // Authority the speed profile is planned with (turn windows reshape
    // the profile within it).
    double plannedForwardAccelerationMps2 = 0.0;
    double plannedBrakingAccelerationMps2 = 0.0;
};

struct ManoeuvreStation
{
    double progressMeters = 0.0;
    double timeSeconds = 0.0;
    double speedMps = 0.0;
    double longitudinalAccelerationMps2 = 0.0;
    double curvaturePerMeter = 0.0;
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 tangentMap {0.0, 0.0, -1.0};
    glm::dvec3 normalMap {0.0};
    glm::dvec3 requiredAccelerationMap {0.0};
    bool mainBurn = false;
    // Planned thrust axis = planned hull forward (rate limited).
    glm::dvec3 hullForwardMap {0.0, 0.0, -1.0};
    // Narrow zone: fixed attitude (signs chosen for the arriving hull).
    bool narrow = false;
    glm::dvec3 narrowForwardMap {0.0, 0.0, -1.0};
    glm::dvec3 narrowUpMap {0.0, 1.0, 0.0};
    double narrowToleranceMeters = 0.0;
    std::size_t phaseIndex = 0;
};

// A speed limit the motion profile decided for a stretch of the route (e.g.
// an S of two curves flown without a hull swing). It holds for the rest of
// the route: later recompiles start from it instead of re-deciding from a
// profile that no longer has room for it.
struct ManoeuvreSpeedCap
{
    double fromProgressMeters = 0.0;
    double toProgressMeters = 0.0;
    double speedMps = 0.0;
};

struct ManoeuvrePhase
{
    ManoeuvrePhaseKind kind = ManoeuvrePhaseKind::Coast;
    std::size_t firstStation = 0;
    std::size_t lastStation = 0;
    double startProgressMeters = 0.0;
    double endProgressMeters = 0.0;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
};

struct NewtonianManoeuvreProgram
{
    bool valid = false;
    std::vector<ManoeuvreStation> stations;
    std::vector<ManoeuvrePhase> phases;
    // Largest planned angle between the planned hull and the required burn
    // axis beyond the nozzle cone (0 = every burn is reachable in time).
    double worstThrustDeficitRad = 0.0;
    double worstThrustDeficitProgressMeters = 0.0;

    // Station interval containing `progress` (index of its lower station).
    [[nodiscard]] std::size_t stationAt(double progress) const noexcept
    {
        if (stations.size() < 2)
            return 0;
        const auto it = std::upper_bound(
            stations.begin(), stations.end(), progress,
            [](double value, const ManoeuvreStation& station)
            {
                return value < station.progressMeters;
            });
        const std::size_t upper =
            static_cast<std::size_t>(it - stations.begin());
        return upper == 0 ? 0 : std::min(upper - 1, stations.size() - 2);
    }
};

namespace manoeuvre_detail
{
[[nodiscard]] inline glm::dvec3 unitOr(
    const glm::dvec3& v, const glm::dvec3& fallback) noexcept
{
    const double l = glm::length(v);
    return std::isfinite(l) && l > 1.0e-12 ? v / l : fallback;
}

[[nodiscard]] inline double angleBetween(
    const glm::dvec3& a, const glm::dvec3& b) noexcept
{
    return std::acos(std::clamp(glm::dot(a, b), -1.0, 1.0));
}

// Rotate unit `from` toward unit `to` by at most `maxAngle` along the great
// circle. Antiparallel vectors turn about `axisHint` (or any perpendicular).
[[nodiscard]] inline glm::dvec3 stepToward(
    const glm::dvec3& from,
    const glm::dvec3& to,
    double maxAngle,
    const glm::dvec3& axisHint) noexcept
{
    const double angle = angleBetween(from, to);
    if (angle <= maxAngle || angle <= 1.0e-9)
        return to;
    glm::dvec3 axis = glm::cross(from, to);
    if (glm::length(axis) < 1.0e-6)
    {
        axis = glm::cross(from, axisHint);
        if (glm::length(axis) < 1.0e-6)
            axis = glm::cross(from, glm::dvec3(0.0, 1.0, 0.0));
        if (glm::length(axis) < 1.0e-6)
            axis = glm::cross(from, glm::dvec3(1.0, 0.0, 0.0));
    }
    axis = glm::normalize(axis);
    const double c = std::cos(maxAngle);
    const double s = std::sin(maxAngle);
    // Rodrigues rotation of `from` about `axis` (axis is perpendicular).
    return unitOr(from * c + glm::cross(axis, from) * s, to);
}
} // namespace manoeuvre_detail

namespace manoeuvre_detail
{
// A station whose required acceleration is (almost) along the path: a
// longitudinal burn, not a curve.
[[nodiscard]] inline bool isLongitudinalStation(
    const ManoeuvreStation& station) noexcept
{
    if (station.narrow)
        return false;
    const glm::dvec3 a = station.requiredAccelerationMap;
    const double along = glm::dot(a, station.tangentMap);
    return glm::length(a - station.tangentMap * along) <=
        NewtonianFlightPolicy::CurveNormalShare * glm::length(a);
}
} // namespace manoeuvre_detail

} // namespace game::navigation::autopilot
