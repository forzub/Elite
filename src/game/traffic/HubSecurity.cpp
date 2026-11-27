#include "src/game/traffic/HubSecurity.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"

namespace game::traffic::security
{
namespace
{
double hash01(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0x1000000u);
}

// FNV-1a: stable across runs and builds (std::hash is not specified)
std::uint32_t seedOf(const std::string& s)
{
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s)
    {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

glm::dvec3 perpendicularPart(const glm::dvec3& v, const glm::dvec3& axisUnit)
{
    return v - axisUnit * glm::dot(v, axisUnit);
}
} // namespace

// ------------------------------------------------------------------------------------------------ lanes, wrong way
bool probeLane(const game::navigation::traffic::HubLaneNetwork& lanes, const glm::dvec3& p, double margin, LaneProbe& out)
{
    double best = std::numeric_limits<double>::max();
    bool found = false;
    for (std::size_t li = 0; li < lanes.lanes().size(); ++li)
    {
        const auto& lane = lanes.lanes()[li];
        const double reach = lane.radiusMeters + margin;
        for (std::size_t k = 0; k + 1 < lane.stationsLocal.size(); ++k)
        {
            const glm::dvec3 a = lane.stationsLocal[k], b = lane.stationsLocal[k + 1];
            const glm::dvec3 ab = b - a;
            const double len2 = glm::dot(ab, ab);
            if (len2 < 1.0e-6)
                continue;
            const double t = std::clamp(glm::dot(p - a, ab) / len2, 0.0, 1.0);
            const glm::dvec3 c = a + ab * t;
            const double d = glm::length(p - c);
            if (d > reach)
                continue;
            const double score = d / std::max(1.0, lane.radiusMeters);
            if (score >= best)
                continue;
            best = score;
            found = true;
            out.lane = static_cast<int>(li);
            out.direction = ab / std::sqrt(len2);
            out.axisPoint = c;
            out.lateralMeters = d;
            out.radiusMeters = lane.radiusMeters;
        }
    }
    return found;
}

WrongWayStep stepWrongWay(WrongWayWatch& w, bool inLane, int lane, const glm::dvec3& laneDir, const glm::dvec3& velocity, double dt,
                          int priorIncidents, const WrongWayParams& p, const FineSchedule& f)
{
    WrongWayStep r;
    const double speed = glm::length(velocity);
    r.cosAgainst = inLane && speed > 1.0e-3 ? glm::dot(velocity / speed, laneDir) : 1.0;
    const bool against = inLane && speed >= p.minSpeedMps && r.cosAgainst < (w.active ? p.exitCos : p.enterCos);
    if (!w.active)
    {
        if (!against)
        {
            w.againstSeconds = 0.0;
            return r;
        }
        w.againstSeconds += dt;
        if (w.againstSeconds < p.confirmSeconds)
            return r;
        const WrongWayWatch fresh;
        w = fresh;
        w.active = true;
        w.lane = lane;
        w.nextPeriodicAt = p.warningToFineSeconds;
        r.event = WrongWayEvent::Started;
        return r;
    }
    if (!against)
    {
        w.clearSeconds += dt;
        if (w.clearSeconds >= p.releaseSeconds)
        {
            w.active = false;
            w.againstSeconds = 0.0;
            r.event = WrongWayEvent::Ended;
        }
        return r;
    }
    w.clearSeconds = 0.0;
    if (lane >= 0)
        w.lane = lane;
    w.againstSeconds += dt;
    w.metersAgainst += speed * dt * std::max(0.0, -r.cosAgainst);
    double due = 0.0;
    if (w.againstSeconds >= w.nextPeriodicAt)
    {
        due += w.finedOnce ? f.periodicFineCr : f.firstFineCr;
        w.finedOnce = true;
        w.nextPeriodicAt += f.periodicSeconds;
    }
    if (w.finedOnce)
    {
        const double km = std::floor(w.metersAgainst / 1000.0);
        if (km > w.lastKmCharged)
        {
            due += (km - w.lastKmCharged) * f.perKmCr;
            w.lastKmCharged = km;
        }
    }
    if (due > 0.0)
    {
        const double factor = std::min(f.repeatCapFactor, std::pow(f.repeatFactor, std::max(0, priorIncidents)));
        due = std::min(due * factor, std::max(0.0, f.incidentCapCr - w.fineChargedCr));
        if (due > 0.0)
        {
            w.fineChargedCr += due;
            r.event = WrongWayEvent::Fine;
            r.fineCr = due;
        }
    }
    return r;
}

// ------------------------------------------------------------------------------------------------ collision avoidance
Cpa closestApproach(const glm::dvec3& r, const glm::dvec3& v)
{
    Cpa c;
    const double vv = glm::dot(v, v);
    c.timeSeconds = vv > 1.0e-9 ? -glm::dot(r, v) / vv : 0.0;
    c.missVector = r + v * c.timeSeconds;
    c.distanceMeters = glm::length(c.missVector);
    return c;
}

EvasionCaps evasionCapsForLength(double lengthMeters)
{
    // estimates scaled from a 30 m hull (RCS 1.2 m/s2, main 12 m/s2, 20 deg/s): the RCS force grows with the hull's
    // area, its mass with the volume -> lateral accel ~ 1/L; turn rate ~ 1/L (moment of inertia ~ L^5, RCS torque ~ L^3).
    // Real references: Shuttle orbiter RCS ~0.14 m/s2 sideways (110 t), Dragon ~0.15 m/s2 (12 t) - the game's ships
    // are exaggerated about x5, as their drives are.
    const double L = std::max(5.0, lengthMeters);
    EvasionCaps c;
    c.lateralAccelMps2 = std::clamp(1.2 * 30.0 / L, 0.25, 2.5);
    c.mainAccelMps2 = std::clamp(12.0 * std::sqrt(30.0 / L), 3.0, 15.0);
    c.turnRateRadPerS = std::clamp(0.35 * 30.0 / L, 0.04, 0.6);
    c.radiusMeters = 0.5 * L;
    return c;
}

EvasionPlan planEvasion(const glm::dvec3& selfPos, const glm::dvec3& selfVel, const glm::dvec3& selfForward, const glm::dvec3& selfUp,
                        const glm::dvec3& threatPos, const glm::dvec3& threatVel, double threatRadius, const EvasionCaps& caps,
                        double horizonSeconds, double marginMeters)
{
    EvasionPlan plan;
    const glm::dvec3 r = threatPos - selfPos;
    const glm::dvec3 v = threatVel - selfVel;
    const Cpa cpa = closestApproach(r, v);
    plan.timeToCpa = cpa.timeSeconds;
    plan.cpaMeters = cpa.distanceMeters;
    plan.requiredMeters = caps.radiusMeters + threatRadius + marginMeters;
    if (!(cpa.timeSeconds > 0.0) || cpa.timeSeconds > horizonSeconds || cpa.distanceMeters >= plan.requiredMeters)
        return plan;
    plan.threat = true;
    const double speed = glm::length(v);
    const glm::dvec3 vHat = speed > 1.0e-6 ? v / speed : glm::dvec3(0.0, 0.0, 1.0);
    // away from where the threat will be at the CPA, across the relative velocity (only that part changes the miss)
    glm::dvec3 away = -perpendicularPart(cpa.missVector, vHat);
    const double missPerp = glm::length(away);
    if (missPerp < 2.0)
    {
        // dead head-on: both alter to starboard (COLREGS rule 14) - the threat, if it follows the rule, goes the other way
        glm::dvec3 starboard = glm::cross(selfForward, selfUp);
        starboard = perpendicularPart(starboard, vHat);
        if (glm::length(starboard) < 1.0e-6)
            starboard = perpendicularPart(glm::dvec3(1.0, 0.0, 0.0), vHat);
        away = starboard;
    }
    plan.direction = glm::normalize(away);
    const double tc = cpa.timeSeconds;
    // the reachable sideways displacement by the CPA: RCS at once, or the main drive after turning to the burn direction
    const double rcs = 0.5 * caps.lateralAccelMps2 * tc * tc;
    const double cosTurn = std::clamp(glm::dot(glm::normalize(selfForward), plan.direction), -1.0, 1.0);
    // either end of the ship can face the burn if the drive is reversible - assume the nose only
    const double rotate = std::acos(cosTurn) / std::max(1.0e-3, caps.turnRateRadPerS);
    const double mainLeft = std::max(0.0, tc - rotate);
    const double main = 0.5 * caps.mainAccelMps2 * mainLeft * mainLeft + 0.5 * caps.lateralAccelMps2 * std::min(tc, rotate) * std::min(tc, rotate);
    plan.usesMainDrive = main > rcs;
    plan.accelMps2 = caps.lateralAccelMps2;
    plan.rotateSeconds = plan.usesMainDrive ? rotate : 0.0;
    plan.predictedMissMeters = missPerp + std::max(rcs, main);
    plan.burn = true;
    plan.cannotClear = plan.predictedMissMeters < caps.radiusMeters + threatRadius;
    return plan;
}

void stepEvasionOffset(EvasionOffset& s, const EvasionPlan& plan, const EvasionCaps& caps, double dt)
{
    glm::dvec3 a(0.0);
    if (plan.burn)
    {
        const bool mainReady = plan.usesMainDrive && s.burnSeconds >= plan.rotateSeconds;
        a = plan.direction * (mainReady ? caps.mainAccelMps2 : caps.lateralAccelMps2);
        s.burnSeconds += dt;
        s.quietSeconds = 0.0;
    }
    else
    {
        s.burnSeconds = 0.0;
        s.quietSeconds += dt;
        if (s.quietSeconds > 2.0)
        {
            // back to the track: a critically damped return within half the RCS acceleration (no drive burn)
            constexpr double Omega = 0.2;
            a = -2.0 * Omega * s.velocity - Omega * Omega * s.offset;
            const double cap = caps.lateralAccelMps2;
            const double m = glm::length(a);
            if (m > cap)
                a *= cap / m;
        }
    }
    s.velocity += a * dt;
    s.offset += s.velocity * dt;
    s.evading = !evasionSettled(s);
}

bool evasionSettled(const EvasionOffset& s)
{
    return glm::length(s.offset) < 0.5 && glm::length(s.velocity) < 0.05;
}

// ------------------------------------------------------------------------------------------------ defence
const char* defenceModelId(DefenceKind kind)
{
    switch (kind)
    {
    case DefenceKind::PointDefenceLaser: return "hubd_pdl_base";
    case DefenceKind::Coilgun: return "hubd_coil_base";
    case DefenceKind::MissileCells: return "hubd_vls";
    case DefenceKind::RadarMast: return "hubd_radar_mast";
    case DefenceKind::Picket: return "hubd_picket";
    default: return "hubd_pdl_base";
    }
}

const char* defenceHeadModelId(DefenceKind kind)
{
    switch (kind)
    {
    case DefenceKind::PointDefenceLaser: return "hubd_pdl_head";
    case DefenceKind::Coilgun: return "hubd_coil_head";
    case DefenceKind::RadarMast: return "hubd_radar_dish";
    case DefenceKind::Picket: return "hubd_pdl_head";
    default: return nullptr;
    }
}

DefenceLayout buildDefenceLayout(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork* lanes)
{
    DefenceLayout L;
    L.hubId = plan.hubId();
    const glm::dvec4 b = plan.bounds();
    const glm::dvec2 centre((b.x + b.z) * 0.5, (b.y + b.w) * 0.5);
    L.cityRadiusMeters = std::max(500.0, 0.5 * std::hypot(b.z - b.x, b.w - b.y));
    const double zone = lanes ? lanes->speedLimitZoneMeters() : 0.0;
    L.perimeterMeters = std::max(L.cityRadiusMeters + 1500.0, zone > 0.0 ? zone : 1.4 * L.cityRadiusMeters);
    L.engageMeters = std::max(L.cityRadiusMeters + 800.0, 0.75 * L.perimeterMeters);
    {
        // the debris field: past the rim on the diagonal (the lanes leave the rim along the avenue axes)
        std::uint32_t h = 2166136261u;
        for (const char ch : L.hubId)
            h = (h ^ static_cast<std::uint8_t>(ch)) * 16777619u;
        L.debrisSeed = h;
        const double ang = 0.7853981 + 1.5707963 * static_cast<double>(h % 4u);
        const double reach = L.cityRadiusMeters + 1500.0;
        L.debrisCentre = glm::dvec3(centre.x + reach * std::cos(ang), 0.0, centre.y + reach * std::sin(ang));
        L.debrisRadiusMeters = 500.0;
        L.debrisCount = 40;
    }
    int services = 0;
    int minI = 1 << 30, maxI = -(1 << 30), minJ = 1 << 30, maxJ = -(1 << 30);
    for (const auto& blk : plan.blocks())
    {
        services += blk.zone == "services" ? 1 : 0;
        minI = std::min(minI, blk.i);
        maxI = std::max(maxI, blk.i);
        minJ = std::min(minJ, blk.j);
        maxJ = std::max(maxJ, blk.j);
    }
    const std::string& id = L.hubId;
    for (const char* k : {"mil", "fort", "naval", "garrison", "bastion", "citadel"})
        if (id.find(k) != std::string::npos)
            L.military = true;
    // a garrison city: a quarter of its blocks are police / services (a civil city has a few)
    L.military = L.military || (services >= 4 && services * 4 >= static_cast<int>(plan.blocks().size()));
    std::uint32_t next = 1;
    const std::uint32_t seed = seedOf(id);
    // the rim ring: the outer corner of every rim block, on its deck (y 0), lasers with a coilgun every third site;
    // military: a second laser on every rim block + missile cells beside each coilgun
    int rimIndex = 0;
    for (const auto& blk : plan.blocks())
    {
        const bool rim = blk.i == minI || blk.i == maxI || blk.j == minJ || blk.j == maxJ;
        if (!rim || blk.zone == "void" || blk.modules == 0)
            continue;
        const int k = rimIndex++;
        glm::dvec2 out = blk.centre - centre;
        out = glm::dvec2(out.x == 0.0 ? 0.0 : (out.x > 0.0 ? 1.0 : -1.0), out.y == 0.0 ? 0.0 : (out.y > 0.0 ? 1.0 : -1.0));
        if (blk.i != minI && blk.i != maxI)
            out.x = 0.0;
        if (blk.j != minJ && blk.j != maxJ)
            out.y = 0.0;
        // the planner's j runs along z, i along x (blocks' centre gives the sign anyway)
        if (out.x == 0.0 && out.y == 0.0)
            out = glm::dvec2(0.0, 1.0);
        const glm::dvec2 p = blk.centre + out * (blk.halfMeters * 0.86);
        DefenceSite s;
        s.id = next++;
        s.kind = (k % 3 == 2) ? DefenceKind::Coilgun : DefenceKind::PointDefenceLaser;
        s.position = glm::dvec3(p.x, 0.0, p.y);
        s.up = glm::dvec3(0.0, 1.0, 0.0);
        s.idlePhase = hash01(seed, s.id) * 6.2831853;
        L.sites.push_back(s);
        if (L.military)
        {
            // a garrison city: a second laser on every rim block (the other corner)
            DefenceSite d = s;
            d.id = next++;
            d.kind = DefenceKind::PointDefenceLaser;
            const glm::dvec2 side(-out.y, out.x);
            d.position = glm::dvec3(p.x - side.x * 150.0, 0.0, p.y - side.y * 150.0);
            d.idlePhase = hash01(seed, d.id) * 6.2831853;
            L.sites.push_back(d);
        }
        if (L.military && s.kind == DefenceKind::Coilgun)
        {
            DefenceSite m = s;
            m.id = next++;
            m.kind = DefenceKind::MissileCells;
            const glm::dvec2 side(-out.y, out.x);
            m.position = glm::dvec3(p.x + side.x * 70.0, 0.0, p.y + side.y * 70.0);
            L.sites.push_back(m);
        }
    }
    // the radar: on the services (police) block nearest the centre, else the centre block
    {
        const game::navigation::HubPlanBlock* best = nullptr;
        double bestD = 1.0e30;
        for (const auto& blk : plan.blocks())
        {
            if (blk.zone == "void")
                continue;
            const double d = glm::length(blk.centre - centre) + (blk.zone == "services" ? 0.0 : 1.0e5);
            if (d < bestD)
            {
                bestD = d;
                best = &blk;
            }
        }
        if (best)
        {
            DefenceSite s;
            s.id = next++;
            s.kind = DefenceKind::RadarMast;
            s.position = glm::dvec3(best->centre.x + best->halfMeters * 0.7, 0.0, best->centre.y - best->halfMeters * 0.7);
            s.up = glm::dvec3(0.0, 1.0, 0.0);
            L.sites.push_back(s);
        }
    }
    // the picket ring: sentry platforms at the perimeter, alternately over and under the plane
    const int pickets = L.military ? 12 : 8;
    for (int i = 0; i < pickets; ++i)
    {
        const double ang = 6.2831853 * (i + 0.5 * hash01(seed + 5u, static_cast<std::uint32_t>(i)) * 0.2) / pickets;
        DefenceSite s;
        s.id = next++;
        s.kind = DefenceKind::Picket;
        const double r = L.perimeterMeters * 0.92;
        s.position = glm::dvec3(centre.x + r * std::cos(ang), (i % 2 == 0 ? 350.0 : -350.0), centre.y + r * std::sin(ang));
        s.up = glm::dvec3(0.0, i % 2 == 0 ? 1.0 : -1.0, 0.0);
        s.idlePhase = hash01(seed, s.id) * 6.2831853;
        L.sites.push_back(s);
    }
    // a RING city (audit 2026-11-27, A1 class): everything above was laid out in the plan's STRIP metres, the consumers
    // (GameSimulationHubSecurity, HubDefencePresentation) take the sites in the MODULE frame - on a ring the turrets and
    // the radar floated in the ring's plane. The deck sites bend onto the deck (HubRingBend); the city's size, the
    // perimeter, the pickets and the debris field come from the ring itself (radius R round the module +Z, its width).
    if (const auto& ring = plan.ring(); ring.valid)
    {
        const double halfWidth = 0.5 * std::abs(b.w - b.y);
        L.cityRadiusMeters = std::hypot(ring.radius, halfWidth);
        L.perimeterMeters = std::max(L.cityRadiusMeters + 1500.0, zone > 0.0 ? zone : 1.4 * L.cityRadiusMeters);
        L.engageMeters = std::max(L.cityRadiusMeters + 800.0, 0.75 * L.perimeterMeters);
        const double reach = L.cityRadiusMeters + 1500.0;
        const double debrisAng = 0.7853981 + 1.5707963 * static_cast<double>(L.debrisSeed % 4u);
        L.debrisCentre = glm::dvec3(reach * std::cos(debrisAng), reach * std::sin(debrisAng), 0.0);
        int picket = 0;
        const int pickets = L.military ? 12 : 8;
        for (auto& site : L.sites)
        {
            if (site.kind == DefenceKind::Picket)
            {
                // round the axis outside the rim, alternately on either side of the ring's plane (its +-Z)
                const int i = picket++;
                const double ang = 6.2831853 * (i + 0.5 * hash01(seed + 5u, static_cast<std::uint32_t>(i)) * 0.2) / pickets;
                const double r = L.perimeterMeters * 0.92;
                const double side = i % 2 == 0 ? 1.0 : -1.0;
                site.position = glm::dvec3(r * std::cos(ang), r * std::sin(ang), side * (halfWidth + 350.0));
                site.up = glm::dvec3(0.0, 0.0, side);
                continue;
            }
            const glm::dvec3 planPoint = site.position;
            site.position = ring.toFrame(planPoint);
            site.up = ring.upAt(planPoint);
        }
    }
    return L;
}

double strayPeriodSeconds(const DefenceLayout& layout)
{
    // a belt / ring hub sees more strays (author: «уничтожение непрошенных астероидов»); the rest the odd debris
    for (const char* k : {"belt", "ring", "ceres", "vesta", "pallas", "asteroid", "trojan"})
        if (layout.hubId.find(k) != std::string::npos)
            return 35.0;
    return 60.0;
}

StrayBody strayBody(const DefenceLayout& L, std::uint32_t index)
{
    StrayBody s;
    s.index = index;
    const std::uint32_t seed = seedOf(L.hubId) ^ 0xA53Cu;
    const auto h = [&](std::uint32_t k) { return hash01(seed + k * 7919u, index); };
    const double period = strayPeriodSeconds(L);
    s.spawnSeconds = index * period + h(1) * period * 0.5;
    // the radar horizon: 1.5 km beyond the perimeter, a direction biased toward the city plane
    const double az = h(2) * 6.2831853;
    const double el = (h(3) - 0.5) * 1.0;   // +-0.5 rad
    const glm::dvec3 dir(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
    s.start = dir * (L.perimeterMeters + 1500.0);
    // size: many small, few big (0.3 .. 4 m, power law)
    s.sizeMeters = 0.3 + 3.7 * std::pow(h(4), 3.0);
    // a drift of 30..80 m/s relative to the hub (a belt hub's neighbours: orbital speed differences of tens of m/s)
    const double speed = 30.0 + 50.0 * h(5);
    s.inbound = h(6) < 0.65;
    glm::dvec3 aim;
    {
        glm::dvec3 side = glm::cross(dir, glm::dvec3(0.0, 1.0, 0.0));
        side = glm::length(side) > 1.0e-6 ? glm::normalize(side) : glm::dvec3(1.0, 0.0, 0.0);
        const double off = s.inbound ? (h(7) - 0.5) * 1.2 * L.cityRadiusMeters : (h(7) < 0.5 ? -1.0 : 1.0) * (1.5 + h(8)) * L.cityRadiusMeters;
        aim = side * off;
    }
    s.velocity = glm::normalize(aim - s.start) * speed;
    if (!s.inbound)
        return s;
    // when it crosses the engagement radius: |start + v t| = engage (first root)
    const double a = glm::dot(s.velocity, s.velocity);
    const double bq = 2.0 * glm::dot(s.start, s.velocity);
    const double c = glm::dot(s.start, s.start) - L.engageMeters * L.engageMeters;
    const double disc = bq * bq - 4.0 * a * c;
    const double tEngage = disc > 0.0 ? std::max(0.0, (-bq - std::sqrt(disc)) / (2.0 * a)) : 0.0;
    s.fireSeconds = s.spawnSeconds + tEngage;
    const glm::dvec3 at = s.start + s.velocity * tEngage;
    s.laser = s.sizeMeters < 1.5;
    double best = 1.0e30;
    for (std::size_t i = 0; i < L.sites.size(); ++i)
    {
        const auto& site = L.sites[i];
        const bool can = s.laser ? (site.kind == DefenceKind::PointDefenceLaser || site.kind == DefenceKind::Picket) : site.kind == DefenceKind::Coilgun;
        if (!can)
            continue;
        const double d = glm::length(site.position - at);
        if (d < best)
        {
            best = d;
            s.shooter = static_cast<int>(i);
        }
    }
    if (s.shooter < 0)
    {
        // no coilgun: the lasers burn it longer
        s.laser = true;
        for (std::size_t i = 0; i < L.sites.size(); ++i)
            if (L.sites[i].kind == DefenceKind::PointDefenceLaser || L.sites[i].kind == DefenceKind::Picket)
            {
                const double d = glm::length(L.sites[i].position - at);
                if (d < best)
                {
                    best = d;
                    s.shooter = static_cast<int>(i);
                }
            }
    }
    // a laser: ~2 MW on a rock spalls / vaporises it in seconds (1.2 s + 0.8 s per metre); a coilgun slug at 3 km/s
    s.killSeconds = s.laser ? s.fireSeconds + 1.2 + 0.8 * s.sizeMeters : s.fireSeconds + best / 3000.0 + 0.3;
    s.killPoint = strayPosition(s, s.killSeconds);
    return s;
}

glm::dvec3 strayPosition(const StrayBody& body, double t)
{
    return body.start + body.velocity * (t - body.spawnSeconds);
}

void debrisFieldAt(const DefenceLayout& L, double t, std::vector<DebrisPiece>& out)
{
    out.clear();
    for (int i = 0; i < L.debrisCount; ++i)
    {
        const std::uint32_t k = static_cast<std::uint32_t>(i);
        // its own slow circle about the field's centre: radius, plane tilt, phase, period 15-40 min
        const double r = L.debrisRadiusMeters * std::sqrt(hash01(L.debrisSeed + 1u, k));
        const double tilt = (hash01(L.debrisSeed + 2u, k) - 0.5) * 1.2;
        const double phase = hash01(L.debrisSeed + 3u, k) * 6.2831853;
        const double omega = 6.2831853 / (900.0 + 1500.0 * hash01(L.debrisSeed + 4u, k));
        const double yaw = hash01(L.debrisSeed + 5u, k) * 6.2831853;
        const double a = phase + omega * t;
        const glm::dvec3 u(std::cos(yaw), 0.0, std::sin(yaw));
        const glm::dvec3 v = glm::normalize(glm::dvec3(-std::sin(yaw) * std::cos(tilt), std::sin(tilt), std::cos(yaw) * std::cos(tilt)));
        DebrisPiece p;
        p.index = k;
        p.position = L.debrisCentre + r * (std::cos(a) * u + std::sin(a) * v);
        p.velocity = r * omega * (-std::sin(a) * u + std::cos(a) * v);
        const double h = hash01(L.debrisSeed + 6u, k);
        p.sizeMeters = 0.5 + 5.5 * h * h * h;   // mostly small, a few large plates
        out.push_back(p);
    }
}

void strayBodiesAt(const DefenceLayout& L, double t, double fx, std::vector<StrayBody>& out)
{
    out.clear();
    const double period = strayPeriodSeconds(L);
    // the longest life: across the zone and out again at the slowest drift
    const double life = 2.0 * (L.perimeterMeters + 1500.0) / 30.0;
    const double first = std::max(0.0, std::floor((t - life) / period) - 1.0);
    const double last = std::floor(t / period);
    for (double k = first; k <= last; k += 1.0)
    {
        StrayBody s = strayBody(L, static_cast<std::uint32_t>(k));
        if (s.spawnSeconds > t)
            continue;
        if (s.inbound ? t <= s.killSeconds + fx : glm::length(strayPosition(s, t)) <= L.perimeterMeters + 2000.0)
            out.push_back(s);
    }
}

const char* hubAlertName(HubAlert a)
{
    switch (a)
    {
    case HubAlert::Calm: return "calm";
    case HubAlert::Alert: return "alert";
    case HubAlert::Violation: return "violation";
    }
    return "?";
}
} // namespace game::traffic::security
