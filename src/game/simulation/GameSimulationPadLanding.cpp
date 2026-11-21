// Hub city pads (author 2026-11-21: «каждому модулю города посадочную площадку ... в инфо-карточке адрес, что за
// объект и кнопку - стыковка»; «меню посадки вызывать кликом по модулю»). The city's pad service flies the ship that
// asked for a building's pad: up / down to the transit height over the city (above every building, under the 430 m
// lanes), across to over the pad, down onto it at a walking pace; then it holds it there. Any control input from the
// pilot (LandOnPad index 0, the client's takeover) gives the ship back at once - on the way or on the pad (take-off).
// Kinematic like the dock's station control (the same hub-frame refresh on the moving hub).

#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/HubPlanPads.h"
#include "src/game/navigation/TravelFrameSystem.h"

namespace
{
// the pilot took the controls back (server time per ship): no capture / touchdown for a while - he is taking off
std::unordered_map<std::uint64_t, double> g_padReleasedAt;
} // namespace

namespace
{
const StaticObject* findCityFrame(const std::unordered_map<EntityId, StaticObject>& objects, const std::string& frameModuleId)
{
    for (const auto& [id, obj] : objects)
        if (obj.hubModuleId == frameModuleId)
            return &obj;
    return nullptr;
}
} // namespace

bool GameSimulation::requestPadLanding(EntityId shipId, const std::string& frameModuleId, const std::string& address, bool land)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    if (!land)
    {
        if (m_padLandings.erase(shipId) != 0)
        {
            g_padReleasedAt[shipId.value] = serverTime();
            ship->setControlState(ShipControlState {});
            std::cerr << "[PadLanding] ship " << shipId.value << ": the pilot takes the controls" << std::endl;
        }
        return true;
    }
    const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(frameModuleId);
    const StaticObject* city = findCityFrame(m_staticObjects, frameModuleId);
    const game::navigation::HubPlanModule* building = nullptr;
    if (plan)
        for (const auto& m : plan->modules())
            if (m.address == address)
                building = &m;
    auto& tr = ship->core().transform();
    const char* refusal = !plan || !city ? "no such city"
                        : !building || !building->pad ? "no pad at that address"
                        : isDockStationControlledShip(shipId) && m_padLandings.count(shipId) == 0 ? "the station flies the ship"
                            : nullptr;
    if (!refusal && glm::length(tr.fullWorldMeters() - world::coordinates::fullMeters(city->worldPosition)) > 60000.0)
        refusal = "farther than 60 km";
    // a ship near the city but not in its hub's frame (normal flight): the pad service takes it into the frame where
    // it is (the kinematic hold starts from its place; the frame velocity is the hub's)
    const bool freeFlight = m_freeFlights.count(shipId) != 0;
    if (!refusal && freeFlight)
    {
        // normal flight (the global integrator) ends here: the pad service holds the ship from now on (left running,
        // the global step flew the held ship into the Earth - live 2026-11-21)
        auto& motion = tr.motion;
        motion.freeFlight = false;
        motion.freeFlightThrust = 0;
        motion.jEngine = false;
        motion.velocityReferenceKind = 0;
        motion.freeFlightReferenceId.clear();
        motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
        motion.freeFlightOrbitBodyId.clear();
        m_freeFlights.erase(shipId);
        std::cerr << "[PadLanding] ship " << shipId.value << ": normal flight off for the pad service" << std::endl;
    }
    if (!refusal &&
        (freeFlight || tr.motion.mode != game::navigation::MotionMode::HubTactical || tr.motion.hubId != city->hubId))
    {
        const auto* hubFrame = hubNavigationFrame(city->hubId);
        game::navigation::ReferenceFrame frame;
        frame.type = game::navigation::ReferenceFrameType::OrbitalHub;
        frame.systemId = city->systemId;
        frame.hubId = city->hubId;
        const glm::mat4 orientation = tr.orientation;
        if (hubFrame && hubFrame->valid)
            frame.localOffsetMeters = hubFrame->worldToLocalPosition(tr.fullWorldMeters());
        if (!hubFrame || !hubFrame->valid || !placeShipInReferenceFrame(shipId, frame))
            refusal = "the hub frame is unavailable";
        else
        {
            tr.orientation = orientation;
            std::cerr << "[PadLanding] ship " << shipId.value << " taken into hub " << city->hubId << "'s frame" << std::endl;
            m_padNotices[shipId] = {freeFlight ? "taken:flight" : "taken:frame", serverTime()};
        }
    }
    if (refusal)
    {
        std::cerr << "[PadLanding] ship " << shipId.value << " pad " << frameModuleId << "/" << address << " refused: " << refusal
                  << std::endl;
        const std::string why = refusal;
        const char* key = why == "no such city" ? "no_city" : why == "no pad at that address" ? "no_pad"
                        : why == "the station flies the ship" ? "station" : why == "farther than 60 km" ? "far" : "frame";
        m_padNotices[shipId] = {std::string("refused:") + key, serverTime()};
        return false;
    }
    return startPadService(shipId, frameModuleId, address, 0);
}

