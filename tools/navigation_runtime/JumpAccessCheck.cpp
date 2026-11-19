// DEV check: how hard is it to jump from one system to another, by the game's own jump rules (no game target linked).
// For a from -> to pair it prints what the visible + hidden planners use (GameServer RequestJump, GameSimulationJump):
//   the straight distance, the way around the charted masses (planInterstellarRoute), the charge for a ship of the
//   given mass vs the tank capacity, the sump crossings (swarms + approach shells) with their catch probability and the
//   total catch (one draw over the whole way), the same with a fresh lane record through each corridor of the target's
//   shell, and the exit currents (departure 1000 AU + arrival 1000 AU, JumpCurrents.h) before / after the beacons'
//   lock, against the exit body's reach (0.3 Hill: beyond it the ship comes out in open space).
//
//   build-relwithdebinfo/EliteJumpAccessCheck.exe [--t <universe s>] [--mass <t>] [--cap <t>] [--core <t>]
//        <from>:<to> ...          a pair of system ids
//        --near <to> [n]          the n nearest systems with beacons (world sites) -> to
//        --chain <from>:<to>      the cheapest chain of hops <= 32 ly (cost = -ln P(no catch) x 10 + charge t)
//        --list                   the systems (id, name, distance from Sol, beacons)
// Run from the repository root (the atlas and the shells load from src/assets/data).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <tuple>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/InterstellarJumpRoute.h"
#include "src/game/navigation/rules/JumpCurrents.h"
#include "src/game/navigation/rules/JumpRules.h"
#include "src/game/navigation/rules/JumpSumps.h"
#include "src/world/celestial/CelestialEphemerisService.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace jr = game::navigation::jump;
using world::celestial::StarAtlasDatabase;

