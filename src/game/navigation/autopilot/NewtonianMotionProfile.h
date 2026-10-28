#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/NewtonianFlightPolicy.h"
#include "src/game/navigation/autopilot/NewtonianManoeuvreTypes.h"

namespace game::navigation::autopilot
{

// Time, required acceleration and the main/RCS split of the stations from
// their speeds and accelerations.
inline void deriveStationKinematics(
    std::vector<ManoeuvreStation>& st,
    const ManoeuvreCapability& capability
) noexcept
{
    using Policy = NewtonianFlightPolicy;
    constexpr double MainShareOfRcs = Policy::MainShareOfRcs;
    constexpr double LowSpeedRcsOnlyMps = Policy::LowSpeedRcsOnlyMps;
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
}

// Newtonian motion profile: the ONE owner of the executable speed profile
// along the route. Input: the route stations (geometry, the runtime speed
// profile of the route layer, narrow zones) and the craft's capability.
// It shapes the speeds so that every manoeuvre is executable by this ship:
// narrow zones flown on RCS with an approach plateau, curve speed-ups within
// the nozzle cone, and turn windows (the hull's planned turn time) between
// burns whose thrust axes differ. Speeds are only lowered. The manoeuvre
// compiler then only labels phases and plans the hull on this profile; it
// is not a second speed planner. `startProgressMeters` is where the craft is:
// a manoeuvre it has already entered is not re-planned. `decidedCaps` (owned
// by the caller for the whole route) holds the speed limits decided by
// earlier calls; they are applied first and new ones are appended.
inline void shapeNewtonianMotionProfile(
    std::vector<ManoeuvreStation>& st,
    const ManoeuvreCapability& capability,
    double startProgressMeters = 0.0,
    std::vector<ManoeuvreSpeedCap>* decidedCaps = nullptr
)
{
    using namespace manoeuvre_detail;
    using Policy = NewtonianFlightPolicy;
    constexpr double MainShareOfRcs = Policy::MainShareOfRcs;
    constexpr double NarrowApproachSeconds = Policy::NarrowApproachSeconds;
    constexpr double UnreachableSlackRad = Policy::UnreachableSlackRad;
    constexpr double TurnSettleSeconds = Policy::TurnSettleSeconds;

    // Reshaping uses the authority the profile itself was planned with
    // (its largest acceleration and deceleration), never more than the
    // live capability: after a main loss the stretch already flown still
    // carries the healthy ramps. The capability values are also the
    // fallback for a profile without any.
    double profileForward = 0.0;
    double profileBraking = 0.0;
    for (const auto& station : st)
    {
        profileForward =
            std::max(profileForward, station.longitudinalAccelerationMps2);
        profileBraking =
            std::max(profileBraking, -station.longitudinalAccelerationMps2);
    }
    if (!(profileForward > 1.0e-6))
        profileForward = capability.plannedForwardAccelerationMps2;
    if (!(profileBraking > 1.0e-6))
        profileBraking = capability.plannedBrakingAccelerationMps2;
    if (capability.plannedForwardAccelerationMps2 > 1.0e-6)
        profileForward =
            std::min(profileForward, capability.plannedForwardAccelerationMps2);
    if (capability.plannedBrakingAccelerationMps2 > 1.0e-6)
        profileBraking =
            std::min(profileBraking, capability.plannedBrakingAccelerationMps2);

    const auto derive = [&]() { deriveStationKinematics(st, capability); };
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
        return isLongitudinalStation(st[i]);
    };
    // A speed cap over [lo, hi]: the stretch at the cap, braking into it
    // and accelerating out of it within the profile's authority.
    const auto applyCap = [&](std::size_t lo, std::size_t hi, double cap,
                              std::vector<char>& changed)
    {
        for (std::size_t i = lo; i <= hi; ++i)
            if (st[i].speedMps > cap)
            {
                st[i].speedMps = cap;
                changed[i] = 1;
            }
        for (std::size_t i = lo; i-- > 0;)
        {
            const double ds = st[i + 1].progressMeters - st[i].progressMeters;
            const double limit = std::sqrt(
                st[i + 1].speedMps * st[i + 1].speedMps +
                2.0 * profileBraking * ds);
            if (st[i].speedMps <= limit)
                break;
            st[i].speedMps = limit;
            changed[i] = 1;
        }
        for (std::size_t i = hi + 1; i < st.size(); ++i)
        {
            const double ds = st[i].progressMeters - st[i - 1].progressMeters;
            const double limit = std::sqrt(
                st[i - 1].speedMps * st[i - 1].speedMps +
                2.0 * profileForward * ds);
            if (st[i].speedMps <= limit)
                break;
            st[i].speedMps = limit;
            changed[i] = 1;
        }
    };
    // Accelerations of changed stretches from the new speeds (v dv/ds).
    const auto reaccelerate = [&](const std::vector<char>& changed)
    {
        for (std::size_t i = 0; i < st.size(); ++i)
        {
            const bool touched =
                changed[i] || (i + 1 < st.size() && changed[i + 1]) ||
                (i > 0 && changed[i - 1]);
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
    };
    if (decidedCaps && profileBraking > 1.0e-9 && profileForward > 1.0e-9)
    {
        std::vector<char> changed(st.size(), 0);
        for (const auto& cap : *decidedCaps)
        {
            std::size_t lo = st.size();
            std::size_t hi = 0;
            for (std::size_t i = 0; i < st.size(); ++i)
                if (st[i].progressMeters >= cap.fromProgressMeters &&
                    st[i].progressMeters <= cap.toProgressMeters)
                {
                    lo = std::min(lo, i);
                    hi = std::max(hi, i);
                }
            if (lo <= hi)
                applyCap(lo, hi, cap.speedMps, changed);
        }
        reaccelerate(changed);
        derive();
    }

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
            else if (!longitudinal(k) && !longitudinal(j) &&
                     st[k].curvaturePerMeter > 1.0e-12 &&
                     st[j].curvaturePerMeter > 1.0e-12)
            {
                // Curve into a curve whose thrust axis is beyond the cone
                // (e.g. an S between two portals): the curves' timing is
                // fixed by geometry, so speed decides. Fly both curves and
                // the gap between them at the higher of: the speed at which
                // both curves are RCS work (no main, no hull swing at all),
                // and the speed at which the gap lasts the turn window.
                // Two different curves: a straight lies between them (a
                // burn gap inside one curve is no curve-to-curve junction).
                bool straightBetween = false;
                for (std::size_t i = k + 1; i < j && !straightBetween; ++i)
                    straightBetween = st[i].curvaturePerMeter <= 1.0e-12;
                if (!straightBetween)
                    continue;
                std::size_t runStart = k;
                while (runStart > 0 &&
                       st[runStart - 1].curvaturePerMeter > 1.0e-12)
                    --runStart;
                std::size_t runEnd = j;
                while (runEnd + 1 < st.size() &&
                       st[runEnd + 1].curvaturePerMeter > 1.0e-12)
                    ++runEnd;
                double curvature = 0.0;
                for (std::size_t i = runStart; i <= runEnd; ++i)
                    curvature = std::max(curvature, st[i].curvaturePerMeter);
                const double rcsRate =
                    MainShareOfRcs * capability.rcsTrajectoryAccelerationMps2;
                // A margin below the burn threshold so the curve stays RCS
                // work.
                constexpr double RcsCurveMargin = 0.95;
                const double rcsCurveSpeed = std::sqrt(
                    std::max(0.0, RcsCurveMargin * rcsRate / curvature));
                const double gap =
                    st[j].progressMeters - st[k].progressMeters;
                const double gapSpeed = gap / windowSeconds;
                const double cap = std::max(rcsCurveSpeed, gapSpeed);
                // Only ahead of the craft with room to turn and brake to
                // the new speed before the first curve: a manoeuvre already
                // entered is not re-planned.
                std::size_t now = 0;
                while (now + 1 < st.size() &&
                       st[now + 1].progressMeters <= startProgressMeters)
                    ++now;
                const double speedNow = st[now].speedMps;
                const double room =
                    st[runStart].progressMeters - startProgressMeters;
                const double needed =
                    speedNow * windowSeconds +
                    std::max(0.0, speedNow * speedNow - cap * cap) /
                        (2.0 * profileBraking);
                if (room < needed)
                    continue;
                bool lowers = false;
                for (std::size_t i = runStart; i <= runEnd && !lowers; ++i)
                    lowers = st[i].speedMps > cap;
                if (!lowers)
                    continue;
                applyCap(runStart, runEnd, cap, reshaped);
                if (decidedCaps)
                    decidedCaps->push_back({st[runStart].progressMeters,
                        st[runEnd].progressMeters, cap});
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

}

} // namespace game::navigation::autopilot