bool GameSimulation::startPadService(EntityId shipId, const std::string& frameModuleId, const std::string& address, int phase)
{
    Ship* ship = getShip(shipId);
    const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(frameModuleId);
    const game::navigation::HubPlanModule* building = nullptr;
    if (plan)
        for (const auto& m : plan->modules())
            if (m.address == address)
                building = &m;
    if (!ship || !building || !building->pad)
        return false;
    PadLanding p;
    p.frameModuleId = frameModuleId;
    p.address = address;
    p.padLocal = building->padPosition;
    p.outLocal = building->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
    p.autoLanding = building->autoLanding;
    // the transit height: over the tallest building of its tier by 80 m (the city air lanes fly at 430 m)
    double tallest = 0.0;
    for (const auto& b : plan->blocks())
        tallest = std::max(tallest, building->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
    p.transitLocal = std::min(380.0, tallest + 80.0);
    p.phase = phase;
    p.sinceS = serverTime();
    m_padLandings[shipId] = p;
    ship->setControlState(ShipControlState {});
    std::cerr << "[PadLanding] ship " << shipId.value << " pad service, pad " << address << " of " << frameModuleId << " from phase "
              << phase << (p.autoLanding ? " (automatic pad)" : " (hand-flown pad)") << " (pad " << building->padPosition.x << ","
              << building->padPosition.y << "," << building->padPosition.z << ", transit " << p.transitLocal << " m)" << std::endl;
    return true;
}

void GameSimulation::updatePadLandings(double dt)
{
    if (dt <= 0.0)
        return;
    std::vector<EntityId> drop;
    for (auto& [id, p] : m_padLandings)
    {
        Ship* ship = getShip(id);
        const StaticObject* city = findCityFrame(m_staticObjects, p.frameModuleId);
        if (!ship || !city)
        {
            drop.push_back(id);
            continue;
        }
        auto& tr = ship->core().transform();
        if (tr.motion.systemId != m_activeCelestialSystemId)
            continue;   // another system's pass
        const glm::dmat3 R(glm::mat3(city->orientation));
        const glm::dvec3 cpos = world::coordinates::fullMeters(city->worldPosition);
        const glm::dvec3 cvel(city->linearVelocity);
        // where the ship is, in the city frame
        // the service keeps the ship's place in the city frame itself: taken once, on its first tick here (this stage has
        // the free ships re-materialized at the hub objects' epoch), then only integrated - never read back (read back
        // it fed the epoch step into itself and blew up / stood still, live 2026-11-21)
        static std::unordered_map<std::uint64_t, glm::dvec3> held;
        static std::unordered_map<std::uint64_t, double> heldSince;   // a new request re-takes the place
        auto heldIt = held.find(id.value);
        if (heldIt == held.end() || p.sinceS > heldSince[id.value])
        {
            heldSince[id.value] = p.sinceS;
            heldIt = held.insert_or_assign(id.value, glm::transpose(R) * (tr.fullWorldMeters() - cpos)).first;
        }
        glm::dvec3& q = heldIt->second;
        const glm::dvec3& out = p.outLocal;
        const auto& dims = ship->core().desc().logicalDimensions();
        const double clearance = (dims.enabled && dims.height > 0.0 ? 0.5 * dims.height : 3.0) + 0.5;
        const glm::dvec3 touchdown = p.padLocal + out * clearance;
        glm::dvec3 target;
        double cruise = 0.0;
        switch (p.phase)
        {
            case 0: target = q + out * (p.transitLocal - glm::dot(q, out)); cruise = 150.0; break;    // up / down
            case 1: target = p.padLocal + out * (p.transitLocal - glm::dot(p.padLocal, out)); cruise = 150.0; break;   // across
            case 2:   // down: onto the pad (an automatic pad); a hand-flown one: to the top of its zone, then the pilot
                target = p.autoLanding ? touchdown : p.padLocal + out * game::navigation::HubPadZoneHeight;
                cruise = p.autoLanding ? 6.0 : 30.0;
                break;
            default: target = touchdown; cruise = 0.0; break;                                           // landed
        }
        const glm::dvec3 to = target - q;
        const double dist = glm::length(to);
        if (serverTime() - p.logS >= 10.0)
        {
            p.logS = serverTime();
            std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << " phase " << p.phase << ": " << static_cast<long long>(dist)
                      << " m to go at " << static_cast<long long>(p.speedMps) << " m/s, city frame (" << static_cast<long long>(q.x) << ","
                      << static_cast<long long>(q.y) << "," << static_cast<long long>(q.z) << ")" << std::endl;
        }
        // the speed: 8 m/s^2 up, braking-limited to stop at the target; the last 30 m of the descent at 2 m/s
        const double brake = std::sqrt(2.0 * 8.0 * dist);
        double want = std::min(cruise, brake);
        if (p.phase == 2 && dist < 30.0)
            want = std::min(want, 2.0);
        // an automatic pad lands a ship only on its legs (author 2026-11-21: «их надо выпустить, только после этого - посадка»):
        // it holds it over the pad until they are out
        const auto gearIt = m_landingGear.find(id);
        const bool legsOut = gearIt != m_landingGear.end() && gearIt->second == 1;
        const bool waitLegs = p.phase == 2 && p.autoLanding && !legsOut;
        if (waitLegs)
        {
            want = 0.0;
            if (serverTime() - p.gearNagS >= 15.0)
            {
                p.gearNagS = serverTime();
                m_padNotices[id] = {"pad:gear_up", serverTime()};
            }
        }
        p.speedMps = std::clamp(want, p.speedMps - 8.0 * dt, p.speedMps + 8.0 * dt);
        glm::dvec3 vLocal(0.0);
        if (p.phase < 3)
        {
            if (waitLegs)
                p.speedMps = 0.0;
            const double step = p.speedMps * dt;
            if (dist <= std::max(0.3, step))
            {
                q = target;
                const int next = p.phase + 1;
                std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": "
                          << (next == 1 ? "at the transit height, across" : next == 2 ? "over the pad, descending" : p.autoLanding ? "LANDED" : "at its zone top")
                          << " (" << static_cast<long long>(serverTime() - p.sinceS) << " s)" << std::endl;
                p.phase = next;
                if (p.phase == 2 || p.phase == 3)
                    p.speedMps = 0.0;
                if (p.phase == 3 && !p.autoLanding)
                {
                    // a hand-flown pad: brought over its zone, the pilot lands
                    m_padNotices[id] = {"pad:manual:" + p.address, serverTime()};
                    drop.push_back(id);
                    std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": hand-flown pad - handed over at its zone"
                              << std::endl;
                }
            }
            else
            {
                vLocal = to / dist * p.speedMps;
                q += vLocal * dt;
            }
        }
        // the attitude: level with the pad (up = its normal), the nose along the way across, kept on the descent
        glm::dvec3 forward = glm::transpose(R) * glm::dvec3(-glm::dvec3(glm::mat3(tr.orientation)[2]));
        const glm::dvec3 flat = vLocal - out * glm::dot(vLocal, out);
        if (glm::length(flat) > 1.0)
            forward = flat;
        forward -= out * glm::dot(forward, out);
        forward = glm::length(forward) > 1.0e-6 ? glm::normalize(forward) : glm::dvec3(0.0, 0.0, -1.0);
        const glm::dvec3 right = glm::normalize(glm::cross(forward, out));
        glm::mat4 orientation(1.0f);
        orientation[0] = glm::vec4(glm::vec3(R * right), 0.0f);
        orientation[1] = glm::vec4(glm::vec3(R * out), 0.0f);
        orientation[2] = glm::vec4(glm::vec3(R * -forward), 0.0f);
        tr.orientation = orientation;
        const glm::dvec3 position = cpos + R * q;
        const glm::dvec3 velocity = cvel + R * vLocal;
        {
            static int nanLogs = 0;
            const auto bad = [](const glm::dvec3& v) { return !std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z); };
            if ((bad(position) || bad(velocity) || bad(forward) || bad(right)) && nanLogs++ < 5)
                std::cerr << "[PadLanding] NaN: q " << q.x << "," << q.y << "," << q.z << " vLocal " << vLocal.x << "," << vLocal.y << ","
                          << vLocal.z << " cvel " << cvel.x << "," << cvel.y << "," << cvel.z << " fwd " << forward.x << "," << forward.y << ","
                          << forward.z << " right " << right.x << "," << right.y << "," << right.z << " speed " << p.speedMps << " dist " << dist
                          << std::endl;
        }
        if (const auto* frame = hubNavigationFrame(tr.motion.hubId); frame && frame->valid)
        {
            // as the dock's station control: keep the travel frame on the moving hub (a stale origin at the release
            // put a released ship 96 km off, live 2026-11-18)
            game::navigation::TravelFrameSystem::refreshMatchedReference(tr.motion, frame->kinematicFrame(), frame->hubId);
            const glm::dvec3 local = frame->worldToLocalPosition(position);
            tr.motion.localPositionMeters = local;
            tr.motion.localVelocityMps = frame->worldToLocalVelocity(position, velocity);
            tr.motion.referenceVelocityMps = tr.motion.travelFrame.localToWorldVelocity(local, glm::dvec3(0.0));
            tr.referenceVelocityMetersPerSecond = tr.motion.referenceVelocityMps;
        }
        tr.motion.worldVelocityMps = velocity;
        tr.setWorldPositionMeters(position);
        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    }
    for (const EntityId id : drop)
        m_padLandings.erase(id);
}

