#include "src/game/damage/HitVolumeBounds.h"
#include "src/game/localization/LocalizedMessage.h"
#include "src/game/equipment/radar/RadarSensorTypes.h"
// GameSimulation: hub security / traffic control (docs/HUB_SECURITY.md). One component per hub that the lanes, the
// promoted air traffic, the dock traffic, the defence ring and the fines report to:
//  - a ship flying a hub lane AGAINST its direction: the controller's warning, then fines charged to its owner's account
//    at the hub polity's national bank (never refused: debt instead), escalating to a fire-control lock - never a kill in
//    peacetime ([Corridor]);
//  - the city's air traffic (a pure function of time, HubAirTraffic) is PROMOTED to real server ships within 5 km of a
//    player and handed back when far and on track / at its lane's end ([TrafficPromote]);
//  - every scripted (kinematic) traffic ship evades a real ship on a collision course within its own Newtonian limits
//    ([Evade]; the offset layer over its nominal track);
//  - the defence ring tracks stray bodies drifting into the zone and kills the inbound ones (deterministic: the clients
//    draw the same shots) ([HubSecurity]).
#include "src/game/simulation/GameSimulationInternal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "src/game/economy/EconomyService.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/traffic/HubAirTraffic.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/ship/ShipDescriptorRegistry.h"

namespace
{
namespace sec = game::traffic::security;
namespace tr = game::navigation::traffic;

// promotion of the presentation air traffic: within 5 km of a player a slot becomes a real ship. Why 5 km: a 50 m hull
// there is ~0.6 deg (~10 px at 1080p) - plainly a ship - and a head-on closing speed of ~255 m/s (lane traffic 105 m/s +
// a ship at the 150 m/s hub limit) leaves ~20 s to the CPA: the time a 56 m freighter's RCS needs to clear its own beam.
constexpr double PromoteMeters = 5000.0;
constexpr double DemoteMeters = 6500.0;        // hysteresis: handed back only when this far from every player ...
constexpr double LaneEndClearMeters = 3000.0;  // ... or at its lane's end with nobody within this
constexpr std::size_t MaxPromotedPerHub = 32;   // 16: a lane ship behind the player was made real only 317 m off (no room to brake, live zl2)
constexpr double CheckSeconds = 0.25;
constexpr double RepeatWindowSeconds = 1800.0; // earlier incidents within 30 min escalate the fines
constexpr double LockAfterSeconds = 30.0;      // still against the traffic 30 s after the warning: fire-control lock
constexpr double LevelHoldSeconds = 60.0;      // the level stays this long after the incident closes

bool isCityModule(const std::string& moduleId)
{
    return moduleId.size() > 5 && moduleId.compare(moduleId.size() - 5, 5, ".city") == 0;
}

const char* kindName(sec::DefenceKind k)
{
    switch (k)
    {
    case sec::DefenceKind::PointDefenceLaser: return "PD laser";
    case sec::DefenceKind::Coilgun: return "coilgun";
    case sec::DefenceKind::MissileCells: return "missile cells";
    case sec::DefenceKind::RadarMast: return "radar";
    case sec::DefenceKind::Picket: return "picket";
    default: return "?";
    }
}
} // namespace

void GameSimulation::refreshEvasionThreats()
{
    const double now = m_serverTimelineClock.timeSeconds();
    // one list per system pass of the tick (every active system has its own pass at the same server time)
    if (now == m_evasionThreatsTime && m_evasionThreatsSystem == m_activeCelestialSystemId)
        return;
    m_evasionThreatsTime = now;
    m_evasionThreatsSystem = m_activeCelestialSystemId;
    m_evasionThreats.clear();
    // the threats: the real, free-flying ships (players and AI ships under physics); the kinematic traffic is kept apart
    // by its own scripts and evades - it does not threaten
    for (const auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr || !passOwns(shipPtr.get()) || isDockStationControlledShip(id))
            continue;
        const auto& t = shipPtr->core().transform();
        if (t.motion.systemId != m_activeCelestialSystemId)
            continue;
        EvasionThreat th;
        th.id = id;
        // a free-flying ship is a step behind the hub objects in this stage: projected to their epoch
        th.position = t.fullWorldMeters();   // the free-flight step re-materializes it at the hub objects' epoch
        th.velocity = t.motion.worldVelocityMps;
        th.radius = 20.0;
        m_evasionThreats.push_back(th);
    }
}

