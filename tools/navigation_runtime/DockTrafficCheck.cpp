// Long accelerated run of the dock traffic + dispatcher (no game, no render).
//
//   EliteDockTrafficCheck [--hours H] [--cobretts N] [--interval S] [--seed S] [--log PATH]
//
// Every step: dispatcher invariants (one ship per hold / pad / lane), pairwise
// ship separation (bounding spheres), ships vs the dock structure (slot,
// corridor, chamber walls, outer body, service block). Logs traffic and
// dispatcher events + a periodic facility snapshot. Exit code 1 on any
// violation.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>

#include "src/game/docking/model/HexDockFacility.h"
#include "src/game/presentation/DockDispatcherTerminalText.h"
#include "src/game/traffic/DockServiceTraffic.h"

using namespace game::traffic::dock;
namespace hex = game::docking::hex;

namespace
{

// Max of a vector's projection over the 8 OBB corners.
double obbExtentAlong(const glm::dmat3& basis, const glm::dvec3& half, const glm::dvec3& axis)
{
    return std::abs(glm::dot(basis[0], axis)) * half.x + std::abs(glm::dot(basis[1], axis)) * half.y +
           std::abs(glm::dot(basis[2], axis)) * half.z;
}

struct StructureCheck
{
    std::string violation;
};

// Pose in the FIXED frame -> problems with the dock structure.
std::string structureViolation(const TrafficPose& pose, const TrafficShipClass& c, double phase)
{
    const auto& g = hex::hexDockGeometry();
    const glm::dvec3 half {c.width * 0.5, c.height * 0.5, c.length * 0.5};
    const glm::dmat3 toRot = glm::transpose(hex::rotationAboutZ(phase));
    const glm::dvec3 p = toRot * pose.position;
    const glm::dmat3 b = toRot * pose.basis;
    const double zMin = p.z - obbExtentAlong(b, half, {0, 0, 1});
    const double zMax = p.z + obbExtentAlong(b, half, {0, 0, 1});
    double radial = 0.0; // max distance to the axis measured on wall normals
    for (int k = 0; k < 6; ++k)
    {
        const glm::dvec3 n = hex::wallNormal(k);
        radial = std::max(radial, glm::dot(p, n) + obbExtentAlong(b, half, n));
    }
    // Service block (fixed frame).
    {
        const glm::dvec3 q = pose.position;
        const double sz = g.serviceCenterZ();
        const double ex = obbExtentAlong(pose.basis, half, {1, 0, 0});
        const double ey = obbExtentAlong(pose.basis, half, {0, 1, 0});
        const double ez = obbExtentAlong(pose.basis, half, {0, 0, 1});
        if (std::abs(q.z - sz) < g.serviceHalfLength + ez && std::abs(q.x) < g.serviceHalfSize + ex &&
            std::abs(q.y) < g.serviceHalfSize + ey)
            return "inside the service block";
    }
    if (zMax < g.entryFaceZ || zMin > g.backFaceZ)
        return {}; // fully in front of / behind the dock body
    // Overlaps the dock body in z: must be inside the free volume.
    if (radial > g.outerApothem + 0.0 && zMin < g.entryFaceZ && zMax > g.entryFaceZ)
    {
        // Crossing the entry face: only through the slot.
    }
    const double ex = obbExtentAlong(b, half, {1, 0, 0});
    const double ey = obbExtentAlong(b, half, {0, 1, 0});
    if (zMin < g.chamberFrontZ)
    {
        // Entry corridor part: |x| <= slotWidth/2, |y| <= slotHeight/2.
        if (std::abs(p.x) + ex > g.slotWidth * 0.5 || std::abs(p.y) + ey > g.slotHeight * 0.5)
        {
            char text[160];
            std::snprintf(text, sizeof(text), "outside the slot in the corridor (x %.1f+%.1f y %.1f+%.1f)", p.x, ex, p.y, ey);
            return text;
        }
    }
    if (zMax > g.chamberFrontZ)
    {
        if (zMax > g.chamberBackZ)
            return "into the chamber back wall";
        if (radial > g.innerApothem + 0.5)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "into a chamber wall (radial %.1f > %.1f)", radial, g.innerApothem);
            return text;
        }
    }
    return {};
}

} // namespace

