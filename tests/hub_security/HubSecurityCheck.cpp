// Unit checks of hub security / traffic control (docs/HUB_SECURITY.md): wrong-way detection in a lane (hysteresis,
// warning -> fine -> periodic / per-km, repeat escalation, the incident cap), the fine banking path (paid / booked as a
// debt, never refused), the CPA + Newtonian evasion math (a head-on that clears, one that cannot), the air-traffic slot
// track a promoted ship flies (identical to the presentation), the defence layout and the stray bodies. Run from the
// repository root (reads the earth_orbital_hub lane network + plan).
//
//   EliteHubSecurityCheck   -> "HUB SECURITY CHECK: N passed, 0 failed", exit code = failures
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "src/game/economy/BankingModel.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/traffic/HubAirTraffic.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/traffic/HubSecurity.h"

namespace sec = game::traffic::security;
namespace tr = game::navigation::traffic;

namespace
{
int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what)
{
    (ok ? g_pass : g_fail)++;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof(b), "%.2f", v);
    return b;
}

void testWrongWay(const tr::HubLaneNetwork& lanes)
{
    const auto& lane = lanes.lanes().front();
    const glm::dvec3 a = lane.stationsLocal[1], b = lane.stationsLocal[2];
    const glm::dvec3 dir = glm::normalize(b - a);
    const glm::dvec3 mid = (a + b) * 0.5;
    sec::LaneProbe probe;
    check(sec::probeLane(lanes, mid + glm::dvec3(0.0, lane.radiusMeters * 0.5, 0.0), 10.0, probe) && probe.lane == 0,
          "probe: a point inside lane 0 finds it");
    check(!sec::probeLane(lanes, mid + glm::dvec3(0.0, 5000.0, 0.0), 10.0, probe), "probe: 5 km above the lane - none");

    // with the traffic, a perpendicular crossing, a slow drift: no incident
    {
        sec::WrongWayWatch w;
        bool any = false;
        for (int i = 0; i < 40; ++i)
            any |= sec::stepWrongWay(w, true, 0, dir, dir * 120.0, 0.25, 0).event != sec::WrongWayEvent::None;
        const glm::dvec3 side = glm::normalize(glm::cross(dir, glm::dvec3(0.0, 1.0, 0.0)));
        for (int i = 0; i < 40; ++i)
            any |= sec::stepWrongWay(w, true, 0, dir, side * 120.0, 0.25, 0).event != sec::WrongWayEvent::None;
        for (int i = 0; i < 40; ++i)
            any |= sec::stepWrongWay(w, true, 0, dir, -dir * 10.0, 0.25, 0).event != sec::WrongWayEvent::None;
        check(!any, "with the traffic / crossing / 10 m/s drift: no incident");
    }
    // against the traffic at 150 m/s for 30 s, then out
    {
        sec::WrongWayWatch w;
        double started = -1.0, firstFine = -1.0, total = 0.0;
        int fines = 0;
        double t = 0.0;
        for (int i = 0; i < 120; ++i, t += 0.25)
        {
            const auto r = sec::stepWrongWay(w, true, 0, dir, -dir * 150.0, 0.25, 0);
            if (r.event == sec::WrongWayEvent::Started)
                started = t;
            if (r.event == sec::WrongWayEvent::Fine)
            {
                if (firstFine < 0.0)
                    firstFine = t;
                total += r.fineCr;
                ++fines;
            }
        }
        check(std::abs(started - 0.75) < 0.01, "confirmed after 1 s against the lane (" + num(started) + " s)");
        check(firstFine > started + 4.5 && firstFine < started + 5.5, "first fine 5 s after the warning (" + num(firstFine) + " s)");
        // 30 s - 1 s confirm = 29 s: first 1500 + periodic at 15, 25 s (2 x 750) + 4 km (4 x 300 at 150 m/s) = 4200
        check(std::abs(total - 4200.0) < 1.0, "30 s / 4.35 km against the lane: 1500 + 2x750 + 4x300 = 4200 cr (" + num(total) + ")");
        bool ended = false;
        for (int i = 0; i < 12 && !ended; ++i)
            ended = sec::stepWrongWay(w, false, -1, dir, -dir * 150.0, 0.25, 0).event == sec::WrongWayEvent::Ended;
        check(ended && !w.active, "out of the lane 2 s: the incident closes");
        // hysteresis: a ship turning to cos -0.35 stays in the incident (exit only above -0.2)
        sec::WrongWayWatch h;
        for (int i = 0; i < 8; ++i)
            sec::stepWrongWay(h, true, 0, dir, -dir * 150.0, 0.25, 0);
        const glm::dvec3 side = glm::normalize(glm::cross(dir, glm::dvec3(0.0, 1.0, 0.0)));
        const glm::dvec3 v = (-0.35 * dir + std::sqrt(1.0 - 0.35 * 0.35) * side) * 150.0;
        for (int i = 0; i < 20; ++i)
            sec::stepWrongWay(h, true, 0, dir, v, 0.25, 0);
        check(h.active, "hysteresis: cos -0.35 keeps an open incident open");
    }
    // repeat escalation and the cap
    {
        sec::WrongWayWatch w;
        double total = 0.0, first = 0.0;
        for (int i = 0; i < 4000; ++i)
        {
            const auto r = sec::stepWrongWay(w, true, 0, dir, -dir * 150.0, 0.25, 3);
            if (r.event == sec::WrongWayEvent::Fine && first == 0.0)
                first = r.fineCr;
            total += r.fineCr;
        }
        check(std::abs(first - 1500.0 * 3.375) < 1.0, "3 earlier incidents: x1.5^3 = 5062 cr first fine (" + num(first) + ")");
        check(std::abs(total - 15000.0) < 1.0, "one incident never exceeds 15 000 cr (" + num(total) + ")");
    }
}