std::string GameSimulation::padServiceStatus(EntityId shipId) const
{
    // a fresh notice first (4 s: the client prints each change once), then the phase while the service flies the ship
    if (const auto n = m_padNotices.find(shipId); n != m_padNotices.end() && serverTime() - n->second.second < 4.0)
        return n->second.first;
    if (const auto p = m_padLandings.find(shipId); p != m_padLandings.end())
        return "phase:" + std::to_string(p->second.phase) + ":" + p->second.address;
    return {};
}

void GameSimulation::toggleLandingGear(EntityId shipId)
{
    auto& gear = m_landingGear[shipId];
    if (gear == 2)
    {
        m_padNotices[shipId] = {"gear:broken", serverTime()};
        return;
    }
    gear = gear == 1 ? 0 : 1;
    m_padNotices[shipId] = {gear == 1 ? "gear:out" : "gear:in", serverTime()};
    std::cerr << "[PadLanding] ship " << shipId.value << " landing legs " << (gear == 1 ? "OUT" : "IN") << std::endl;
}

// The pads near the players (author 2026-11-21): an automatic pad's capture zone (a box HubPadZoneHalf x HubPadZoneHeight
// over its deck) takes a ship flying in with its legs out - the pad's service sets it down; a hand-flown pad counts a
// soft touchdown on the legs (< 2.5 m/s) as landed - held on the deck like the service's; the legs damp up to 4 m/s and
// break above it; no legs: the hull hits the deck (the ordinary contact).
void GameSimulation::updatePadTouchdowns(double dt)
{
    (void)dt;
    const double now = serverTime();
    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr || !isPlayerControlled(id) || !passOwns(shipPtr.get()) || m_padLandings.count(id) != 0 ||
            isDockStationControlledShip(id))
            continue;
        if (const auto r = g_padReleasedAt.find(id.value); r != g_padReleasedAt.end() && now - r->second < 15.0)
            continue;   // just taken back: taking off
        auto& tr = shipPtr->core().transform();
        if (tr.motion.systemId != m_activeCelestialSystemId)
            continue;
        const glm::dvec3 shipWorld = tr.fullWorldMeters();
        const auto& dims = shipPtr->core().desc().logicalDimensions();
        const double clearance = (dims.enabled && dims.height > 0.0 ? 0.5 * dims.height : 3.0) + 0.5;
        const auto gearIt = m_landingGear.find(id);
        const std::uint8_t gear = gearIt == m_landingGear.end() ? 0 : gearIt->second;
        for (const auto& [oid, obj] : m_staticObjects)
        {
            if (obj.type != ObjectType::HubBeltCity || obj.systemId != m_activeCelestialSystemId)
                continue;
            const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(obj.hubModuleId);
            if (!plan)
                continue;
            const glm::dvec3 cpos = world::coordinates::fullMeters(obj.worldPosition);
            if (glm::length(shipWorld - cpos) > 20000.0)
                continue;
            const glm::dmat3 R(glm::mat3(obj.orientation));
            const glm::dvec3 q = glm::transpose(R) * (shipWorld - cpos);
            const glm::dvec3 v = glm::transpose(R) * (tr.motion.worldVelocityMps - glm::dvec3(obj.linearVelocity));
            for (const auto& m : plan->modules())
            {
                if (!m.pad)
                    continue;
                const glm::dvec3 out = m.under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
                const glm::dvec3 d = q - m.padPosition;
                const double flat = std::max(std::abs(d.x), std::abs(d.z));
                const double h = glm::dot(d, out);
                if (flat > game::navigation::HubPadZoneHalf || h < -2.0 || h > game::navigation::HubPadZoneHeight)
                    continue;
                static std::unordered_map<std::uint64_t, double> nagAt;
                const auto nag = [&](const std::string& what) {
                    if (now - nagAt[id.value] >= 15.0)
                    {
                        nagAt[id.value] = now;
                        m_padNotices[id] = {what, now};
                    }
                };
                if (m.autoLanding)
                {
                    // the capture zone: the pad's service sets the ship down (its legs out first)
                    if (gear == 1)
                    {
                        m_padNotices[id] = {"pad:capture:" + m.address, now};
                        (void)startPadService(id, obj.hubModuleId, m.address, 2);
                    }
                    else
                        nag(gear == 2 ? "gear:broken" : "pad:gear_up");
                    break;
                }
                // a hand-flown pad: the touchdown on the deck
                if (flat > 0.5 * game::navigation::HubPadDeckSide || h - clearance > 1.0)
                    break;
                const double speed = glm::length(v);
                if (gear == 1 && speed < 2.5)
                {
                    m_padNotices[id] = {"pad:landed_manual:" + m.address, now};
                    (void)startPadService(id, obj.hubModuleId, m.address, 3);
                    std::cerr << "[PadLanding] ship " << id.value << " LANDED by hand on " << m.address << " at " << speed << " m/s"
                              << std::endl;
                }
                else if (gear == 1 && speed >= 4.0)
                {
                    m_landingGear[id] = 2;
                    m_padNotices[id] = {"gear:broke_hard", now};
                    std::cerr << "[PadLanding] ship " << id.value << " landing legs BROKEN on " << m.address << " at " << speed << " m/s"
                              << std::endl;
                }
                else if (gear != 1)
                    nag(gear == 2 ? "gear:broken" : "pad:no_gear");
                break;
            }
        }
    }
}