void GameSimulation::applyTrafficEvasion(EntityId self, glm::dvec3& position, glm::dvec3& velocity, const glm::dvec3& forward,
                                         const glm::dvec3& up, double lengthMeters, double /*dt*/, const std::string& hubId)
{
    refreshEvasionThreats();
    const double now = m_serverTimelineClock.timeSeconds();
    auto found = m_evasion.find(self);
    if (found == m_evasion.end() && m_evasionThreats.empty())
        return;
    const sec::EvasionCaps caps = sec::evasionCapsForLength(lengthMeters);
    const glm::dvec3 nominalPos = position, nominalVel = velocity;
    const glm::dvec3 selfPos = nominalPos + (found != m_evasion.end() ? found->second.offset.offset : glm::dvec3(0.0));
    const glm::dvec3 selfVel = nominalVel + (found != m_evasion.end() ? found->second.offset.velocity : glm::dvec3(0.0));
    sec::EvasionPlan best;
    const EvasionThreat* threat = nullptr;
    for (const auto& th : m_evasionThreats)
    {
        if (th.id == self || glm::length(th.position - selfPos) > 12000.0)
            continue;
        // a slow relative motion is the dispatcher's business (holds, pads), not a collision course
        if (glm::length(th.velocity - selfVel) < 15.0)
            continue;
        const sec::EvasionPlan p = sec::planEvasion(selfPos, selfVel, forward, up, th.position, th.velocity, th.radius, caps);
        if (p.threat && (!best.threat || p.timeToCpa < best.timeToCpa))
        {
            best = p;
            threat = &th;
        }
    }
    // DEV: ELITE_DEV_SECURITY_LOG=1 - the nearest real ship's CPA as this ship sees it (every 2 s, within 3 km)
    static const bool securityLog = std::getenv("ELITE_DEV_SECURITY_LOG") != nullptr;
    if (securityLog)
    {
        static std::unordered_map<std::uint64_t, double> lastLog;
        double& at = lastLog[self.value];
        for (const auto& th : m_evasionThreats)
            if (th.id != self && glm::length(th.position - selfPos) < 3000.0 && now - at >= 2.0)
            {
                at = now;
                const sec::Cpa c = sec::closestApproach(th.position - selfPos, th.velocity - selfVel);
                std::fprintf(stderr, "[Evade] DEV ship %llu vs %llu: range %.0f m, closing %.1f m/s, cpa t=%.1f d=%.0f\n",
                             static_cast<unsigned long long>(self.value), static_cast<unsigned long long>(th.id.value),
                             glm::length(th.position - selfPos), glm::length(th.velocity - selfVel), c.timeSeconds, c.distanceMeters);
            }
    }
    if (found == m_evasion.end())
    {
        if (!best.threat)
            return;
        found = m_evasion.emplace(self, EvasionTrack {}).first;
    }
    EvasionTrack& track = found->second;
    const double dt = track.lastTime < 0.0 ? 0.0 : std::clamp(now - track.lastTime, 0.0, 0.5);
    track.lastTime = now;
    if (best.threat && threat && (track.threat != threat->id.value || now >= track.nextLog))
    {
        const bool fresh = track.threat != threat->id.value;
        track.threat = threat->id.value;
        track.nextLog = now + 2.0;
        char line[512];
        std::snprintf(line, sizeof(line),
                      "[Evade] ship %llu: threat %llu cpa t=%.1f s d=%.0f m (needs %.0f) -> burn %.2f m/s2 %s dir (%.2f,%.2f,%.2f), "
                      "predicted miss %.0f m%s",
                      static_cast<unsigned long long>(self.value), static_cast<unsigned long long>(threat->id.value), best.timeToCpa,
                      best.cpaMeters, best.requiredMeters,
                      best.usesMainDrive ? caps.mainAccelMps2 : caps.lateralAccelMps2,
                      best.usesMainDrive ? "main drive (after turning)" : "RCS", best.direction.x, best.direction.y, best.direction.z,
                      best.predictedMissMeters, best.cannotClear ? " - CANNOT CLEAR: the hulls will touch" : "");
        std::cerr << line << std::endl;
        if (fresh)
            noteHubSecurityEvent(hubId, 3, self, std::string());
    }
    sec::stepEvasionOffset(track.offset, best, caps, dt);
    if (!best.threat && track.threat != 0 && sec::evasionSettled(track.offset))
    {
        std::cerr << "[Evade] ship " << self.value << ": threat " << track.threat << " passed, back on its track" << std::endl;
        track.threat = 0;
    }
    position = nominalPos + track.offset.offset;
    velocity = nominalVel + track.offset.velocity;
    if (!best.threat && track.threat == 0 && sec::evasionSettled(track.offset))
        m_evasion.erase(found);
}

void GameSimulation::noteHubSecurityEvent(const std::string& hubId, int kind, EntityId ship, const std::string& text)
{
    if (hubId.empty())
        return;
    auto& s = m_hubSecurity[hubId];
    switch (kind)
    {
    case 1: s.violators[ship.value] = std::max(s.violators[ship.value], 1); break;   // a violation opened / escalated
    case 2: s.violators.erase(ship.value); break;                                      // cleared
    case 3: ++s.evasions; break;
    case 4: ++s.promotions; break;
    case 5: ++s.intercepts; break;
    default: break;
    }
    s.dirty = true;
    if (!text.empty())
        std::cerr << "[HubSecurity] " << hubId << ": " << text << std::endl;
}

std::vector<std::uint32_t> GameSimulation::cityPromotedAirSlots(EntityId objectId) const
{
    std::vector<std::uint32_t> out;
    for (const auto& [id, p] : m_promotedAir)
        if (p.city == objectId)
            out.push_back(p.slot);
    std::sort(out.begin(), out.end());
    return out;
}