void testFineBanking()
{
    using namespace game::economy;
    BankDirectory d;
    NationalBank sol;
    sol.id = "sol";
    sol.name = "Sol Treasury";
    sol.code = "CR";
    sol.bloc = "sol";
    sol.reserve = true;
    sol.loanRate = 0.062;
    sol.overdraftRate = 0.122;
    d.add(sol);
    d.rules.startAccountCr = 3000.0;
    d.rules.startCashCr = 0.0;
    d.rules.startOverdraftCr = 1000.0;
    ClientLedger l = openLedger(d, 1, "sol", 0);
    auto r = payFine(l, d, "sol", 1500.0, 0, "lane");
    check(r.ok && r.lifelineCr == 0.0 && std::abs(toUnits(findAccount(l, "sol")->balance) - 1500.0) < 0.01, "fine paid from the account");
    r = payFine(l, d, "sol", 2000.0, 0, "lane");
    check(r.ok && r.lifelineCr == 0.0 && std::abs(toUnits(findAccount(l, "sol")->balance) + 500.0) < 0.01,
          "fine paid into the arranged overdraft (balance " + num(toUnits(findAccount(l, "sol")->balance)) + ")");
    const std::size_t loans = l.loans.size();
    r = payFine(l, d, "sol", 15000.0, 0, "lane");
    check(r.ok && std::abs(r.lifelineCr - 15000.0) < 0.01 && l.loans.size() == loans + 1 && l.loans.back().annualRate == 0.0,
          "unpayable fine: never refused, booked as a 0 % fine debt");
    check(std::abs(toUnits(findAccount(l, "sol")->balance) + 500.0) < 0.01, "... the account is not touched by the debt");
    double fuelOk = pay(l, d, "sol", 200.0, Purpose::Fuel, 1, true, "fuel").ok ? 1.0 : 0.0;
    check(fuelOk == 1.0, "fuel is still payable after a fine debt (lifeline)");
    for (int day = 0; day < 400; ++day)
        stepLedgerDay(l, d, day);
    check(true, "a year of daily steps with the fine debt runs (standing " + std::to_string(static_cast<int>(l.standing)) + ")");
}