namespace
{
double g_universeS = 853005600.0;   // 3126-01-01 (the shells' epoch)
double g_massT = 260.0;              // Cobrett-I N1
double g_capT = 3.0;                 // 3 x std.tank.jumpcharge
double g_coreT = 500.0;              // um.core.z05
double g_laneErrDeg = 0.2;          // the lane records' survey error
std::vector<double> g_laneAges {0.0, 2.0, 5.0};
constexpr double StarMuMin = 1.0e18; // a body this heavy is a star for the exit rule (Sun 1.3e20, a brown dwarf ~1e18)

struct Currents
{
    double driftKm = 0.0, arrivalKm = 0.0, departureKm = 0.0, shellKm = 0.0;
    double reachKm = 0.0;     // 0.3 Hill of the exit body (or 1.5 x the orbit), beyond it: open space
    std::string exitBody;
    bool ok = false;
};

// the currents of one system along the line a -> b (system frame, metres)
glm::dvec3 systemDrift(const world::celestial::CelestialSystemDefinition& def, const world::celestial::CelestialEphemerisService& eph,
                       const glm::dvec3& a, const glm::dvec3& b)
{
    std::vector<jr::CurrentMass> masses;
    std::unordered_map<std::string, glm::dvec3> centres;
    for (const auto& body : eph.bodies())
    {
        const auto s = eph.state(body.id, g_universeS);
        if (!s)
            continue;
        centres[body.id] = s->positionMeters();
        if (body.gravitationalParameterM3s2 > 0.0)
            masses.push_back({s->positionMeters(), body.gravitationalParameterM3s2, body.radiusMeters});
    }
    return jr::jumpCurrentDrift(masses, jr::jumpSumpsOfSystem(def), centres, a, b);
}

std::string exitBodyOf(const StarAtlasDatabase& atlas, int systemId)
{
    const auto* system = atlas.findSystem(systemId);
    if (!system)
        return {};
    static world::celestial::WorldSitesCatalog sites;
    static const bool loaded = sites.loadFromRuntimeOrSource();
    std::unordered_map<std::string, int> on;
    if (loaded)
        for (const auto& s : sites.sitesInSystem(systemId))
            ++on[s.parentBodyId];
    std::tuple<int, int, double> best {-1, -1, -1.0};
    std::string id;
    for (const auto& b : system->bodies)
    {
        if (b.type != world::celestial::BodyType::Planet || b.radiusKm < 1000.0)
            continue;
        const auto n = on.find(b.id);
        const std::tuple<int, int, double> key {n == on.end() ? 0 : n->second, b.observed ? 1 : 0, b.massKg};
        if (key > best)
        {
            best = key;
            id = b.id;
        }
    }
    return id;
}

// the departure (1000 AU out along dir from the start system's exit-side point) + the arrival (1000 AU in to the planned
// exit point) - as GameSimulationJump does (the departure from the ship's point; here from the start system's own exit
// body orbit, where a ship waits for the jump)
Currents exitCurrents(const StarAtlasDatabase& atlas, int fromId, int toId, const glm::dvec3& dir, const glm::dvec3& shellDriftM)
{
    Currents out;
    const auto* to = atlas.findSystem(toId);
    if (!to)
        return out;
    const world::celestial::CelestialEphemerisService ephTo(*to);
    out.exitBody = exitBodyOf(atlas, toId);
    const auto* eb = ephTo.body(out.exitBody);
    const auto es = ephTo.state(out.exitBody, g_universeS);
    if (!eb || !es)
        return out;
    glm::dvec3 starPos(0.0);
    double starMu = 0.0;
    for (const auto& b : ephTo.bodies())
        if (b.gravitationalParameterM3s2 > starMu && b.type == world::celestial::BodyType::Star)
            if (const auto s = ephTo.state(b.id, g_universeS))
            {
                starMu = b.gravitationalParameterM3s2;
                starPos = s->positionMeters();
            }
    const glm::dvec3 centre = es->positionMeters();
    const glm::dvec3 toStar = starPos - centre;
    const glm::dvec3 radial = glm::length(toStar) > 1.0 ? glm::normalize(toStar) : glm::dvec3(1, 0, 0);
    const double floor = game::navigation::rules::floorAltitudeM(eb->radiusMeters, eb->atmosphereOuterRadiusMeters);
    const glm::dvec3 planned = centre + radial * (eb->radiusMeters + floor + jr::ExitOrbitAboveFloorM);
    const glm::dvec3 u = glm::normalize(dir);
    glm::dvec3 drift = systemDrift(*to, ephTo, planned - u * (1000.0 * jr::AuM), planned);
    out.arrivalKm = glm::length(drift) / 1000.0;
    if (const auto* from = atlas.findSystem(fromId))
    {
        const world::celestial::CelestialEphemerisService ephFrom(*from);
        const std::string fb = exitBodyOf(atlas, fromId);
        if (const auto fs = ephFrom.state(fb, g_universeS))
        {
            const auto* fbb = ephFrom.body(fb);
            const glm::dvec3 ship = fs->positionMeters() + glm::dvec3(fbb ? fbb->radiusMeters * 1.2 : 1.0e7, 0, 0);
            const glm::dvec3 dep = systemDrift(*from, ephFrom, ship, ship + u * (1000.0 * jr::AuM));
            out.departureKm = glm::length(dep) / 1000.0;
            drift += dep;
        }
    }
    out.shellKm = glm::length(shellDriftM) / 1000.0;
    drift += shellDriftM;
    out.driftKm = glm::length(drift) / 1000.0;
    const double hill = glm::length(toStar) * std::cbrt(eb->gravitationalParameterM3s2 / std::max(1.0, 3.0 * starMu));
    const double rMin = eb->radiusMeters + floor + jr::ExitOrbitAboveFloorM;
    out.reachKm = std::max(rMin * 1.5, 0.3 * hill) / 1000.0;
    out.ok = true;
    return out;
}

double totalCatch(const std::vector<jr::InterstellarSumpCrossing>& cs)
{
    double surv = 1.0;
    for (const auto& c : cs)
        surv *= 1.0 - c.probability;
    return 1.0 - surv;
}

std::string nameOf(const StarAtlasDatabase& atlas, int id)
{
    const auto* s = atlas.findSystemSummary(id);
    return s ? s->name : std::string("?");
}

struct PairResult
{
    double straightLy = 0.0, pathLy = 0.0, chargeT = 0.0, catchP = 0.0, lock = 1.0;
    bool ok = false;
};

PairResult evaluate(const StarAtlasDatabase& atlas, int fromId, int toId, bool print)
{
    PairResult r;
    const auto* from = atlas.findSystemSummary(fromId);
    const auto* to = atlas.findSystemSummary(toId);
    if (!from || !to || fromId == toId)
    {
        if (print)
            std::printf("  %d -> %d: unknown system\n", fromId, toId);
        return r;
    }
    r.straightLy = glm::length(to->positionLy - from->positionLy);
    const glm::dvec3 startLy = from->positionLy;
    auto route = jr::planInterstellarRoute(atlas, fromId, toId, &startLy);
    r.pathLy = route.ok ? route.lengthLy : r.straightLy;
    if (route.pointsLy.size() < 2)
        r.pathLy = r.straightLy;
    const bool bFrom = jr::systemHasBeacons(fromId), bTo = jr::systemHasBeacons(toId);
    r.lock = jr::exitLockFactor(bFrom, bTo);
    r.chargeT = jr::chargeT(r.pathLy, g_massT);
    const auto refusal = jr::check(r.pathLy, g_massT, g_coreT, g_capT);
    const auto crossings = jr::interstellarSumpCrossings(atlas, route, r.lock, g_universeS);
    r.catchP = totalCatch(crossings);
    r.ok = refusal == jr::Refusal::None;
    if (!print)
        return r;
    std::printf("== %s (%d) -> %s (%d)\n", from->name.c_str(), fromId, to->name.c_str(), toId);
    std::printf("  distance %.2f ly, way %.2f ly, %zu correction(s)%s", r.straightLy, r.pathLy,
                route.pointsLy.size() >= 2 ? route.pointsLy.size() - 2 : 0, route.ok ? "" : " (NO WAY AROUND FOUND)");
    for (const auto& n : route.avoidedNames)
        std::printf(" [%s]", n.c_str());
    std::printf("\n  beacons: from %s, to %s -> lock x%.0f\n", bFrom ? "yes" : "no", bTo ? "yes" : "no", r.lock);
    std::printf("  charge %.3f t of %.1f t (ship %.0f t, core %.0f t), charge-up %.0f s, check: %s\n", r.chargeT, g_capT, g_massT,
                g_coreT, jr::chargeUpSeconds(r.pathLy), refusal == jr::Refusal::None ? "OK" : jr::refusalText(refusal));
    for (const auto& c : crossings)
        std::printf("  crosses %s %s: depth %.3f ly (tau %.2f), catch %.2f %%\n", c.shell ? "SHELL" : "swarm", c.name.c_str(), c.depth,
                    c.depth / jr::InterstellarSumpScaleLy, 100.0 * c.probability);
    std::printf("  catch on the way (one draw): %.2f %%  -> arrival %.2f %%\n", 100.0 * r.catchP, 100.0 * (1.0 - r.catchP));
    // the target's shell: a fresh lane record along each corridor
    if (const auto* shell = jr::approachShellOf(atlas, toId))
    {
        std::printf("  target shell %s: layer %.0f/%.0f/%.0f AU, tau %.2f, %zu corridor(s)\n", shell->name.c_str(),
                    shell->innerLy * jr::MetersPerLy / jr::AuM, shell->peakLy * jr::MetersPerLy / jr::AuM,
                    shell->outerLy * jr::MetersPerLy / jr::AuM, shell->tau, shell->corridors.size());
        for (std::size_t k = 0; k < shell->corridors.size(); ++k)
            for (const double ageYears : g_laneAges)
            {
                jr::LaneRecord rec;
                rec.systemId = toId;
                rec.corridor = static_cast<int>(k + 1);
                rec.surveyUniverseS = g_universeS - ageYears * 365.25 * 86400.0;
                // the surveyed direction: the corridor then, off by the survey error toward a fixed side
                const glm::dvec3 truth = jr::corridorDirAt(*shell, shell->corridors[k], rec.surveyUniverseS);
                glm::dvec3 side = glm::cross(truth, glm::dvec3(0.0, 1.0, 0.0));
                side = glm::length(side) > 1e-9 ? glm::normalize(side) : glm::dvec3(1.0, 0.0, 0.0);
                const double e = g_laneErrDeg * 3.14159265358979323846 / 180.0;
                rec.dir = glm::normalize(truth * std::cos(e) + side * std::sin(e));
                rec.errorDeg = g_laneErrDeg;
                rec.source = "check";
                auto laned = jr::planInterstellarRoute(atlas, fromId, toId, &startLy);
                const auto uses = jr::applyLaneRecords(atlas, laned, fromId, toId, {rec}, g_universeS);
                const auto lc = jr::interstellarSumpCrossings(atlas, laned, r.lock, g_universeS);
                const double shellKm = glm::length(jr::interstellarShellDriftM(atlas, laned, g_universeS)) / 1000.0;
                std::printf("    lane %-14s age %5.2f y (err %.2f deg): %-9s way %.2f ly, charge %.3f t, expected %5.1f %%, real catch %6.2f %%, shell currents %.0f km (/lock %.0f km)\n",
                            shell->corridors[k].name.c_str(), ageYears, g_laneErrDeg, uses.empty() ? "unusable" : jr::laneStateText(uses[0].state),
                            laned.lengthLy, jr::chargeT(laned.lengthLy, g_massT),
                            100.0 * jr::laneExpectedRisk(*shell, rec, g_universeS, r.lock), 100.0 * totalCatch(lc), shellKm, shellKm / r.lock);
            }
    }
    glm::dvec3 dir = to->positionLy - from->positionLy;
    if (route.ok && route.pointsLy.size() > 2)
        dir = route.pointsLy[1] - route.pointsLy[0];
    const auto cur = exitCurrents(atlas, fromId, toId, dir, jr::interstellarShellDriftM(atlas, route, g_universeS));
    if (cur.ok)
        std::printf("  exit at %s (dumb jump): currents drift %.0f km (departure %.0f, arrival %.0f, shell %.0f), after the lock %.0f km; reach 0.3 Hill %.0f km -> %s\n", cur.exitBody.c_str(),
                    cur.driftKm, cur.departureKm, cur.arrivalKm, cur.shellKm, cur.driftKm / r.lock, cur.reachKm,
                    cur.driftKm / r.lock > cur.reachKm ? "OPEN SPACE (carried off the planet's reach)" : "on the planet's orbit");
    return r;
}

void chain(const StarAtlasDatabase& atlas, int fromId, int toId)
{
    std::vector<const world::celestial::StarSystemSummary*> all;
    for (const auto& s : atlas.systems())
        all.push_back(&s);
    for (const auto& s : atlas.distantSystems())
        all.push_back(&s);
    // the deep-space places (interstellar swarms): a stop in open space - a recharge from carried tanks, no beacons
    for (const auto& s : atlas.deepSpacePlaces())
        all.push_back(&s);
    std::unordered_map<int, double> best;
    std::unordered_map<int, int> prev;
    using Q = std::pair<double, int>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> q;
    best[fromId] = 0.0;
    q.push({0.0, fromId});
    while (!q.empty())
    {
        const auto [cost, id] = q.top();
        q.pop();
        if (cost > best[id] + 1e-12)
            continue;
        if (id == toId)
            break;
        const auto* a = atlas.findSystemSummary(id);
        for (const auto* s : all)
        {
            if (s->id == id || glm::length(s->positionLy - a->positionLy) > jr::MaxHopLy)
                continue;
            const auto r = evaluate(atlas, id, s->id, false);
            if (!r.ok)
                continue;
            const double c = cost - 10.0 * std::log(std::max(1e-12, 1.0 - r.catchP)) + r.chargeT;
            if (!best.count(s->id) || c < best[s->id])
            {
                best[s->id] = c;
                prev[s->id] = id;
                q.push({c, s->id});
            }
        }
    }
    if (!best.count(toId))
    {
        std::printf("== chain %s -> %s: NO chain of hops <= %.0f ly\n", nameOf(atlas, fromId).c_str(), nameOf(atlas, toId).c_str(), jr::MaxHopLy);
        return;
    }
    std::vector<int> path {toId};
    while (path.back() != fromId)
        path.push_back(prev[path.back()]);
    std::reverse(path.begin(), path.end());
    std::printf("== chain %s -> %s: %zu hop(s)\n", nameOf(atlas, fromId).c_str(), nameOf(atlas, toId).c_str(), path.size() - 1);
    double surv = 1.0, charge = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const auto r = evaluate(atlas, path[i - 1], path[i], false);
        surv *= 1.0 - r.catchP;
        charge += r.chargeT;
        std::printf("  hop %zu: %s -> %s (%d)  %.2f ly, charge %.3f t, lock x%.0f, catch %.2f %%\n", i, nameOf(atlas, path[i - 1]).c_str(),
                    nameOf(atlas, path[i]).c_str(), path[i], r.pathLy, r.chargeT, r.lock, 100.0 * r.catchP);
    }
    std::printf("  total charge %.3f t (tanks %.1f t: %d refuel stop(s) at least), arrival without a catch %.2f %%\n", charge, g_capT,
                static_cast<int>(std::ceil(charge / g_capT)) - 1, 100.0 * surv);
}
} // namespace

