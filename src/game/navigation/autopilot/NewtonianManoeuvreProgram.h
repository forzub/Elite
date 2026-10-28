#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/NewtonianFlightPolicy.h"
#include "src/game/navigation/autopilot/NewtonianManoeuvreTypes.h"
#include "src/game/navigation/autopilot/NewtonianMotionProfile.h"

namespace game::navigation::autopilot
{
// Newtonian manoeuvre program: see NewtonianManoeuvreTypes.h. The speed
// profile it is compiled on is shaped by NewtonianMotionProfile.h.

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
    constexpr double RcsDeltaVSeconds = Policy::RcsDeltaVSeconds;
    constexpr double CurveNormalShare = Policy::CurveNormalShare;
    constexpr double OrientRateRadPerSec = Policy::OrientRateRadPerSec;
    constexpr double UnreachableSlackRad = Policy::UnreachableSlackRad;

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

    // The executable speed profile (one owner).
    shapeNewtonianMotionProfile(st, capability);
    const auto longitudinal = [&](std::size_t i)
    {
        return isLongitudinalStation(st[i]);
    };

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