std::vector<GameSimulation::HubRadarContact> GameSimulation::hubRadarContacts(const glm::dvec3& observerWorld, int systemId,
                                                                          std::size_t maxCount) const
{
    std::vector<HubRadarContact> out;
    const auto sys = m_securityBySystem.find(systemId);
    if (sys == m_securityBySystem.end() || maxCount == 0)
        return out;
    struct Candidate
    {
        double distance;
        HubRadarContact contact;
    };
    std::vector<Candidate> found;
    const double universe = m_orbitalUniverseTimeSeconds;
    for (const auto& c : sys->second.cities)
    {
        const auto objIt = m_staticObjects.find(c.city);
        if (objIt == m_staticObjects.end())
            continue;
        const StaticObject& city = objIt->second;
        const glm::dmat3 R(glm::mat3(city.orientation));
        const glm::dvec3 cpos = world::coordinates::fullMeters(city.worldPosition);
        const glm::dvec3 cvel(city.linearVelocity);
        if (glm::length(cpos - observerWorld) > 60000.0)
            continue;   // beyond any hull radar
        const auto push = [&](std::uint32_t key, const glm::dvec3& local, const glm::dvec3& localVel, double rcs, std::uint8_t kind,
                              double radius) {
            HubRadarContact k;
            k.key = key;
            k.boundingRadiusMeters = radius;
            k.worldPosition = cpos + R * local;
            k.worldVelocity = cvel + R * localVel;
            k.radarCrossSection = rcs;
            k.kind = kind;
            found.push_back({glm::length(k.worldPosition - observerWorld), k});
        };
        const std::uint32_t cityBits = (static_cast<std::uint32_t>(c.city.value) & 0x3FFu) << 18;
        if (c.plan && c.lanes && !c.plan->airFleet().empty())
        {
            const auto& layout = game::navigation::traffic::hubAirTrafficLayoutCached(*c.plan, *c.lanes);
            game::navigation::traffic::HubAirSlotPose pose;
            for (const auto& slot : layout.slots())
            {
                bool promoted = false;
                for (const auto& [id, pa] : m_promotedAir)
                    if (pa.city == c.city && pa.slot == slot.id)
                        promoted = true;   // a real ship now: on the radar as itself
                if (!promoted && layout.pose(slot, universe, pose))
                {
                    // the hull's size: its catalog passport's dimensions, else the presentation hull's length with the
                    // catalog's widest proportions (a Cobra: 26 m wide on 22 m long - 0.75 x the length as the radius)
                    double radius = 15.0;
                    if (slot.fleetIndex >= 0 && static_cast<std::size_t>(slot.fleetIndex) < c.plan->airFleet().size())
                    {
                        const auto& hull = c.plan->airFleet()[static_cast<std::size_t>(slot.fleetIndex)];
                        radius = 0.75 * hull.lengthMeters;
                        if (!hull.passport.empty())
                        {
                            const ShipDescriptor& d = ShipDescriptorRegistry::resolve(ObjectType::CobraMk1, hull.passport);
                            const auto dim = d.logicalDimensions();
                            radius = game::damage::dimensionsBoundingRadius(dim.length, dim.width, dim.height);
                        }
                    }
                    push(0xC0000000u | cityBits | (slot.id & 0xFFFFu), pose.position, pose.velocity, 1.0,
                         game::radar::RadarTrackKindShip, radius);
                }
            }
        }
        if (c.plan && !c.defence.sites.empty())
        {
            std::vector<game::traffic::security::StrayBody> bodies;
            game::traffic::security::strayBodiesAt(c.defence, universe, 0.0, bodies);
            for (const auto& b : bodies)
                push(0xE0000000u | cityBits | (b.index & 0x3FFFFu), game::traffic::security::strayPosition(b, universe),
                     b.velocity, std::max(0.01, b.sizeMeters * b.sizeMeters / 400.0), game::radar::RadarTrackKindDebris,
                     0.5 * b.sizeMeters * 1.7320508);   // a piece of size s: inside the sphere round its s-cube
        }
        if (c.plan)
        {
            std::vector<game::traffic::security::DebrisPiece> pieces;
            game::traffic::security::debrisFieldAt(c.defence, universe, pieces);
            for (const auto& p : pieces)
                push(0xA0000000u | cityBits | (p.index & 0x3FFFFu), p.position, p.velocity,
                     std::max(0.01, p.sizeMeters * p.sizeMeters / 400.0), game::radar::RadarTrackKindDebris,
                     0.5 * p.sizeMeters * 1.7320508);
        }
    }
    if (found.size() > maxCount)
    {
        std::nth_element(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(maxCount), found.end(),
                         [](const Candidate& x, const Candidate& y) { return x.distance < y.distance; });
        found.resize(maxCount);
    }
    for (auto& f : found)
        out.push_back(f.contact);
    return out;
}


void GameSimulation::despawnPromotedAirShip(EntityId id)
{
    m_promotedAir.erase(id);
    m_airTrafficShells.erase(id);
    // collapse, then remove, then forget (forgetShip first released the condition cell and dematerializeShip
    // allocated a fresh one only for removeTrafficShell to release it again)
    if (getShip(id))
        dematerializeShip(id, game::simulation::ShipSimulationTier::TrafficShell);
    removeTrafficShell(id);
    forgetShip(id);
}