void testEvasion()
{
    // a 56 m freighter in a lane at 105 m/s, a ship head-on at 150 m/s, 5 km out (the promotion radius)
    const sec::EvasionCaps caps = sec::evasionCapsForLength(56.0);
    check(caps.lateralAccelMps2 > 0.6 && caps.lateralAccelMps2 < 0.7, "56 m hull RCS " + num(caps.lateralAccelMps2) + " m/s2");
    const sec::Cpa c = sec::closestApproach(glm::dvec3(0.0, 0.0, -5000.0), glm::dvec3(0.0, 0.0, 255.0));
    check(std::abs(c.timeSeconds - 5000.0 / 255.0) < 1.0e-6 && c.distanceMeters < 1.0e-6, "CPA head-on: t = 19.6 s, d = 0");
    const sec::Cpa m = sec::closestApproach(glm::dvec3(100.0, 0.0, -1000.0), glm::dvec3(0.0, 0.0, 100.0));
    check(std::abs(m.timeSeconds - 10.0) < 1.0e-9 && std::abs(m.distanceMeters - 100.0) < 1.0e-9, "CPA offset 100 m: t = 10 s, d = 100 m");

    const auto run = [&](double startMeters, double& minDist, bool& cannot) {
        glm::dvec3 nominal(0.0), nominalVel(0.0, 0.0, -105.0);   // nose -Z
        glm::dvec3 threat(0.0, 0.0, -startMeters), threatVel(0.0, 0.0, 150.0);
        sec::EvasionOffset off;
        minDist = 1.0e30;
        cannot = false;
        for (double t = 0.0; t < 80.0; t += 0.02)
        {
            const glm::dvec3 self = nominal + off.offset;
            const glm::dvec3 selfVel = nominalVel + off.velocity;
            const auto plan = sec::planEvasion(self, selfVel, glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 1.0, 0.0), threat, threatVel, 20.0, caps);
            if (t == 0.0)
                cannot = plan.cannotClear;
            sec::stepEvasionOffset(off, plan, caps, 0.02);
            nominal += nominalVel * 0.02;
            threat += threatVel * 0.02;
            minDist = std::min(minDist, glm::length(threat - (nominal + off.offset)));
        }
        return off;
    };
    double minDist = 0.0;
    bool cannot = false;
    const auto off = run(5000.0, minDist, cannot);
    check(!cannot && minDist > 28.0 + 20.0 + 30.0, "head-on from 5 km: the freighter clears, miss " + num(minDist) + " m");
    check(glm::length(off.offset) < 5.0, "... and is back near its track 60 s after the pass (offset " + num(glm::length(off.offset)) + " m)");
    double minNear = 0.0;
    bool cannotNear = false;
    run(600.0, minNear, cannotNear);
    check(cannotNear && minNear < 48.0, "head-on from 600 m: cannot clear (honest: miss " + num(minNear) + " m < 48 m hulls)");
    // starboard rule: nose -Z, up +Y -> starboard +X
    const auto p = sec::planEvasion(glm::dvec3(0.0), glm::dvec3(0.0, 0.0, -105.0), glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 1.0, 0.0),
                                    glm::dvec3(0.0, 0.0, -3000.0), glm::dvec3(0.0, 0.0, 150.0), 20.0, caps);
    check(p.burn && p.direction.x > 0.99, "dead head-on: alter to starboard (COLREGS 14), dir x " + num(p.direction.x));
    const auto q = sec::planEvasion(glm::dvec3(0.0), glm::dvec3(0.0, 0.0, -105.0), glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 1.0, 0.0),
                                    glm::dvec3(30.0, 0.0, -3000.0), glm::dvec3(0.0, 0.0, 150.0), 20.0, caps);
    check(q.burn && q.direction.x < -0.99, "threat passing 30 m to starboard: evade to port, away from it");
}