int main(int argc, char** argv)
{
    double hours = 6.0;
    TrafficConfig config;
    std::string logPath = "D:/tmp/elite_dock_dispatcher_run.log";
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--hours")) hours = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--cobretts")) config.cobrettCount = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--interval")) config.shuttleIntervalSeconds = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--emergency")) config.emergency = true;
        else if (!std::strcmp(argv[i], "--seed")) config.seed = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--log")) logPath = argv[++i];
    }
    const double omega = hex::hexDockGeometry().spinRadPerSecond();
    DockServiceTraffic traffic(config, [omega](double t) { return omega * t; });
    std::ofstream log(logPath);
    log << "# dock traffic check: hours=" << hours << " cobretts=" << config.cobrettCount
        << " shuttle_interval_s=" << config.shuttleIntervalSeconds << " seed=" << config.seed << "\n";

    const double dt = 0.5;
    const double end = hours * 3600.0;
    std::size_t invariantViolations = 0, separationViolations = 0, structureViolations = 0;
    double minGap = 1.0e18;
    std::string minGapWhat;
    std::map<std::string, double> minGapByZone;
    double nextSnapshot = 0.0;
    std::size_t terminalSamples = 0;
    std::map<std::string, int> lastStructureLog;

    struct Wait
    {
        bool active = false;
        bool departure = false;
        double since = 0.0;
        double estimate = 0.0;
    };
    std::map<std::uint64_t, Wait> waits;
    double maxDepartureWait = 0.0, maxHoldWait = 0.0, estimateError = 0.0;
    std::size_t estimateSamples = 0;
    for (double t = 0.0; t <= end; t += dt)
    {
        traffic.advance(t);
        for (const auto& line : traffic.drainLog())
        {
            char stamp[32];
            std::snprintf(stamp, sizeof(stamp), "%9.1f ", line.timeSeconds);
            log << stamp << line.text << "\n";
        }
        const std::string invariant = traffic.dispatcher().validate();
        if (!invariant.empty())
        {
            ++invariantViolations;
            log << "VIOLATION t=" << t << " dispatcher: " << invariant << "\n";
        }
        const auto actors = traffic.actors();
        std::vector<TrafficPose> poses;
        for (const auto& a : actors)
            poses.push_back(traffic.pose(a.id, t));
        for (std::size_t i = 0; i < actors.size(); ++i)
        {
            if (!poses[i].present) continue;
            const auto& ci = trafficShipClass(actors[i].kind);
            const std::string s = structureViolation(poses[i], ci, omega * t);
            if (!s.empty())
            {
                ++structureViolations;
                if (lastStructureLog[actors[i].callsign]++ < 3)
                    log << "VIOLATION t=" << t << " " << actors[i].callsign << " (" << actorPhaseName(actors[i].phase)
                        << ") structure: " << s << "\n";
            }
            for (std::size_t j = i + 1; j < actors.size(); ++j)
            {
                if (!poses[j].present) continue;
                const auto& cj = trafficShipClass(actors[j].kind);
                const double gap = glm::length(poses[i].position - poses[j].position) - ci.radius - cj.radius;
                const std::string zone = std::string(actorPhaseName(actors[i].phase)) + "/" + actorPhaseName(actors[j].phase);
                if (gap < minGap)
                {
                    minGap = gap;
                    minGapWhat = actors[i].callsign + " " + actorPhaseName(actors[i].phase) + " vs " + actors[j].callsign +
                                 " " + actorPhaseName(actors[j].phase) + " at t=" + std::to_string(static_cast<int>(t));
                }
                auto it = minGapByZone.find(zone);
                if (it == minGapByZone.end() || gap < it->second)
                    minGapByZone[zone] = gap;
                if (gap < 5.0)
                {
                    ++separationViolations;
                    log << "VIOLATION t=" << t << " separation " << gap << " m: " << actors[i].callsign << " ("
                        << actorPhaseName(actors[i].phase) << ") vs " << actors[j].callsign << " ("
                        << actorPhaseName(actors[j].phase) << ")\n";
                }
            }
        }
        {
            // Waits: a departure from its request to its clearance, an arrival at
            // its hold until entry; the dispatcher's estimate at the start of the
            // wait vs the real wait.
            using game::docking::traffic::DockTrafficPhase;
            for (const auto& st : traffic.dispatcher().snapshot().ships)
            {
                const bool waitingPhase = st.phase == DockTrafficPhase::DepartureQueued || st.phase == DockTrafficPhase::AtHold;
                auto& w = waits[st.shipId];
                if (waitingPhase && !w.active)
                {
                    w.active = true;
                    w.departure = st.phase == DockTrafficPhase::DepartureQueued;
                    w.since = t;
                    w.estimate = st.estimatedWaitSeconds;
                }
                else if (!waitingPhase && w.active)
                {
                    w.active = false;
                    const double real = t - w.since;
                    (w.departure ? maxDepartureWait : maxHoldWait) = std::max(w.departure ? maxDepartureWait : maxHoldWait, real);
                    estimateError += std::abs(real - w.estimate);
                    ++estimateSamples;
                }
            }
        }
        if (t >= nextSnapshot)
        {
            nextSnapshot += 60.0;
            const auto snap = traffic.dispatcher().snapshot();
            log << "SNAPSHOT t=" << t << " arrival_queue=" << snap.arrivalQueueLength
                << " departure_queue=" << snap.departureQueueLength << " holds=";
            for (const auto& [id, owner] : snap.holds) log << id << ":" << (owner ? std::to_string(owner % 1000) : "-") << " ";
            log << "pads=";
            for (const auto& [id, owner] : snap.pads) log << id << ":" << (owner ? std::to_string(owner % 1000) : "-") << " ";
            log << "lanes=";
            for (const auto& [id, owner] : snap.resources) log << id << ":" << (owner ? std::to_string(owner % 1000) : "-") << " ";
            log << "\n";
            // Terminal text of a few ships, as the pilot would read it.
            if (terminalSamples < 40)
                for (const auto& st : snap.ships)
                    if (st.phase == game::docking::traffic::DockTrafficPhase::Queued ||
                        st.phase == game::docking::traffic::DockTrafficPhase::AtHold ||
                        st.phase == game::docking::traffic::DockTrafficPhase::EntryCleared)
                    {
                        log << "TERMINAL ship " << st.shipId % 1000 << ": ";
                        for (const auto& l : game::presentation::dockDispatcherTerminalLines(
                                 game::presentation::dockDispatcherTerminalInput(st)))
                            log << "[" << l << "] ";
                        log << "\n";
                        ++terminalSamples;
                    }
        }
    }
    const auto& stats = traffic.stats();
    char summary[1200];
    std::snprintf(summary, sizeof(summary),
        "SUMMARY hours=%.1f arrivals=%zu departures=%zu shuttle_flights=%zu cobrett_loops=%zu "
        "max_arrival_queue=%zu max_holds_occupied=%zu max_pads_occupied=%zu max_queue_wait_s=%.0f "
        "arrivals_per_hour=%.1f invariant_violations=%zu separation_violations=%zu structure_violations=%zu "
        "min_gap_m=%.1f (%s) max_departure_wait_s=%.0f max_hold_wait_s=%.0f wait_estimate_mean_abs_error_s=%.0f (n=%zu) flow_holds=%zu",
        hours, stats.arrivals, stats.departures, stats.shuttleFlights, stats.cobrettLoops, stats.maxArrivalQueue,
        stats.maxHoldsOccupied, stats.maxPadsOccupied, stats.maxQueueWaitSeconds, stats.arrivals / std::max(hours, 1.0e-9),
        invariantViolations, separationViolations, structureViolations, minGap, minGapWhat.c_str(),
        maxDepartureWait, maxHoldWait, estimateSamples ? estimateError / static_cast<double>(estimateSamples) : 0.0, estimateSamples, stats.flowHolds);
    log << summary << "\n";
    log << "MIN GAP BY PHASE PAIR (m):\n";
    for (const auto& [zone, gap] : minGapByZone)
        log << "  " << zone << " " << gap << "\n";
    std::printf("%s\nlog: %s\n", summary, logPath.c_str());
    return invariantViolations + separationViolations + structureViolations == 0 ? 0 : 1;
}