// the approach-shell currents rule + the Fortuna closure data (docs/FORTUNA_LEXIE.md); exit code 0 = all pass
int selfTest(const StarAtlasDatabase& atlas)
{
    int failed = 0;
    const auto expect = [&](bool ok, const std::string& what) {
        std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok)
            ++failed;
    };
    std::printf("== selftest (t = %.0f s)\n", g_universeS);
    const auto* sol = atlas.findSystemSummary(0);
    const glm::dvec3 solLy = sol ? sol->positionLy : glm::dvec3(0.0);
    // 1. a shell without currents adds no drift (the rule stays narrow)
    {
        const auto route = jr::planInterstellarRoute(atlas, 45, 41);
        const double km = glm::length(jr::interstellarShellDriftM(atlas, route, g_universeS)) / 1000.0;
        expect(km == 0.0, "83 Leonis -> HD 104067 (shell without currents): shell drift " + std::to_string(km) + " km == 0");
    }
    const auto* fortuna = jr::approachShellOf(atlas, 61);
    expect(fortuna != nullptr && fortuna->driftAuPerTau > 0.0, "Fortuna shell has currents (driftAuPerTau > 0)");
    if (!fortuna)
        return 1;
    // 2. a dumb jump Sol -> Fortuna: caught almost surely; a survivor is carried off Lexie's reach
    {
        const auto route = jr::planInterstellarRoute(atlas, 0, 61, &solLy);
        const double lock = jr::exitLockFactor(jr::systemHasBeacons(0), jr::systemHasBeacons(61));
        const double p = totalCatch(jr::interstellarSumpCrossings(atlas, route, lock, g_universeS));
        expect(p >= 0.95, "Sol -> Fortuna dumb: catch " + std::to_string(100.0 * p) + " % >= 95 %");
        const glm::dvec3 shell = jr::interstellarShellDriftM(atlas, route, g_universeS);
        const auto cur = exitCurrents(atlas, 0, 61, glm::dvec3(atlas.findSystemSummary(61)->positionLy - solLy), shell);
        expect(cur.ok && cur.driftKm / lock > cur.reachKm, "Sol -> Fortuna dumb survivor: residual " + std::to_string(cur.driftKm / lock) +
                                                              " km > Lexie reach " + std::to_string(cur.reachKm) + " km (open space)");
    }
    // 3. the surviving lane: a fresh, precise record gets through to Lexie's orbit most of the time
    {
        jr::LaneRecord rec;
        rec.systemId = 61;
        rec.corridor = 1;
        rec.surveyUniverseS = g_universeS;
        rec.dir = jr::corridorDirAt(*fortuna, fortuna->corridors[0], g_universeS);
        rec.errorDeg = 0.02;
        rec.source = "selftest";
        auto route = jr::planInterstellarRoute(atlas, 0, 61, &solLy);
        const auto uses = jr::applyLaneRecords(atlas, route, 0, 61, {rec}, g_universeS);
        const double lock = jr::exitLockFactor(jr::systemHasBeacons(0), jr::systemHasBeacons(61));
        const double p = totalCatch(jr::interstellarSumpCrossings(atlas, route, lock, g_universeS));
        expect(!uses.empty(), "Fortuna-1 fresh record (0.02 deg) is usable");
        expect(p < 0.10, "Fortuna-1 fresh record: catch " + std::to_string(100.0 * p) + " % < 10 %");
        const glm::dvec3 dir = route.pointsLy.size() > 2 ? route.pointsLy[1] - route.pointsLy[0] : glm::dvec3(atlas.findSystemSummary(61)->positionLy - solLy);
        const auto cur = exitCurrents(atlas, 0, 61, dir, jr::interstellarShellDriftM(atlas, route, g_universeS));
        expect(cur.ok && cur.driftKm / lock < cur.reachKm, "Fortuna-1 fresh record: residual " + std::to_string(cur.driftKm / lock) +
                                                               " km < Lexie reach " + std::to_string(cur.reachKm) + " km (on the orbit)");
        // a month-old record of the same survey is already doubtful
        rec.surveyUniverseS = g_universeS - 0.25 * 365.25 * 86400.0;
        rec.dir = jr::corridorDirAt(*fortuna, fortuna->corridors[0], rec.surveyUniverseS);
        expect(jr::laneState(*fortuna, rec, g_universeS) != jr::LaneState::Known, "Fortuna-1 record 3 months old is no longer 'known'");
    }
    // 4. the collapsed corridors: even a precise military record (0.1 deg) is useless
    for (std::size_t k = 1; k < fortuna->corridors.size(); ++k)
    {
        jr::LaneRecord rec;
        rec.systemId = 61;
        rec.corridor = static_cast<int>(k + 1);
        rec.surveyUniverseS = g_universeS;
        rec.dir = jr::corridorDirAt(*fortuna, fortuna->corridors[k], g_universeS);
        rec.errorDeg = 0.1;
        expect(jr::laneState(*fortuna, rec, g_universeS) == jr::LaneState::Unknown,
               fortuna->corridors[k].name + " (collapsed): a fresh 0.1 deg record is useless");
    }
    std::printf("selftest: %s (%d failed)\n", failed == 0 ? "PASS" : "FAIL", failed);
    return failed == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    StarAtlasDatabase atlas;
    if (!atlas.loadFromRuntimeOrSource())
    {
        std::cerr << "star atlas did not load (run from the repository root)" << std::endl;
        return 2;
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--t" && i + 1 < argc)
            g_universeS = std::atof(argv[++i]);
        else if (a == "--mass" && i + 1 < argc)
            g_massT = std::atof(argv[++i]);
        else if (a == "--cap" && i + 1 < argc)
            g_capT = std::atof(argv[++i]);
        else if (a == "--core" && i + 1 < argc)
            g_coreT = std::atof(argv[++i]);
        else if (a == "--lane-err" && i + 1 < argc)
            g_laneErrDeg = std::atof(argv[++i]);
        else if (a == "--lane-ages" && i + 1 < argc)
        {
            // comma-separated years, e.g. 0,0.02,0.1
            g_laneAges.clear();
            std::string list = argv[++i];
            for (std::size_t p = 0; p < list.size();)
            {
                const std::size_t q = list.find(',', p);
                g_laneAges.push_back(std::atof(list.substr(p, q == std::string::npos ? std::string::npos : q - p).c_str()));
                p = q == std::string::npos ? list.size() : q + 1;
            }
        }
        else if (a == "--selftest")
            return selfTest(atlas);
        else if (a == "--list")
        {
            const auto* sol = atlas.findSystemSummary(0);
            const auto show = [&](const std::vector<world::celestial::StarSystemSummary>& v, const char* scope) {
                for (const auto& s : v)
                    std::printf("%4d %-28s %-7s %7.2f ly from Sol  beacons %s\n", s.id, s.name.c_str(), scope,
                                sol ? glm::length(s.positionLy - sol->positionLy) : 0.0, jr::systemHasBeacons(s.id) ? "yes" : "no");
            };
            show(atlas.systems(), "local");
            show(atlas.distantSystems(), "distant");
        }
        else if (a == "--near" && i + 1 < argc)
        {
            const int to = std::atoi(argv[++i]);
            int n = 4;
            if (i + 1 < argc && std::isdigit(static_cast<unsigned char>(argv[i + 1][0])))
                n = std::atoi(argv[++i]);
            const auto* t = atlas.findSystemSummary(to);
            if (!t)
                continue;
            std::vector<std::pair<double, int>> near;
            for (const auto* v : {&atlas.systems(), &atlas.distantSystems()})
                for (const auto& s : *v)
                    if (s.id != to && jr::systemHasBeacons(s.id))
                        near.push_back({glm::length(s.positionLy - t->positionLy), s.id});
            std::sort(near.begin(), near.end());
            for (int k = 0; k < n && k < static_cast<int>(near.size()); ++k)
                evaluate(atlas, near[k].second, to, true);
        }
        else if (a == "--chain" && i + 1 < argc)
        {
            int f = -1, t = -1;
            if (std::sscanf(argv[++i], "%d:%d", &f, &t) == 2)
                chain(atlas, f, t);
        }
        else
        {
            int f = -1, t = -1;
            if (std::sscanf(a.c_str(), "%d:%d", &f, &t) == 2)
                evaluate(atlas, f, t, true);
            else
                std::cerr << "unknown argument " << a << std::endl;
        }
    }
    return 0;
}