void GameSimulation::updateHubSecurity(double dt)
{
    const auto perfStart = std::chrono::steady_clock::now();
    const double now = m_serverTimelineClock.timeSeconds();
    const double universe = m_orbitalUniverseTimeSeconds;

    // ---- the cities of the active system (cached per system: static objects are only ever added)
    HubSecuritySystem& sys = m_securityBySystem[m_activeCelestialSystemId];
    if (sys.staticGeneration != m_staticObjectsGeneration)
    {
        sys.staticGeneration = m_staticObjectsGeneration;
        sys.cities.clear();
        for (const auto& [oid, obj] : m_staticObjects)
        {
            if (obj.type != ObjectType::HubBeltCity || obj.systemId != m_activeCelestialSystemId || !isCityModule(obj.hubModuleId))
                continue;
            HubSecurityCity c;
            c.city = oid;
            c.hubId = obj.hubId;
            c.lanes = tr::HubLaneCatalog::forFrameModule(obj.hubModuleId);
            c.plan = game::navigation::HubPlanCatalog::forFrameModule(obj.hubModuleId);
            if (!c.lanes && !c.plan)
                continue;
            if (c.plan)
                c.defence = sec::buildDefenceLayout(*c.plan, c.lanes);
            sys.cities.push_back(std::move(c));
        }
        std::sort(sys.cities.begin(), sys.cities.end(), [](const auto& a, const auto& b) { return a.hubId < b.hubId; });
        std::cerr << "[HubSecurity] system " << m_activeCelestialSystemId << ": " << sys.cities.size()
                  << " hub cities under traffic control" << std::endl;
    }
    auto& securityCities = sys.cities;
    const auto cityOf = [&](EntityId id) -> const HubSecurityCity* {
        for (const auto& c : securityCities)
            if (c.city == id)
                return &c;
        return nullptr;
    };

    // ---- the players (positions / velocities) of the active system
    struct Player
    {
        EntityId id;
        Ship* ship;
        glm::dvec3 position;
        glm::dvec3 velocity;
    };
    std::vector<Player> players;
    for (auto& [id, shipPtr] : m_ships)
        if (shipPtr && isPlayerControlled(id) && passOwns(shipPtr.get()) &&
            shipPtr->core().transform().motion.systemId == m_activeCelestialSystemId)
            players.push_back({id, shipPtr.get(),
                               shipPtr->core().transform().fullWorldMeters(),
                               shipPtr->core().transform().motion.worldVelocityMps});

    // ---- the promoted air traffic: its pose every tick (the nominal lane track + its evasion layer)
    std::vector<EntityId> handBack;
    for (auto& [sid, pa] : m_promotedAir)
    {
        const auto objIt = m_staticObjects.find(pa.city);
        if (objIt != m_staticObjects.end() && objIt->second.systemId != m_activeCelestialSystemId)
            continue;   // another system's pass owns it
        const HubSecurityCity* c = cityOf(pa.city);
        if (!c || !c->plan || !c->lanes || objIt == m_staticObjects.end())
        {
            handBack.push_back(sid);
            continue;
        }
        ShipTransform* target = nullptr;
        if (auto it = m_shipShells.find(sid); it != m_shipShells.end())
            target = &it->second.transform;
        else if (Ship* ship = getShip(sid))
            target = &ship->core().transform();
        if (!target)
        {
            handBack.push_back(sid);
            continue;
        }
        const auto& layout = tr::hubAirTrafficLayoutCached(*c->plan, *c->lanes);
        const tr::HubAirLaneTrack* lt = layout.track(pa.lane);
        if (!lt)
        {
            handBack.push_back(sid);
            continue;
        }
        const StaticObject& city = objIt->second;
        const glm::dmat3 R(glm::mat3(city.orientation));
        const glm::dvec3 cpos = world::coordinates::fullMeters(city.worldPosition);
        const glm::dvec3 cvel(city.linearVelocity);
        if (pa.curSpeedMps < 0.0)
            pa.curSpeedMps = pa.speedMps;
        const double along = pa.along0 + pa.curSpeedMps * (universe - pa.time0);
        tr::HubAirSlotPose pose;
        layout.poseAt(pa.lane, std::min(along, lt->lengthMeters), pa.curSpeedMps, pose);
        // the TRAFFIC RULE for the lane ship too: a player ahead in its lane - keep the gap (60 m + 2 s), at most its
        // speed + the closing the gap allows; free road - back to the lane speed. At most 6 m/s2 either way (a lane
        // ship re-entering at its lane's portal ran into the player from behind at 27 m/s, suite v3)
        {
            const glm::dmat3 Rt = glm::transpose(R);
            double want = std::min(pa.speedMps, lt->speedAt(along));   // the route's turns (connector limits)
            for (const auto& pl : players)
            {
                const glm::dvec3 rel = Rt * (pl.position - (cpos + R * pose.position));
                const double ahead = glm::dot(rel, pose.forward);
                if (ahead <= 0.0 || ahead > 1500.0 || glm::length(rel - pose.forward * ahead) > 60.0 + pa.lengthMeters)
                    continue;
                const double vp = glm::dot(Rt * (pl.velocity - cvel), pose.forward);
                const double gap = 60.0 + pa.lengthMeters + 2.0 * pa.curSpeedMps;
                want = std::min(want, std::max(0.0, vp + 0.5 * (ahead - gap)));
            }
            // braking: 6 m/s2 normally, up to 15 m/s2 when the gap is closing fast (a lane ship made real 317 m behind a
            // slow player at 105 m/s needed ~470 m at 6 - it rammed it, live zl2)
            const double brakeMps2 = pa.curSpeedMps - want > 20.0 ? 15.0 : 6.0;
            const double next = std::clamp(want, pa.curSpeedMps - brakeMps2 * dt, pa.curSpeedMps + 6.0 * dt);
            if (std::abs(next - pa.curSpeedMps) > 1.0e-9)
            {
                pa.along0 = along;
                pa.time0 = universe;
                pa.curSpeedMps = next;
                layout.poseAt(pa.lane, std::min(along, lt->lengthMeters), pa.curSpeedMps, pose);
            }
        }
        if (along > lt->lengthMeters)
            pose.position += pose.forward * (along - lt->lengthMeters);   // past the lane's end: straight on into open sky
        glm::dvec3 position = cpos + R * pose.position;
        glm::dvec3 velocity = cvel + R * pose.velocity;
        applyTrafficEvasion(sid, position, velocity, R * pose.forward, R * pose.up, pa.lengthMeters, dt, pa.hubId);
        glm::mat4 orientation(glm::mat3(R) * glm::mat3(pose.local));
        // a live traffic ship (GameSimulationLiveTraffic): the lane gives only its point, its pilot flies it
        const bool live = getShip(sid) &&
            !noteTrafficSlot(sid, c->hubId, cpos + R * pose.position, cvel + R * pose.velocity, orientation, true);
        if (live)
            continue;   // no hand-back while it flies itself (it goes back to the script first)
        target->orientation = orientation;
        // the lane's world pose; the local description kept consistent with it (core stabilization D-1)
        game::simulation::setWorldKinematics(*target, position, velocity);
        target->motion.hubId = c->hubId;
        double nearest = 1.0e30;
        for (const auto& p : players)
            nearest = std::min(nearest, glm::length(p.position - position));
        const bool settled = m_evasion.find(sid) == m_evasion.end();
        if (along > lt->lengthMeters ? (nearest > LaneEndClearMeters || along > lt->lengthMeters + 20000.0)
                                     : (nearest > DemoteMeters && settled))
        {
            std::cerr << "[TrafficPromote] " << pa.hubId << " slot " << pa.slot << " (ship " << sid.value << ") handed back to the "
                      << "presentation: " << (along > lt->lengthMeters ? "its lane's end" : "far from every player, on track")
                      << ", nearest player " << static_cast<long long>(nearest) << " m" << std::endl;
            handBack.push_back(sid);
        }
    }
    for (const EntityId id : handBack)
        despawnPromotedAirShip(id);

    // ---- the 4 Hz control pass
    sys.checkSeconds -= dt;
    if (sys.checkSeconds <= 0.0)
    {
        sys.checkSeconds = CheckSeconds;
        const double step = CheckSeconds;
        std::unordered_set<std::uint64_t> judged;
        for (auto& c : securityCities)
        {
            const auto objIt = m_staticObjects.find(c.city);
            if (objIt == m_staticObjects.end())
                continue;
            const StaticObject& city = objIt->second;
            const glm::dmat3 R(glm::mat3(city.orientation));
            const glm::dmat3 Rt = glm::transpose(R);
            const glm::dvec3 cpos = world::coordinates::fullMeters(city.worldPosition);
            const glm::dvec3 cvel(city.linearVelocity);
            const double perimeter = c.plan ? c.defence.perimeterMeters : 8000.0;
            auto& state = m_hubSecurity[c.hubId];
            // DEV: ELITE_DEV_LANE_START="<lane index>:<along m>:<speed m/s>[:with]" - once, the first player in normal flight
            // near this city is put on that lane's centre line, nose and velocity AGAINST the lane (":with" = along it):
            // live checks of the wrong-way judge, the evasion and the turrets' lock, in the frame the judge uses
            if (static bool laneStartDone = false; !laneStartDone && c.lanes)
                if (const char* ls = std::getenv("ELITE_DEV_LANE_START"); ls && *ls)
                {
                    int laneIndex = 0;
                    double along = 0.0, speed = 0.0;
                    char mode[16] = {};
                    const int n = std::sscanf(ls, "%d:%lf:%lf:%15s", &laneIndex, &along, &speed, mode);
                    for (auto& p : players)
                    {
                        auto& motion = p.ship->core().transform().motion;
                        if (n < 3 || !motion.freeFlight || glm::length(Rt * (p.position - cpos)) > 20000.0 ||
                            laneIndex < 0 || laneIndex >= static_cast<int>(c.lanes->lanes().size()))
                            continue;
                        const auto& lane = c.lanes->lanes()[static_cast<std::size_t>(laneIndex)];
                        double left = along;
                        glm::dvec3 at = lane.stationsLocal.front(), dir(1.0, 0.0, 0.0);
                        for (std::size_t k = 0; k + 1 < lane.stationsLocal.size(); ++k)
                        {
                            const glm::dvec3 seg = lane.stationsLocal[k + 1] - lane.stationsLocal[k];
                            const double len = glm::length(seg);
                            if (len < 1.0e-6)
                                continue;
                            dir = seg / len;
                            at = lane.stationsLocal[k] + dir * std::min(left, len);
                            if (left <= len)
                                break;
                            left -= len;
                        }
                        const glm::dvec3 fwdLocal = std::string(mode) == "with" ? dir : -dir;
                        const glm::dvec3 pos = cpos + R * at;
                        const glm::dvec3 fwd = R * fwdLocal;
                        auto& tr = p.ship->core().transform();
                        auto& frame = motion.travelFrame;
                        const glm::dvec3 shipPos = pos;   // same epoch as the hub objects (free-flight step)
                        frame.originMeters = shipPos;
                        frame.linearVelocityMps = cvel + fwd * speed;
                        motion.localPositionMeters = glm::dvec3(0.0);
                        motion.localVelocityMps = glm::dvec3(0.0);
                        tr.setWorldPositionMeters(shipPos);
                        const glm::dvec3 right = glm::normalize(glm::cross(fwd, R * glm::dvec3(0.0, 1.0, 0.0)));
                        tr.orientation = glm::mat4(glm::mat3(glm::dmat3(right, glm::cross(right, fwd), -fwd)));
                        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
                        p.position = pos;
                        p.velocity = cvel + fwd * speed;
                        laneStartDone = true;
                        std::cerr << "[Corridor] DEV ship " << p.id.value << " put on lane " << lane.id << " +" << along << " m, "
                                  << speed << " m/s " << (std::string(mode) == "with" ? "WITH" : "AGAINST") << " it" << std::endl;
                        break;
                    }
                }
            std::vector<glm::dvec3> nearLocal;
            for (auto& p : players)
            {
                const glm::dvec3 local = Rt * (p.position - cpos);
                if (glm::length(local) > perimeter + PromoteMeters + 20000.0)
                    continue;
                nearLocal.push_back(local);
                if (!c.lanes)
                    continue;
                if (isDockStationControlledShip(p.id))
                    continue;   // the station flies it (a departure / an arrival): not the pilot's lane violation
                judged.insert(p.id.value);
                // -------- the lanes: flying against the traffic
                auto& ww = m_wrongWay[p.id];
                if (!ww.hubId.empty() && ww.hubId != c.hubId && ww.watch.active)
                    continue;   // its open incident belongs to another hub
                sec::LaneProbe probe;
                const bool inLane = glm::length(local) <= perimeter && sec::probeLane(*c.lanes, local, 10.0, probe);
                const glm::dvec3 vLocal = Rt * (p.velocity - cvel);
                int prior = 0;
                ww.incidentTimes.erase(std::remove_if(ww.incidentTimes.begin(), ww.incidentTimes.end(),
                                                      [&](double t) { return now - t > RepeatWindowSeconds; }),
                                       ww.incidentTimes.end());
                prior = static_cast<int>(ww.incidentTimes.size());
                const sec::WrongWayStep r = sec::stepWrongWay(ww.watch, inLane, probe.lane, probe.direction, vLocal, step, prior);
                // DEV: ELITE_DEV_SECURITY_LOG=1 - the judge's inputs every 2 s (live checks of the lane geometry / frames)
                static const bool securityLog = std::getenv("ELITE_DEV_SECURITY_LOG") != nullptr;
                if (securityLog && now - ww.debugLogAt >= 2.0)
                {
                    ww.debugLogAt = now;
                    char line[400];
                    std::snprintf(line, sizeof(line),
                                  "[Corridor] DEV ship %llu city (%.0f,%.0f,%.0f) v (%.1f,%.1f,%.1f) |city v| %.0f in lane %d (%s, lateral %.0f m) "
                                  "cos %.2f active %d against %.1f s",
                                  static_cast<unsigned long long>(p.id.value), local.x, local.y, local.z, vLocal.x, vLocal.y, vLocal.z,
                                  glm::length(cvel), inLane ? probe.lane : -1,
                                  inLane ? c.lanes->lanes()[static_cast<std::size_t>(probe.lane)].id.c_str() : "-", probe.lateralMeters,
                                  r.cosAgainst, ww.watch.active ? 1 : 0, ww.watch.againstSeconds);
                    std::cerr << line << std::endl;
                }
                auto& motion = p.ship->core().transform().motion;
                const std::string laneId =
                    ww.watch.lane >= 0 && ww.watch.lane < static_cast<int>(c.lanes->lanes().size()) ? c.lanes->lanes()[static_cast<std::size_t>(ww.watch.lane)].id : std::string();
                const auto emit = [&](int ev, double fine, double debt) {
                    motion.securityEvent = static_cast<std::uint8_t>(ev);
                    motion.securityEventFineCr = fine;
                    motion.securityEventDebtCr = debt;
                    motion.securityHubId = c.hubId;
                    motion.securityLaneId = laneId;
                    ++motion.securitySerial;
                };
                char line[512];
                switch (r.event)
                {
                case sec::WrongWayEvent::Started:
                {
                    ww.hubId = c.hubId;
                    ww.laneId = laneId;
                    const int level = prior >= 2 ? 3 : 1;
                    motion.securityLevel = static_cast<std::uint8_t>(std::max<int>(motion.securityLevel, level));
                    emit(level == 3 ? 3 : 1, 0.0, 0.0);
                    std::snprintf(line, sizeof(line),
                                  "[Corridor] ship %llu WRONG WAY in lane %s of %s: %.0f m/s, cos %.2f against the lane, %d earlier incident(s) "
                                  "-> controller warning%s",
                                  static_cast<unsigned long long>(p.id.value), laneId.c_str(), c.hubId.c_str(), glm::length(vLocal),
                                  r.cosAgainst, prior, level == 3 ? " + fire-control LOCK (repeat offender)" : "");
                    std::cerr << line << std::endl;
                    noteHubSecurityEvent(c.hubId, 1, p.id, std::string());
                    break;
                }
                case sec::WrongWayEvent::Fine:
                {
                    const auto paid = game::economy::service::chargeFine(
                        static_cast<std::uint32_t>(p.id.value), c.hubId, r.fineCr,
                        game::localization::Message("security.fine.memo.wrong_way").arg("lane", laneId).str());
                    // the display mirror of the bank (the bank debt / payment is the truth); no economy: the fine assessed
                    motion.securityFinesTotalCr += paid.charged ? paid.paidCr + paid.debtCr : r.fineCr;
                    motion.securityLevel = static_cast<std::uint8_t>(std::max<int>(motion.securityLevel, 2));
                    emit(2, r.fineCr, paid.debtCr);
                    state.finesCr += r.fineCr;
                    std::snprintf(line, sizeof(line),
                                  "[Corridor] ship %llu fined %.0f cr (%s: paid %.0f, debt %.0f) - lane %s of %s, %.0f s / %.0f m against "
                                  "the traffic, incident %.0f cr, total %.0f cr",
                                  static_cast<unsigned long long>(p.id.value), r.fineCr, paid.charged ? "banked" : "no economy", paid.paidCr,
                                  paid.debtCr, laneId.c_str(), c.hubId.c_str(), ww.watch.againstSeconds, ww.watch.metersAgainst,
                                  ww.watch.fineChargedCr, motion.securityFinesTotalCr);
                    std::cerr << line << std::endl;
                    noteHubSecurityEvent(c.hubId, 1, p.id, std::string());
                    break;
                }
                case sec::WrongWayEvent::Ended:
                    ww.incidentTimes.push_back(now);
                    ww.levelUntil = now + LevelHoldSeconds;
                    emit(4, 0.0, 0.0);
                    std::cerr << "[Corridor] ship " << p.id.value << " clear of lane " << ww.laneId << " of " << c.hubId << ": incident "
                              << static_cast<long long>(ww.watch.fineChargedCr) << " cr" << std::endl;
                    break;
                default:
                    break;
                }
                if (ww.watch.active && ww.watch.againstSeconds > LockAfterSeconds && motion.securityLevel < 3)
                {
                    motion.securityLevel = 3;
                    emit(3, 0.0, 0.0);
                    noteHubSecurityEvent(c.hubId, 1, p.id,
                                         "fire-control LOCK on ship " + std::to_string(p.id.value) + " (" +
                                             std::to_string(static_cast<int>(ww.watch.againstSeconds)) +
                                             " s against the traffic): turrets tracking, patrol called - no fire in peacetime");
                }
                if (!ww.watch.active && motion.securityLevel > 0 && ww.levelUntil > 0.0 && now > ww.levelUntil)
                {
                    motion.securityLevel = 0;
                    ww.levelUntil = 0.0;
                    ww.hubId.clear();
                    noteHubSecurityEvent(c.hubId, 2, p.id, "ship " + std::to_string(p.id.value) + " cleared");
                }
            }

            // -------- promotion of the presentation air traffic near the players
            if (c.plan && c.lanes && !nearLocal.empty() && !c.plan->airFleet().empty())
            {
                const auto& layout = tr::hubAirTrafficLayoutCached(*c.plan, *c.lanes);
                std::size_t count = 0;
                for (const auto& [id, pa] : m_promotedAir)
                    count += pa.city == c.city ? 1 : 0;
                // the NEAREST slots are made real first (author 2026-11-22: a slot made real 47 m from the player, inside
                // its lane - the cap had been filled in list order, the ghost ahead of the player stayed a ghost); a
                // candidate much nearer than the farthest real one takes its place
                struct Candidate
                {
                    const tr::HubAirSlot* slot;
                    tr::HubAirSlotPose pose;
                    double nearest;
                };
                std::vector<Candidate> candidates;
                for (const auto& slot : layout.slots())
                {
                    const auto& hull = c.plan->airFleet()[static_cast<std::size_t>(slot.fleetIndex)];
                    if (hull.passport.empty())
                        continue;   // a presentation-only hull (the city shuttle): no catalog ship to make real
                    bool taken = false;
                    for (const auto& [id, pa] : m_promotedAir)
                        if (pa.city == c.city && pa.slot == slot.id)
                            taken = true;
                    tr::HubAirSlotPose cpose;
                    if (taken || !layout.pose(slot, universe, cpose))
                        continue;
                    double nearest = 1.0e30;
                    for (const auto& l : nearLocal)
                        nearest = std::min(nearest, glm::length(l - cpose.position));
                    if (nearest <= PromoteMeters)
                        candidates.push_back({&slot, cpose, nearest});
                }
                std::sort(candidates.begin(), candidates.end(),
                          [](const Candidate& a, const Candidate& b) { return a.nearest < b.nearest; });
                tr::HubAirSlotPose pose;
                for (const auto& cand : candidates)
                {
                    if (count >= MaxPromotedPerHub)
                    {
                        // the farthest real one of this city gives its place to a much nearer candidate
                        EntityId farthest {};
                        double farD = -1.0;
                        for (const auto& [id, pa] : m_promotedAir)
                        {
                            if (pa.city != c.city)
                                continue;
                            const Ship* sh = getShip(id);
                            if (!sh)
                                continue;
                            const glm::dvec3 l = glm::transpose(R) * (sh->core().transform().fullWorldMeters() - cpos);
                            double d = 1.0e30;
                            for (const auto& n : nearLocal)
                                d = std::min(d, glm::length(n - l));
                            if (d > farD)
                            {
                                farD = d;
                                farthest = id;
                            }
                        }
                        if (farthest.value == 0 || farD < cand.nearest + 1000.0)
                            break;
                        despawnPromotedAirShip(farthest);
                        --count;
                    }
                    const auto& slot = *cand.slot;
                    const auto& hull = c.plan->airFleet()[static_cast<std::size_t>(slot.fleetIndex)];
                    pose = cand.pose;
                    const double nearest = cand.nearest;
                    const glm::dvec3 position = cpos + R * pose.position;
                    const glm::dvec3 velocity = cvel + R * pose.velocity;
                    const glm::mat4 orientation(glm::mat3(R) * glm::mat3(pose.local));
                    ShipInitData init;
                    init.visual.shipType = hull.name;
                    char callsign[64];
                    std::snprintf(callsign, sizeof(callsign), "%s-%03u", hull.name.c_str(), slot.id % 1000u);
                    init.visual.shipName = callsign;
                    init.registry.instanceId = 970000000ull + (static_cast<std::uint64_t>(c.city.value % 1000ull) << 16) + slot.id;
                    init.registry.ownerName = "city air traffic " + c.hubId;
                    init.registry.registrationId = callsign;
                    init.registry.shipRole = ShipRoleType::Civilian;
                    const ShipDescriptor& descriptor = ShipDescriptorRegistry::resolve(ObjectType::CobraMk1, hull.passport);
                    const EntityId id = spawnTrafficShell(city.systemId, descriptor, position, velocity, init, orientation, 0, 0);
                    if (id.value == 0)
                        continue;
                    PromotedAirShip pa;
                    pa.city = c.city;
                    pa.hubId = c.hubId;
                    pa.slot = slot.id;
                    pa.lane = slot.lane;
                    pa.along0 = pose.alongMeters;
                    pa.time0 = universe;
                    pa.speedMps = slot.speedMps;
                    pa.curSpeedMps = glm::length(pose.velocity);   // where it is on its route (a turn: slower)
                    pa.lengthMeters = hull.lengthMeters;
                    m_promotedAir[id] = pa;
                    m_airTrafficShells.insert(id);
                    auto& info = m_dockTrafficInfo[id];
                    info.active = true;
                    info.callsign = callsign;
                    info.phase = "lane";
                    info.destination = layout.track(slot.lane) ? layout.track(slot.lane)->name : std::string();
                    ++count;
                    std::cerr << "[TrafficPromote] " << c.hubId << " slot " << slot.id << " (" << hull.name << ", " << hull.passport
                              << ") -> real ship " << id.value << " at lane " << info.destination << " +"
                              << static_cast<long long>(pose.alongMeters) << " m, " << static_cast<long long>(nearest)
                              << " m from a player (" << count << " promoted here)" << std::endl;
                    noteHubSecurityEvent(c.hubId, 4, id, std::string());
                }
            }

            // -------- the defence ring: stray bodies (deterministic, the clients draw the same shots)
            if (c.plan && !c.defence.sites.empty() && !nearLocal.empty())
            {
                std::vector<sec::StrayBody> bodies;
                sec::strayBodiesAt(c.defence, universe, 0.0, bodies);
                int tracked = 0;
                for (const auto& b : bodies)
                {
                    if (b.inbound && universe < b.killSeconds)
                        ++tracked;
                    if (c.lastStrayCheck >= 0.0 && b.spawnSeconds > c.lastStrayCheck && b.spawnSeconds <= universe)
                    {
                        char line[256];
                        std::snprintf(line, sizeof(line), "radar: stray #%u, %.1f m, %.0f m/s, %s", b.index, b.sizeMeters,
                                      glm::length(b.velocity), b.inbound ? "INBOUND - weapons free at the engagement radius" : "passing - tracked only");
                        noteHubSecurityEvent(c.hubId, 0, EntityId {}, line);
                    }
                }
                std::vector<sec::StrayBody> recent;
                sec::strayBodiesAt(c.defence, universe, 5.0, recent);
                for (const auto& b : recent)
                    if (b.inbound && c.lastStrayCheck >= 0.0 && b.killSeconds > c.lastStrayCheck && b.killSeconds <= universe && b.shooter >= 0)
                    {
                        const auto& site = c.defence.sites[static_cast<std::size_t>(b.shooter)];
                        char line[256];
                        std::snprintf(line, sizeof(line), "stray #%u (%.1f m) destroyed by %s #%u at %.0f m from the centre, %.0f m from the gun",
                                      b.index, b.sizeMeters, kindName(site.kind), site.id, glm::length(b.killPoint),
                                      glm::length(b.killPoint - site.position));
                        noteHubSecurityEvent(c.hubId, 5, EntityId {}, line);
                    }
                state.tracked = tracked;
                c.lastStrayCheck = universe;
            }

            // -------- the hub's state
            const sec::HubAlert alert = !state.violators.empty() ? sec::HubAlert::Violation
                                        : state.tracked > 0       ? sec::HubAlert::Alert
                                                                  : sec::HubAlert::Calm;
            if (alert != state.alert)
            {
                std::cerr << "[HubSecurity] " << c.hubId << ": " << sec::hubAlertName(state.alert) << " -> " << sec::hubAlertName(alert)
                          << " (" << state.tracked << " stray inbound, " << state.violators.size() << " violator(s))" << std::endl;
                state.alert = alert;
            }
            if (state.dirty && now - state.lastSummary >= 60.0)
            {
                state.lastSummary = now;
                state.dirty = false;
                std::cerr << "[HubSecurity] " << c.hubId << " summary: " << sec::hubAlertName(state.alert) << ", fines "
                          << static_cast<long long>(state.finesCr) << " cr, strays destroyed " << state.intercepts << ", evasions "
                          << state.evasions << ", promotions " << state.promotions << std::endl;
            }
        }
        // a ship that left every city's zone with an incident open: it is clear of the lanes
        for (auto& p : players)
        {
            const auto it = m_wrongWay.find(p.id);
            if (judged.count(p.id.value) || it == m_wrongWay.end() || !it->second.watch.active)
                continue;
            const sec::WrongWayStep r = sec::stepWrongWay(it->second.watch, false, -1, glm::dvec3(0.0), glm::dvec3(0.0), step, 0);
            if (r.event == sec::WrongWayEvent::Ended)
            {
                it->second.incidentTimes.push_back(now);
                it->second.levelUntil = now + LevelHoldSeconds;
                auto& motion = p.ship->core().transform().motion;
                motion.securityEvent = 4;
                ++motion.securitySerial;
                std::cerr << "[Corridor] ship " << p.id.value << " left the hub zone: incident closed" << std::endl;
            }
        }
    }

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - perfStart).count();
    m_securityPerfMs += ms;
    ++m_securityPerfTicks;
    if (now - m_securityPerfLogAt >= 30.0)
    {
        if (m_securityPerfTicks > 0 && m_securityPerfLogAt > 0.0)
        {
            char line[200];
            std::snprintf(line, sizeof(line), "[HubSecurity] perf: %.3f ms/tick avg over %lld ticks (%zu cities, %zu promoted, %zu evading)",
                          m_securityPerfMs / static_cast<double>(m_securityPerfTicks), m_securityPerfTicks, securityCities.size(),
                          m_promotedAir.size(), m_evasion.size());
            std::cerr << line << std::endl;
        }
        m_securityPerfLogAt = now;
        m_securityPerfMs = 0.0;
        m_securityPerfTicks = 0;
    }
}