void testAirSlotsAndDefence(const tr::HubLaneNetwork& lanes, const game::navigation::HubPlan& plan)
{
    const auto& layout = tr::hubAirTrafficLayoutCached(plan, lanes);
    check(!layout.slots().empty(), std::to_string(layout.slots().size()) + " air-traffic slots");
    // the track a promoted ship flies (along0 + v dt) is the presentation pose
    double worst = 0.0;
    for (const auto& s : layout.slots())
    {
        tr::HubAirSlotPose a, b, c;
        const double t0 = 51234.5;
        layout.pose(s, t0, a);
        layout.pose(s, t0 + 7.0, b);
        if (b.lap != a.lap)
            continue;
        layout.poseAt(s.lane, a.alongMeters + s.speedMps * 7.0, s.speedMps, c);
        worst = std::max(worst, glm::length(b.position - c.position));
    }
    check(worst < 1.0e-3, "a promoted slot's track = its presentation pose (max diff " + num(worst) + " m)");

    const auto L = sec::buildDefenceLayout(plan, &lanes);
    int lasers = 0, coils = 0, pickets = 0, radars = 0;
    for (const auto& s : L.sites)
    {
        std::printf("      site %u kind %d at (%.0f, %.0f, %.0f)\n", s.id, static_cast<int>(s.kind), s.position.x, s.position.y, s.position.z);
        lasers += s.kind == sec::DefenceKind::PointDefenceLaser;
        coils += s.kind == sec::DefenceKind::Coilgun;
        pickets += s.kind == sec::DefenceKind::Picket;
        radars += s.kind == sec::DefenceKind::RadarMast;
    }
    check(lasers > 0 && coils > 0 && pickets == 8 && radars == 1,
          "defence: " + std::to_string(lasers) + " lasers, " + std::to_string(coils) + " coilguns, " + std::to_string(pickets) +
              " pickets, radar; perimeter " + num(L.perimeterMeters) + " m, engage " + num(L.engageMeters) + " m");
    // strays: deterministic, the inbound ones killed inside the engagement radius
    int inbound = 0, killedInside = 0;
    for (std::uint32_t i = 1000; i < 1100; ++i)
    {
        const auto s = sec::strayBody(L, i);
        const auto s2 = sec::strayBody(L, i);
        if (s.killSeconds != s2.killSeconds || s.start != s2.start)
            check(false, "stray determinism");
        if (!s.inbound)
            continue;
        ++inbound;
        killedInside += glm::length(s.killPoint) <= L.engageMeters + 1.0 && glm::length(s.killPoint) > L.cityRadiusMeters * 0.5 ? 1 : 0;
    }
    check(inbound > 40 && killedInside == inbound,
          std::to_string(inbound) + "/100 strays inbound, all killed inside the engagement radius before the city");
}
} // namespace

int main()
{
    tr::HubLaneNetwork lanes;
    game::navigation::HubPlan plan;
    const bool ok = lanes.load("src/assets/data/navigation/hub_lanes/earth_orbital_hub.json") &&
                    plan.load("src/assets/data/navigation/hub_plans/earth_orbital_hub.json");
    check(ok, "earth_orbital_hub lanes + plan loaded");
    if (ok)
    {
        testWrongWay(lanes);
        testAirSlotsAndDefence(lanes, plan);
    }
    // a RING city's defence stands on its deck (audit 2026-11-27: the strip layout floated in the ring's plane)
    {
        game::navigation::HubPlan ringPlan;
        if (ringPlan.load("src/assets/data/navigation/hub_plans/earth_ring_hub.json") && ringPlan.ring().valid)
        {
            const auto R = sec::buildDefenceLayout(ringPlan, nullptr);
            const auto& ring = ringPlan.ring();
            double worstDeck = 0.0, worstUp = 0.0;
            int deckSites = 0;
            for (const auto& s : R.sites)
            {
                if (s.kind == sec::DefenceKind::Picket)
                    continue;
                ++deckSites;
                // on the deck: at radius R from the axis (+Z), the up toward the axis
                worstDeck = std::max(worstDeck, std::abs(std::hypot(s.position.x, s.position.y) - ring.radius));
                const glm::dvec3 toAxis = glm::normalize(glm::dvec3(-s.position.x, -s.position.y, 0.0));
                worstUp = std::max(worstUp, glm::length(glm::normalize(s.up) - toAxis));
            }
            check(deckSites > 0 && worstDeck < 1.0e-6 && worstUp < 1.0e-9,
                  "ring defence: " + std::to_string(deckSites) + " deck sites on the deck (off " + num(worstDeck) +
                      " m), up to the axis; city radius " + num(R.cityRadiusMeters) + " m (ring R " + num(ring.radius) + ")");
            check(R.cityRadiusMeters < ring.radius + 1000.0, "ring city radius from the ring, not the strip length");
        }
        else
            check(false, "earth_ring_hub plan loaded");
    }
    testFineBanking();
    testEvasion();
    std::printf("HUB SECURITY CHECK: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
