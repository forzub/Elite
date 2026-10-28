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

// Compile the program. `samples` must be ordered by progress. The initial
// hull forward is where the program's first planned hull starts.
[[nodiscard]] inline NewtonianManoeuvreProgram compileNewtonianManoeuvreProgram(
    const std::vector<ManoeuvreRouteSample>& samples,
    const ManoeuvreCapability& capability,
    const glm::dvec3& initialHullForwardMap,
    double startProgressMeters = 0.0
)
{
    using namespace manoeuvre_detail;
    NewtonianManoeuvreProgram program;
    if (samples.size() < 2 || !(capability.mainAccelerationMps2 > 1.0e-9))
        return program;

    // Doctrine values: NewtonianFlightPolicy (shared with the pilot).
    using Policy = NewtonianFlightPolicy;
    constexpr double MainShareOfRcs = Policy::MainShareOfRcs;
    constexpr double MinCoastSeconds = Policy::MinCoastSeconds;
    constexpr double MinBurnSeconds = Policy::MinBurnSeconds;
    constexpr double SettleSpeedMps = Policy::ProgramSettleSpeedMps;
    constexpr double LowSpeedRcsOnlyMps = Policy::LowSpeedRcsOnlyMps;
    constexpr double RcsDeltaVSeconds = Policy::RcsDeltaVSeconds;
    constexpr double NarrowApproachSeconds = Policy::NarrowApproachSeconds;
    constexpr double CurveNormalShare = Policy::CurveNormalShare;
    constexpr double OrientRateRadPerSec = Policy::OrientRateRadPerSec;
    constexpr double UnreachableSlackRad = Policy::UnreachableSlackRad;
    constexpr double TurnSettleSeconds = Policy::TurnSettleSeconds;

    // Reshaping uses the authority the profile itself was planned with
    // (its largest acceleration and deceleration); the capability values
    // are only a fallback for a profile without any.
    double profileForward = 0.0;
    double profileBraking = 0.0;
    for (const auto& sample : samples)
    {
        profileForward =
            std::max(profileForward, sample.longitudinalAccelerationMps2);
        profileBraking =
            std::max(profileBraking, -sample.longitudinalAccelerationMps2);
    }
    if (!(profileForward > 1.0e-6))
        profileForward = capability.plannedForwardAccelerationMps2;
    if (!(profileBraking > 1.0e-6))
        profileBraking = capability.plannedBrakingAccelerationMps2;

    auto& st = program.stations;
    st.resize(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const auto& in = samples[i];
        auto& out = st[i];
        out.progressMeters = in.progressMeters;
        out.speedMps = std::max(0.0, in.speedMps);
        out.longitudinalAccelerationMps2 = in.longitudinalAccelerationMps2;
        out.curvaturePerMeter = std::max(0.0, in.curvaturePerMeter);
        out.positionMapMeters = in.positionMapMeters;
        out.tangentMap = unitOr(in.tangentMap, glm::dvec3(0.0, 0.0, -1.0));
        out.normalMap = out.curvaturePerMeter > 1.0e-12
            ? unitOr(in.normalMap, glm::dvec3(0.0)) : glm::dvec3(0.0);
        out.narrow = in.narrow;
        out.narrowForwardMap = unitOr(in.narrowAxisMap, out.tangentMap);
        out.narrowUpMap = unitOr(in.narrowUpMap, glm::dvec3(0.0, 1.0, 0.0));
        out.narrowToleranceMeters = std::max(0.0, in.narrowToleranceMeters);
    }

    // Time, required acceleration and the main/RCS split from the station
    // speeds and accelerations.
    const auto derive = [&]()
    {
        double time = 0.0;
        for (std::size_t i = 0; i < st.size(); ++i)
        {
            auto& out = st[i];
            if (i > 0)
            {
                const double ds = std::max(
                    0.0, out.progressMeters - st[i - 1].progressMeters);
                const double v = std::max(
                    0.5, 0.5 * (out.speedMps + st[i - 1].speedMps));
                time += ds / v;
            }
            out.timeSeconds = time;
            out.requiredAccelerationMap =
                out.tangentMap * out.longitudinalAccelerationMps2 +
                out.normalMap * (out.speedMps * out.speedMps *
                                 out.curvaturePerMeter);
            // Near zero speed a manoeuvre ends: the last metres of a stop
            // (and any crawl) are RCS work, never a main burn.
            out.mainBurn =
                !out.narrow && out.speedMps > LowSpeedRcsOnlyMps &&
                glm::length(out.requiredAccelerationMap) >
                    MainShareOfRcs * capability.rcsTrajectoryAccelerationMps2;
        }
    };
    derive();

    // Narrow zones (Step 3): inside, RCS only — at a speed the RCS can stop
    // within the zone, changing speed only at the RCS rate. Before the zone
    // the main brakes to that speed (planned braking authority); after it
    // the main may accelerate again. Speeds are only lowered.
    {
        const double rcsRate =
            MainShareOfRcs * capability.rcsTrajectoryAccelerationMps2;
        std::vector<char> touched(st.size(), 0);
        for (std::size_t a = 0; a < st.size(); ++a)
        {
            if (!st[a].narrow || (a > 0 && st[a - 1].narrow))
                continue;
            std::size_t b = a;
            while (b + 1 < st.size() && st[b + 1].narrow)
                ++b;
            const double length =
                st[b].progressMeters - st[a].progressMeters;
            const double cap = std::sqrt(std::max(0.0, rcsRate * length));
            for (std::size_t i = a; i <= b; ++i)
                if (st[i].speedMps > cap)
                {
                    st[i].speedMps = cap;
                    touched[i] = 1;
                }
            for (std::size_t i = a + 1; i <= b; ++i)
            {
                const double ds =
                    st[i].progressMeters - st[i - 1].progressMeters;
                const double limit = std::sqrt(
                    st[i - 1].speedMps * st[i - 1].speedMps +
                    2.0 * rcsRate * ds);
                if (st[i].speedMps > limit)
                {
                    st[i].speedMps = limit;
                    touched[i] = 1;
                }
            }
            for (std::size_t i = b; i-- > a;)
            {
                const double ds =
                    st[i + 1].progressMeters - st[i].progressMeters;
                const double limit = std::sqrt(
                    st[i + 1].speedMps * st[i + 1].speedMps +
                    2.0 * rcsRate * ds);
                if (st[i].speedMps > limit)
                {
                    st[i].speedMps = limit;
                    touched[i] = 1;
                }
            }
            // The approach is a plateau at the zone speed, long enough to
            // settle the zone attitude (incl. roll) and the lateral position
            // before the entry: no speed-up toward the zone, the brake ends
            // before this plateau.
            const double entrySpeed = st[a].speedMps;
            const double approachStart =
                st[a].progressMeters - entrySpeed * NarrowApproachSeconds;
            std::size_t first = a;
            for (std::size_t i = a; i-- > 0;)
            {
                if (st[i].progressMeters < approachStart)
                    break;
                first = i;
                if (st[i].speedMps > entrySpeed)
                {
                    st[i].speedMps = entrySpeed;
                    touched[i] = 1;
                }
            }
            for (std::size_t i = first; i-- > 0;)
            {
                const double ds =
                    st[i + 1].progressMeters - st[i].progressMeters;
                const double limit = std::sqrt(
                    st[i + 1].speedMps * st[i + 1].speedMps +
                    2.0 * profileBraking * ds);
                if (st[i].speedMps <= limit)
                    break;
                st[i].speedMps = limit;
                touched[i] = 1;
            }
            for (std::size_t i = b + 1; i < st.size(); ++i)
            {
                const double ds =
                    st[i].progressMeters - st[i - 1].progressMeters;
                const double limit = std::sqrt(
                    st[i - 1].speedMps * st[i - 1].speedMps +
                    2.0 * profileForward * ds);
                if (st[i].speedMps <= limit)
                    break;
                st[i].speedMps = limit;
                touched[i] = 1;
            }
        }
        for (std::size_t i = 0; i < st.size(); ++i)
        {
            const bool nextToTouched =
                touched[i] || (i + 1 < st.size() && touched[i + 1]) ||
                (i > 0 && touched[i - 1]);
            if (!nextToTouched)
                continue;
            const std::size_t lo = i + 1 < st.size() ? i : i - 1;
            const double ds =
                st[lo + 1].progressMeters - st[lo].progressMeters;
            st[i].longitudinalAccelerationMps2 = ds > 1.0e-9
                ? (st[lo + 1].speedMps * st[lo + 1].speedMps -
                   st[lo].speedMps * st[lo].speedMps) / (2.0 * ds)
                : 0.0;
            if (st[i].narrow)
                st[i].longitudinalAccelerationMps2 = std::clamp(
                    st[i].longitudinalAccelerationMps2, -rcsRate, rcsRate);
        }
        derive();
    }

    // A curve flown on the main engine keeps its thrust on the curve axis:
    // a speed-up there may use only what the nozzle cone gives beside the
    // path-normal push. The remaining speed-up waits for the curve's end.
    {
        const double coneShare = std::tan(capability.gimbalLimitRad);
        bool limited = false;
        for (std::size_t i = 0; i + 1 < st.size(); ++i)
        {
            const double normal =
                st[i].speedMps * st[i].speedMps * st[i].curvaturePerMeter;
            if (st[i].curvaturePerMeter > 1.0e-12 &&
                normal > MainShareOfRcs *
                             capability.rcsTrajectoryAccelerationMps2 &&
                st[i].longitudinalAccelerationMps2 > coneShare * normal)
            {
                st[i].longitudinalAccelerationMps2 = coneShare * normal;
                limited = true;
            }
            // Forward envelope: speeds follow the (limited) accelerations.
            const double ds =
                st[i + 1].progressMeters - st[i].progressMeters;
            const double reach = std::sqrt(std::max(
                0.0,
                st[i].speedMps * st[i].speedMps +
                    2.0 * std::max(0.0, st[i].longitudinalAccelerationMps2) *
                        ds));
            if (limited && st[i].longitudinalAccelerationMps2 >= 0.0 &&
                st[i + 1].speedMps > reach)
            {
                st[i + 1].speedMps = reach;
                // After the curve the profile may accelerate again.
                if (st[i + 1].curvaturePerMeter <= 1.0e-12)
                    st[i + 1].longitudinalAccelerationMps2 =
                        capability.plannedForwardAccelerationMps2;
            }
            else if (st[i + 1].curvaturePerMeter <= 1.0e-12)
            {
                limited = false;
            }
        }
        derive();
    }

    // A narrow zone is crossed nose-first or, when the craft arrives from a
    // brake with its hull retro, stern-first (the hull box is symmetric):
    // no 180 degree turn just for the entry.
    for (std::size_t a = 0; a < st.size(); ++a)
    {
        if (!st[a].narrow || (a > 0 && st[a - 1].narrow))
            continue;
        bool fromBrake = false;
        for (std::size_t k = a; k-- > 0;)
            if (st[k].mainBurn)
            {
                fromBrake = st[k].longitudinalAccelerationMps2 < 0.0;
                break;
            }
        const glm::dvec3 desired =
            fromBrake ? -st[a].tangentMap : st[a].tangentMap;
        const double sign =
            glm::dot(st[a].narrowForwardMap, desired) >= 0.0 ? 1.0 : -1.0;
        for (std::size_t i = a; i < st.size() && st[i].narrow; ++i)
            st[i].narrowForwardMap = sign * st[i].narrowForwardMap;
    }

    // Turn windows. Two burns back to back whose thrust axes differ beyond
    // the nozzle cone leave the hull no time to turn. Where one side is a
    // longitudinal burn (its timing is free, unlike a curve's), the speed
    // profile is reshaped so that a coast of the planned turn time sits
    // between them: braking into a curve ends earlier, accelerating out of
    // a curve starts later. Speeds are only ever lowered, so the profile
    // stays within the planned braking/forward authority.
    const double turnRate =
        std::max(1.0e-3, capability.plannedTurnRateRadPerSec);
    const auto burnAxis = [&](std::size_t i)
    {
        if (st[i].narrow)
            return st[i].narrowForwardMap;
        return unitOr(
            st[i].requiredAccelerationMap,
            st[i].longitudinalAccelerationMps2 < 0.0
                ? -st[i].tangentMap : st[i].tangentMap);
    };
    const auto longitudinal = [&](std::size_t i)
    {
        if (st[i].narrow)
            return false;
        const glm::dvec3 a = st[i].requiredAccelerationMap;
        const double along = glm::dot(a, st[i].tangentMap);
        return glm::length(a - st[i].tangentMap * along) <=
            CurveNormalShare * glm::length(a);
    };
    // One reshape can create the next junction (slowing a curve makes the
    // brake before it run up to the curve), hence a few passes.
    constexpr int TurnWindowPasses = 6;
    for (int pass = 0;
         pass < TurnWindowPasses &&
         profileBraking > 1.0e-9 && profileForward > 1.0e-9;
         ++pass)
    {
        std::vector<char> reshaped(st.size(), 0);
        // Braking envelope backward from station `from` (exclusive).
        const auto brakeBackwardFrom = [&](std::size_t from)
        {
            for (std::size_t i = from; i-- > 0;)
            {
                const double ds =
                    st[i + 1].progressMeters - st[i].progressMeters;
                const double limit = std::sqrt(
                    st[i + 1].speedMps * st[i + 1].speedMps +
                    2.0 * profileBraking * ds);
                if (st[i].speedMps <= limit)
                    break;
                st[i].speedMps = limit;
                reshaped[i] = 1;
            }
        };
        for (std::size_t k = 0; k + 1 < st.size(); ++k)
        {
            if (!st[k].mainBurn)
                continue;
            // The next burn station: the same run continuing, or the start
            // of the next burn after a coast that may be too short to turn.
            std::size_t j = k + 1;
            while (j < st.size() && !st[j].mainBurn &&
                   !(st[j].narrow && !st[j - 1].narrow))
                ++j;
            if (j >= st.size())
                break;
            const double turn = angleBetween(burnAxis(k), burnAxis(j));
            if (turn <= capability.gimbalLimitRad + UnreachableSlackRad)
                continue;
            const double windowSeconds = turn / turnRate + TurnSettleSeconds;
            if (st[j].timeSeconds - st[k].timeSeconds >= windowSeconds)
                continue;
            if (longitudinal(k) && longitudinal(j) &&
                st[k].longitudinalAccelerationMps2 *
                        st[j].longitudinalAccelerationMps2 < 0.0)
            {
                if (st[k].longitudinalAccelerationMps2 > 0.0)
                {
                    // Accelerating straight into braking (a speed peak):
                    // lower the peak until a plateau of the turn time
                    // separates the two burns.
                    // The plateau stays inside this peak: from the start of
                    // its speed-up to the end of its brake. If no plateau
                    // fits there, the profile is left as it is.
                    std::size_t runStart = k;
                    while (runStart > 0 && st[runStart - 1].mainBurn &&
                           longitudinal(runStart - 1) &&
                           st[runStart - 1].longitudinalAccelerationMps2 > 0.0)
                        --runStart;
                    std::size_t runEnd = j;
                    while (runEnd + 1 < st.size() && st[runEnd + 1].mainBurn &&
                           longitudinal(runEnd + 1) &&
                           st[runEnd + 1].longitudinalAccelerationMps2 < 0.0)
                        ++runEnd;
                    const double floor = std::max(
                        st[runStart].speedMps, st[runEnd].speedMps);
                    double cap = st[j].speedMps;
                    std::size_t lo = k;
                    std::size_t hi = j;
                    bool fits = false;
                    for (int step = 0; step < 80 && cap > floor; ++step)
                    {
                        lo = k;
                        hi = j;
                        while (lo > runStart && st[lo - 1].speedMps >= cap)
                            --lo;
                        while (hi < runEnd && st[hi + 1].speedMps >= cap)
                            ++hi;
                        if (st[hi].progressMeters - st[lo].progressMeters >=
                            cap * windowSeconds)
                        {
                            fits = true;
                            break;
                        }
                        cap *= 0.97;
                    }
                    // No plateau fits inside this peak: the speed-up is
                    // pointless right before the brake; drop it (cap at
                    // the speed the peak starts from).
                    if (!fits)
                    {
                        cap = floor;
                        lo = runStart;
                        hi = runEnd;
                    }
                    for (std::size_t i = lo; i <= hi; ++i)
                        if (st[i].speedMps > cap)
                        {
                            st[i].speedMps = cap;
                            reshaped[i] = 1;
                        }
                }
                else
                {
                    // Braking straight into accelerating (a speed valley,
                    // e.g. into a zone limit): a plateau at the valley speed
                    // of the turn time; the brake ends earlier and the
                    // speed-up starts later.
                    const double cap =
                        std::min(st[k].speedMps, st[j].speedMps);
                    const double half = 0.5 * cap * windowSeconds;
                    const double from = st[j].progressMeters - half;
                    const double to = st[j].progressMeters + half;
                    std::size_t first = j;
                    for (std::size_t i = 0; i < st.size(); ++i)
                        if (st[i].progressMeters >= from &&
                            st[i].progressMeters <= to)
                        {
                            first = std::min(first, i);
                            if (st[i].speedMps > cap)
                            {
                                st[i].speedMps = cap;
                                reshaped[i] = 1;
                            }
                        }
                    brakeBackwardFrom(first);
                    for (std::size_t i = 1; i < st.size(); ++i)
                    {
                        if (st[i].progressMeters <= to)
                            continue;
                        const double ds =
                            st[i].progressMeters - st[i - 1].progressMeters;
                        const double limit = std::sqrt(
                            st[i - 1].speedMps * st[i - 1].speedMps +
                            2.0 * profileForward *
                                ds);
                        if (st[i].speedMps <= limit)
                            break;
                        st[i].speedMps = limit;
                        reshaped[i] = 1;
                    }
                }
            }
            else if (longitudinal(k) && !longitudinal(j))
            {
                // Finish braking before the curve.
                const double cap = st[j].speedMps;
                const double windowStart =
                    st[j].progressMeters - cap * windowSeconds;
                for (std::size_t i = j; i-- > 0;)
                {
                    const double ds =
                        st[i + 1].progressMeters - st[i].progressMeters;
                    const double limit = st[i].progressMeters >= windowStart
                        ? cap
                        : std::sqrt(
                              st[i + 1].speedMps * st[i + 1].speedMps +
                              2.0 * profileBraking *
                                  ds);
                    if (st[i].speedMps <= limit)
                    {
                        if (st[i].progressMeters < windowStart)
                            break;
                        continue;
                    }
                    st[i].speedMps = limit;
                    reshaped[i] = 1;
                }
            }
            else if (!longitudinal(k) && longitudinal(j) &&
                     st[j].longitudinalAccelerationMps2 < 0.0)
            {
                // Braking right after a curve: fly the curve at the speed
                // that lets the brake begin one turn window later.
                const double windowEnd =
                    st[k].progressMeters + st[k].speedMps * windowSeconds;
                std::size_t m = k + 1;
                while (m + 1 < st.size() && st[m].progressMeters < windowEnd)
                    ++m;
                const double cap = st[m].speedMps;
                for (std::size_t i = k + 1; i <= m; ++i)
                    if (st[i].speedMps > cap)
                    {
                        st[i].speedMps = cap;
                        reshaped[i] = 1;
                    }
                std::size_t i = k + 1;
                while (i > 0 && st[i - 1].mainBurn && !longitudinal(i - 1))
                {
                    --i;
                    if (st[i].speedMps > cap)
                    {
                        st[i].speedMps = cap;
                        reshaped[i] = 1;
                    }
                }
                brakeBackwardFrom(i);
            }
            else if (!longitudinal(k) && longitudinal(j))
            {
                // Start accelerating after the curve.
                const double cap = st[k].speedMps;
                const double windowEnd =
                    st[k].progressMeters + cap * windowSeconds;
                for (std::size_t i = k + 1; i < st.size(); ++i)
                {
                    const double ds =
                        st[i].progressMeters - st[i - 1].progressMeters;
                    const double limit = st[i].progressMeters <= windowEnd
                        ? cap
                        : std::sqrt(
                              st[i - 1].speedMps * st[i - 1].speedMps +
                              2.0 * profileForward *
                                  ds);
                    if (st[i].speedMps <= limit)
                    {
                        if (st[i].progressMeters > windowEnd)
                            break;
                        continue;
                    }
                    st[i].speedMps = limit;
                    reshaped[i] = 1;
                }
            }
        }
        // Longitudinal acceleration of the reshaped stretches from the new
        // speeds (v dv/ds); untouched stations keep the profile's value.
        for (std::size_t i = 0; i < st.size(); ++i)
        {
            const bool touched =
                reshaped[i] || (i + 1 < st.size() && reshaped[i + 1]) ||
                (i > 0 && reshaped[i - 1]);
            if (!touched)
                continue;
            const std::size_t lo = i + 1 < st.size() ? i : i - 1;
            const double ds =
                st[lo + 1].progressMeters - st[lo].progressMeters;
            st[i].longitudinalAccelerationMps2 = ds > 1.0e-9
                ? (st[lo + 1].speedMps * st[lo + 1].speedMps -
                   st[lo].speedMps * st[lo].speedMps) / (2.0 * ds)
                : 0.0;
        }
        derive();
        if (std::find(reshaped.begin(), reshaped.end(), 1) == reshaped.end())
            break;
    }

    // Close short coast gaps between burns, then drop short burns.
    const auto runs = [&](bool value, auto&& visit)
    {
        std::size_t i = 0;
        while (i < st.size())
        {
            if (st[i].mainBurn != value) { ++i; continue; }
            std::size_t j = i;
            while (j + 1 < st.size() && st[j + 1].mainBurn == value)
                ++j;
            visit(i, j);
            i = j + 1;
        }
    };
    // First drop burns that are RCS work: too short, or a velocity change
    // the RCS budget clears within RcsDeltaVSeconds (e.g. a sub-m/s trim of
    // a profile re-anchored on the actual speed). Their accelerations are
    // bounded to what RCS gives. Only then close short coast gaps, so a
    // trim cannot glue itself to a real burn.
    const double rcsShare =
        MainShareOfRcs * capability.rcsTrajectoryAccelerationMps2;
    const auto dropRcsWork = [&](std::size_t a, std::size_t b)
    {
        const double end = b + 1 < st.size()
            ? st[b + 1].timeSeconds : st[b].timeSeconds;
        double deltaV = 0.0;
        for (std::size_t k = a; k <= b; ++k)
        {
            const double dt = k + 1 < st.size()
                ? st[k + 1].timeSeconds - st[k].timeSeconds : 0.0;
            deltaV += glm::length(st[k].requiredAccelerationMap) * dt;
        }
        if (end - st[a].timeSeconds < MinBurnSeconds &&
            deltaV > RcsDeltaVSeconds * rcsShare)
        {
            // Too short for the main but not small: keep it (the turn
            // windows and the executor's gate deal with it).
            return;
        }
        if (deltaV > RcsDeltaVSeconds * rcsShare)
            return;
        // A small longitudinal trim on a straight: RCS flies it over this
        // run and the coast after it. The speeds follow the RCS rate; the
        // trim must be complete before the next burn begins, so the craft
        // is never faster than the profile where the next manoeuvre starts.
        for (std::size_t k = a; k <= b; ++k)
            if (st[k].curvaturePerMeter > 1.0e-12)
                return;
        std::size_t next = b + 1;
        while (next < st.size() && !st[next].mainBurn)
            ++next;
        std::vector<double> speeds(st.size());
        for (std::size_t k = 0; k < st.size(); ++k)
            speeds[k] = st[k].speedMps;
        for (std::size_t k = a + 1; k < std::min(next + 1, st.size()); ++k)
        {
            const double dt = st[k].timeSeconds - st[k - 1].timeSeconds;
            const double step = rcsShare * std::max(0.0, dt);
            speeds[k] = std::clamp(
                st[k].speedMps, speeds[k - 1] - step, speeds[k - 1] + step);
        }
        const std::size_t check = std::min(next, st.size() - 1);
        if (std::abs(speeds[check] - st[check].speedMps) > 0.05)
            return;
        for (std::size_t k = a; k < std::min(next, st.size()); ++k)
        {
            st[k].mainBurn = false;
            st[k].speedMps = speeds[k];
            const std::size_t lo = k + 1 < st.size() ? k : k - 1;
            const double ds =
                st[lo + 1].progressMeters - st[lo].progressMeters;
            st[k].longitudinalAccelerationMps2 = ds > 1.0e-9
                ? std::clamp(
                      (speeds[lo + 1] * speeds[lo + 1] -
                       speeds[lo] * speeds[lo]) / (2.0 * ds),
                      -rcsShare, rcsShare)
                : 0.0;
        }
    };
    runs(true, [&](std::size_t a, std::size_t b)
    {
        // A speed-up and the brake right after it are separate manoeuvres:
        // split the run where the along-track requirement changes sign.
        std::size_t from = a;
        for (std::size_t k = a + 1; k <= b + 1; ++k)
            if (k > b ||
                (st[k].longitudinalAccelerationMps2 >= 0.0) !=
                    (st[k - 1].longitudinalAccelerationMps2 >= 0.0))
            {
                dropRcsWork(from, k - 1);
                from = k;
            }
    });
    runs(false, [&](std::size_t a, std::size_t b)
    {
        if (a > 0 && b + 1 < st.size() &&
            st[b + 1].timeSeconds - st[a - 1].timeSeconds < MinCoastSeconds)
            for (std::size_t k = a; k <= b; ++k)
                st[k].mainBurn = true;
    });

    // Planned thrust axis where the main burns; elsewhere none.
    std::vector<glm::dvec3> axis(st.size(), glm::dvec3(0.0));
    // A burn station whose own requirement is negligible (a short coast
    // merged into a burn) holds the axis of the burn around it instead of a
    // default direction.
    const double negligible =
        0.25 * MainShareOfRcs * capability.rcsTrajectoryAccelerationMps2;
    const auto computeAxes = [&]()
    {
        for (std::size_t i = 0; i < st.size(); ++i)
        {
            axis[i] = glm::dvec3(0.0);
            if (!st[i].mainBurn)
                continue;
            if (glm::length(st[i].requiredAccelerationMap) > negligible)
                axis[i] = glm::normalize(st[i].requiredAccelerationMap);
            else if (i > 0 && st[i - 1].mainBurn)
                axis[i] = axis[i - 1];
        }
        // Leading negligible stations of a run take the run's next axis.
        for (std::size_t i = st.size(); i-- > 0;)
            if (st[i].mainBurn && glm::length(axis[i]) < 0.5)
                axis[i] = i + 1 < st.size() && st[i + 1].mainBurn
                    ? axis[i + 1]
                    : (st[i].longitudinalAccelerationMps2 < 0.0
                           ? -st[i].tangentMap : st[i].tangentMap);
    };

    const double rate = std::max(1.0e-3, capability.plannedTurnRateRadPerSec);
    std::vector<glm::dvec3> ahead(st.size());
    const auto planHull = [&]()
    {
        // Backward pass: a burn holds its own axis (the current manoeuvre
        // has priority); a coast turns toward the axis of the burn that
        // follows it early enough to be there when that burn begins.
        glm::dvec3 next = unitOr(initialHullForwardMap, st.back().tangentMap);
        // A route that ends without a burn keeps the hull of its last burn.
        for (std::size_t k = st.size(); k-- > 0;)
            if (st[k].mainBurn) { next = axis[k]; break; }
        // Two burns back to back (brake straight into a curve, a curve into
        // an acceleration) have no coast to turn in: the hull starts onto
        // the next axis inside the burn, as late as its rate allows. A burn
        // continuing with a turning axis (an arc) is led the same way.
        for (std::size_t k = st.size(); k-- > 0;)
        {
            if (st[k].narrow)
            {
                next = st[k].narrowForwardMap;
            }
            else if (st[k].mainBurn)
            {
                const bool burnContinues =
                    k + 1 < st.size() && st[k + 1].mainBurn;
                const double dt = k + 1 < st.size()
                    ? st[k + 1].timeSeconds - st[k].timeSeconds : 0.0;
                next = burnContinues
                    ? stepToward(next, axis[k], rate * dt, st[k].tangentMap)
                    : axis[k];
            }
            ahead[k] = next;
        }
        // Forward pass: never turn faster than the planned rate. It starts
        // at the craft's present station with its present hull; stations
        // behind it are history and keep that hull.
        glm::dvec3 hull = unitOr(initialHullForwardMap, ahead.front());
        for (std::size_t k = 0; k < st.size(); ++k)
        {
            const bool behind = k + 1 < st.size() &&
                st[k + 1].progressMeters <= startProgressMeters;
            const double dt = k > 0 && !behind
                ? st[k].timeSeconds - st[k - 1].timeSeconds : 0.0;
            if (!behind)
                hull = stepToward(
                    hull, ahead[k], rate * dt, st[k].tangentMap);
            st[k].hullForwardMap = hull;
        }
    };

    // A burn the hull cannot reach within its own duration (e.g. a short
    // speed top-up while the hull already points retro) is not flown: its
    // stations become coast and the plan is made again.
    constexpr int MaxPlanPasses = 4;
    for (int pass = 0; pass < MaxPlanPasses; ++pass)
    {
        computeAxes();
        planHull();
        bool dropped = false;
        runs(true, [&](std::size_t a, std::size_t b)
        {
            // Judged ahead of the craft only (behind it is history).
            if (st[b].progressMeters < startProgressMeters)
                return;
            std::size_t reachable = 0;
            std::size_t counted = 0;
            for (std::size_t k = a; k <= b; ++k)
            {
                if (st[k].progressMeters < startProgressMeters)
                    continue;
                ++counted;
                if (angleBetween(st[k].hullForwardMap, axis[k]) <=
                        capability.gimbalLimitRad + UnreachableSlackRad)
                    ++reachable;
            }
            // Only an optional speed-up may be dropped; a braking or curve
            // burn is required by the route ahead and stays in the plan.
            bool optional = true;
            for (std::size_t k = a; k <= b && optional; ++k)
                optional =
                    st[k].longitudinalAccelerationMps2 > 0.0 &&
                    longitudinal(k);
            if (optional && 2 * reachable < counted)
            {
                for (std::size_t k = a; k <= b; ++k)
                    st[k].mainBurn = false;
                dropped = true;
            }
        });
        if (!dropped)
            break;
    }
    computeAxes();
    planHull();
    for (std::size_t k = 0; k < st.size(); ++k)
    {
        if (!st[k].mainBurn || st[k].progressMeters < startProgressMeters)
            continue;
        const double deficit = std::max(
            0.0,
            angleBetween(st[k].hullForwardMap, axis[k]) -
                capability.gimbalLimitRad);
        if (deficit > program.worstThrustDeficitRad)
        {
            program.worstThrustDeficitRad = deficit;
            program.worstThrustDeficitProgressMeters = st[k].progressMeters;
        }
    }

    // Label stations and group them into phases.
    std::vector<ManoeuvrePhaseKind> kind(st.size());
    for (std::size_t k = 0; k < st.size(); ++k)
    {
        if (st[k].mainBurn)
        {
            // Labelled by its (possibly inherited) planned axis.
            const glm::dvec3 a = axis[k];
            const double along = glm::dot(a, st[k].tangentMap);
            const double normal = glm::length(a - st[k].tangentMap * along);
            kind[k] = normal > CurveNormalShare * glm::length(a)
                ? ManoeuvrePhaseKind::Curve
                : along >= 0.0 ? ManoeuvrePhaseKind::Burn
                               : ManoeuvrePhaseKind::Brake;
            continue;
        }
        const bool finalStop =
            k + 1 == st.size() || st[k].speedMps <= SettleSpeedMps;
        if (finalStop && st[k].speedMps <= SettleSpeedMps &&
            k + 1 >= st.size() - 1)
        {
            kind[k] = ManoeuvrePhaseKind::Settle;
            continue;
        }
        const double dt = k + 1 < st.size()
            ? st[k + 1].timeSeconds - st[k].timeSeconds : 0.0;
        const double turned = k + 1 < st.size()
            ? angleBetween(st[k].hullForwardMap, st[k + 1].hullForwardMap)
            : 0.0;
        kind[k] = dt > 1.0e-9 && turned / dt > OrientRateRadPerSec
            ? ManoeuvrePhaseKind::Orient : ManoeuvrePhaseKind::Coast;
    }
    for (std::size_t k = 0; k < st.size();)
    {
        std::size_t j = k;
        while (j + 1 < st.size() && kind[j + 1] == kind[k])
            ++j;
        ManoeuvrePhase phase;
        phase.kind = kind[k];
        phase.firstStation = k;
        phase.lastStation = j;
        phase.startProgressMeters = st[k].progressMeters;
        phase.endProgressMeters =
            j + 1 < st.size() ? st[j + 1].progressMeters
                              : st[j].progressMeters;
        phase.startTimeSeconds = st[k].timeSeconds;
        phase.endTimeSeconds =
            j + 1 < st.size() ? st[j + 1].timeSeconds : st[j].timeSeconds;
        if (phase.kind == ManoeuvrePhaseKind::Orient &&
            angleBetween(st[k].hullForwardMap, st[j].hullForwardMap) >
                0.5 * 3.14159265358979323846)
            phase.kind = ManoeuvrePhaseKind::Flip;
        for (std::size_t m = k; m <= j; ++m)
            st[m].phaseIndex = program.phases.size();
        program.phases.push_back(phase);
        k = j + 1;
    }
    program.valid = !program.phases.empty();
    return program;
}

} // namespace game::navigation::autopilot
