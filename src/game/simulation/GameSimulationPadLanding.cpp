// Hub city pads (author 2026-11-21: «каждому модулю города посадочную площадку ... в инфо-карточке адрес, что за
// объект и кнопку - стыковка»; «меню посадки вызывать кликом по модулю»). The city's pad service flies the ship that
// asked for a building's pad: up / down to the transit height over the city (above every building, under the 430 m
// lanes), across to over the pad, down onto it at a walking pace; then it holds it there. Any control input from the
// pilot (LandOnPad index 0, the client's takeover) gives the ship back at once - on the way or on the pad (take-off).
// Kinematic like the dock's station control (the same hub-frame refresh on the moving hub).

#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/HubPlanPads.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include <glm/gtc/quaternion.hpp>

namespace
{
// the pilot took the controls back (server time per ship): no capture / touchdown for a while - he is taking off
std::unordered_map<std::uint64_t, double> g_padReleasedAt;
// released from a pad: no capture again until the ship has been out of every pad zone once (live 2026-11-21: the docking
// lift-off was re-captured and set back down)
std::unordered_set<std::uint64_t> g_padAwaitZoneExit;
// a ship taken back while standing on a pad (author 2026-11-21: «начинаю вращать корабль на платформе - он пробивает кормой
// платформу; если он на опорах - так не должно быть»): its legs stand on the deck - the attitude it landed with is held and
// it may only rise; free once 3 m over the deck (or after a minute)
struct PadTakeoff
{
    std::string frameModuleId;
    glm::dvec3 pad {0.0};
    glm::dvec3 out {0.0, 1.0, 0.0};
    glm::dmat3 attitude {1.0};   // in the city frame
    double since = 0.0;
};
std::unordered_map<std::uint64_t, PadTakeoff> g_padTakeoffs;
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

// the ship is taken into `hubId`'s navigation frame where it is: normal flight (the global integrator) off, the frame's
// local position from its world position, the attitude kept. The pad service and the docking request near a hub in
// normal flight use it (live 2026-11-21: the docking autopilot refused silently - "not matched to target Hub frame").
bool GameSimulation::takeShipIntoHubFrame(EntityId shipId, const std::string& hubId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& tr = ship->core().transform();
    if (!m_freeFlights.count(shipId) && tr.motion.mode == game::navigation::MotionMode::HubTactical && tr.motion.hubId == hubId)
        return true;
    const auto* hubFrame = hubNavigationFrame(hubId);
    if (!hubFrame || !hubFrame->valid)
        return false;
    if (m_freeFlights.count(shipId))
    {
        // left running, the global step flew a held ship into the Earth (live 2026-11-21)
        auto& motion = tr.motion;
        motion.freeFlight = false;
        motion.freeFlightThrust = 0;
        motion.jEngine = false;
        motion.velocityReferenceKind = 0;
        motion.freeFlightReferenceId.clear();
        motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
        motion.freeFlightOrbitBodyId.clear();
        m_freeFlights.erase(shipId);
    }
    game::navigation::ReferenceFrame frame;
    frame.type = game::navigation::ReferenceFrameType::OrbitalHub;
    frame.systemId = tr.motion.systemId;
    frame.hubId = hubId;
    frame.localOffsetMeters = hubFrame->worldToLocalPosition(tr.fullWorldMeters());
    const glm::mat4 orientation = tr.orientation;
    if (!placeShipInReferenceFrame(shipId, frame))
        return false;
    tr.orientation = orientation;
    std::cerr << "[HubFrame] ship " << shipId.value << " taken into hub " << hubId << "'s frame" << std::endl;
    return true;
}

bool GameSimulation::enterHubFrameForModule(EntityId shipId, const std::string& moduleId)
{
    for (const auto& [oid, o] : m_staticObjects)
        if (o.hubModuleId == moduleId && !o.hubId.empty())
        {
            // only near the hub (its traffic space): farther the route to the hub (orbital transfer) comes first
            const Ship* ship = getShip(shipId);
            const auto* hubFrame = hubNavigationFrame(o.hubId);
            if (!ship || !hubFrame || !hubFrame->valid)
                return false;
            const double d = glm::length(hubFrame->worldToLocalPosition(ship->core().transform().fullWorldMeters()));
            if (d > 30000.0)
            {
                std::cerr << "[HubFrame] ship " << shipId.value << ": " << d << " m from hub " << o.hubId << " - too far" << std::endl;
                return false;
            }
            return takeShipIntoHubFrame(shipId, o.hubId);
        }
    std::cerr << "[HubFrame] ship " << shipId.value << ": module " << moduleId << " not found" << std::endl;
    return false;
}

bool GameSimulation::requestPadLanding(EntityId shipId, const std::string& frameModuleId, const std::string& address, bool land)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    if (!land)
    {
        if (const auto held = m_padLandings.find(shipId); held != m_padLandings.end() && held->second.phase == 3)
            for (const auto& [oid, obj] : m_staticObjects)
                if (obj.hubModuleId == held->second.frameModuleId)
                {
                    const glm::dmat3 R(glm::mat3(obj.orientation));
                    g_padTakeoffs[shipId.value] = {held->second.frameModuleId, held->second.padLocal, held->second.outLocal,
                                                   glm::transpose(R) * glm::dmat3(glm::mat3(ship->core().transform().orientation)),
                                                   serverTime()};
                    break;
                }
        if (m_padLandings.erase(shipId) != 0)
        {
            g_padReleasedAt[shipId.value] = serverTime();
            g_padAwaitZoneExit.insert(shipId.value);
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
    if (!refusal)
    {
        const auto caps = game::ship::telemetry::applyAuthorityToControlParams(ship->core().effectivePhysics(),
                                                                                 ship->core().controlAuthorityTelemetry());
        if (std::max(static_cast<double>(caps.strafeAccel),
                     caps.forwardMainEngineAvailable ? 0.5 * caps.forwardMainEngineAccelerationMps2 : 0.0) < 0.2)
            refusal = "no working thrust";
    }
    // a ship near the city but not in its hub's frame (normal flight): the pad service takes it into the frame where
    // it is (the kinematic hold starts from its place; the frame velocity is the hub's)
    const bool freeFlight = m_freeFlights.count(shipId) != 0;
    if (!refusal && (freeFlight || tr.motion.mode != game::navigation::MotionMode::HubTactical || tr.motion.hubId != city->hubId))
    {
        if (!takeShipIntoHubFrame(shipId, city->hubId))
            refusal = "the hub frame is unavailable";
        else
            m_padNotices[shipId] = {freeFlight ? "taken:flight" : "taken:frame", serverTime()};
    }
    if (refusal)
    {
        std::cerr << "[PadLanding] ship " << shipId.value << " pad " << frameModuleId << "/" << address << " refused: " << refusal
                  << std::endl;
        const std::string why = refusal;
        const char* key = why == "no such city" ? "no_city" : why == "no pad at that address" ? "no_pad"
                        : why == "the station flies the ship" ? "station" : why == "farther than 60 km" ? "far" : why == "no working thrust" ? "no_thrust" : "frame";
        m_padNotices[shipId] = {std::string("refused:") + key, serverTime()};
        return false;
    }
    // brought over the pad by the planner and the autopilot (the route to its approach point, author 2026-11-21): the
    // service only flies the last metres across and sets the ship down on its legs; from elsewhere it climbs first
    int phase = 0;
    {
        const glm::dmat3 R(glm::mat3(city->orientation));
        const glm::dvec3 q = glm::transpose(R) * (tr.fullWorldMeters() - world::coordinates::fullMeters(city->worldPosition));
        const glm::dvec3 out = building->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
        glm::dvec3 flat = q - building->padPosition;
        const double height = glm::dot(flat, out);
        flat -= out * height;
        double tallest = 0.0;
        for (const auto& b : plan->blocks())
            tallest = std::max(tallest, building->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
        const double transitAbovePad = std::max({std::min(380.0, tallest + 80.0), std::abs(building->padPosition.y) + 60.0, game::navigation::traffic::laneTubeCeilingMeters(game::navigation::traffic::HubLaneCatalog::forFrameModule(frameModuleId), building->under) + 40.0}) - glm::dot(building->padPosition, out);
        if (glm::length(flat) < 300.0 && height > transitAbovePad - 150.0)
            phase = 1;
    }
    return startPadService(shipId, frameModuleId, address, phase);
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
    // THE DISPATCHER (author 2026-11-22: «если 3 места - все три могут быть заняты. если места все заняты - вход
    // запрещён»): a free place of the module for this ship; none -> refused, "taken" with the expected wait
    const int places = game::navigation::hubBerthPlaces(*building);
    std::vector<char> taken(static_cast<std::size_t>(std::max(places, 1)), 0);
    double longestStay = 0.0;
    for (const auto& [other, op] : m_padLandings)
        if (other != shipId && op.frameModuleId == frameModuleId && op.address == address && op.bay >= 0 &&
            op.bay < static_cast<int>(taken.size()))
        {
            taken[static_cast<std::size_t>(op.bay)] = 1;
            longestStay = std::max(longestStay, serverTime() - op.sinceS);
        }
    int bay = -1;
    for (int b = 0; b < places && bay < 0; ++b)
        if (!taken[static_cast<std::size_t>(b)])
            bay = b;
    if (bay < 0)
    {
        // the wait: a berth is held ~20 min on average; the one there longest frees first
        const int waitMin = std::max(1, static_cast<int>(std::ceil((1200.0 - std::min(longestStay, 1140.0)) / 60.0)));
        m_padNotices[shipId] = {"refused:taken:" + address + ":" + std::to_string(waitMin), serverTime()};
        std::cerr << "[PadLanding] ship " << shipId.value << " pad " << address << " of " << frameModuleId << ": all " << places
                  << " place(s) taken - refused, wait ~" << waitMin << " min" << std::endl;
        return false;
    }
    PadLanding p;
    p.frameModuleId = frameModuleId;
    p.address = address;
    p.bay = bay;
    p.padLocal = game::navigation::hubBerthTouchdown(*building, bay);
    p.outLocal = building->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
    p.autoLanding = building->autoLanding;
    p.hangar = building->padKind == game::navigation::HubPlanModule::PadKind::Hangar;
    if (p.hangar)
    {
        p.autoLanding = true;   // the hangar's crew brings the ship in
        p.doorFrontLocal = game::navigation::hubHangarDoorFront(*building, bay);
        p.bayEntryLocal = game::navigation::hubHangarBayEntry(*building, bay);
    }
    // the transit height: over the tallest building of its tier by 80 m (the city air lanes fly at 430 m)
    double tallest = 0.0;
    for (const auto& b : plan->blocks())
        tallest = std::max(tallest, building->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
    p.transitLocal = std::max({std::min(380.0, tallest + 80.0), std::abs(building->padPosition.y) + 60.0, game::navigation::traffic::laneTubeCeilingMeters(game::navigation::traffic::HubLaneCatalog::forFrameModule(frameModuleId), building->under) + 40.0});
    // a berth at the base reached from a street lane: across at the ship's own height (hubPadServiceTransit)
    if (const StaticObject* cityObj = findCityFrame(m_staticObjects, frameModuleId))
    {
        const glm::dmat3 R(glm::mat3(cityObj->orientation));
        const glm::dvec3 q = glm::transpose(R) * (ship->core().transform().fullWorldMeters() -
                                                  world::coordinates::fullMeters(cityObj->worldPosition));
        const double shipHeight = glm::dot(q, p.outLocal);
        const double low = game::navigation::traffic::hubPadServiceTransit(p.transitLocal, std::abs(building->padPosition.y), shipHeight);
        if (low < p.transitLocal)
        {
            std::cerr << "[PadLanding] ship " << shipId.value << " pad " << address << ": a berth at the base - across at "
                      << static_cast<long long>(low) << " m (the street level), not over the city at "
                      << static_cast<long long>(p.transitLocal) << " m" << std::endl;
            p.transitLocal = low;
        }
    }
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
        // what the ship can still do (author 2026-11-21: «будет ли отказ двигателя влиять на полёт к посадке?»): the service flies
        // it within its working thrust (damage included) - no faster than it could brake, no quicker turns than its RCS
        // allows; with next to no thrust it cannot be landed: the service lets go, the ship coasts
        const auto caps = game::ship::telemetry::applyAuthorityToControlParams(ship->core().effectivePhysics(),
                                                                                 ship->core().controlAuthorityTelemetry());
        const double accel = std::min(8.0, std::max(static_cast<double>(caps.strafeAccel),
                                                    caps.forwardMainEngineAvailable ? 0.5 * caps.forwardMainEngineAccelerationMps2 : 0.0));
        const float turnDegPerS = std::clamp(glm::degrees(static_cast<float>(caps.angularAccel)) * 3.0f, 2.0f, 20.0f);
        if (accel < 0.2 && p.phase < 3)
        {
            m_padNotices[id] = {"pad:no_thrust", serverTime()};
            std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": no working thrust (" << accel
                      << " m/s2) - the service lets go, the ship coasts" << std::endl;
            drop.push_back(id);
            continue;
        }
        const auto& dims = ship->core().desc().logicalDimensions();
        const double clearance = (dims.enabled && dims.height > 0.0 ? 0.5 * dims.height : 3.0) + 0.5;
        const glm::dvec3 touchdown = p.padLocal + out * clearance;
        glm::dvec3 target;
        double cruise = 0.0;
        // the route, once, from where the service took the ship (hubPadRoute: round the tall buildings - author
        // 2026-11-21: «кобра пролетела сквозь башню»)
        if (p.phase <= 1 && p.route.empty())
            if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(p.frameModuleId))
                for (const auto& m : plan->modules())
                    if (m.address == p.address)
                    {
                        p.route = game::navigation::hubPadRoute(*plan, m, q, p.transitLocal, true);
                        p.leg = 1;
                        if (p.route.size() > 2)
                            std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": route round "
                                      << p.route.size() - 2 << " tall building(s)" << std::endl;
                        break;
                    }
        switch (p.phase)
        {
            case 0:   // up / down to the transit height
                target = p.route.empty() ? q + out * (p.transitLocal - glm::dot(q, out)) : p.route.front();
                cruise = 150.0;
                break;
            case 1:   // across, leg by leg (round the tall buildings)
                target = p.leg < p.route.size() ? p.route[p.leg] : p.padLocal + out * (p.transitLocal - glm::dot(p.padLocal, out));
                cruise = 150.0;
                break;
            case 2:   // down: onto the pad (an automatic pad); a hand-flown one: to the top of its zone, then the pilot
                target = p.autoLanding ? touchdown : p.padLocal + out * game::navigation::HubPadZoneHeight;
                cruise = p.autoLanding ? 6.0 : 30.0;
                if (p.hangar)   // a hangar: down in front of its door, in through it, then down onto the bay's lift
                {
                    target = p.hangarStep == 0 ? p.doorFrontLocal : p.hangarStep == 1 ? p.bayEntryLocal : touchdown;
                    cruise = p.hangarStep == 0 ? 12.0 : p.hangarStep == 1 ? 5.0 : 3.0;
                }
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
        // ... a corner of the way round the towers (phase 1, more legs ahead) is not a stop: flown at CornerMps, the full
        // stop only over the pad (stopping at every corner made 5.5 km take 137 s, live 2026-11-22)
        constexpr double CornerMps = 40.0;
        const bool cornerAhead = p.phase == 1 && p.leg + 1 < p.route.size();
        const double brake = std::sqrt(2.0 * accel * dist + (cornerAhead ? CornerMps * CornerMps : 0.0));
        double want = std::min(cruise, brake);
        if (p.phase == 2 && dist < 30.0)
            want = std::min(want, 2.0);
        // an automatic pad lands a ship only on its legs (author 2026-11-21: «их надо выпустить, только после этого - посадка»):
        // it holds it over the pad until they are out
        const auto gearIt = m_landingGear.find(id);
        const bool legsOut = gearIt != m_landingGear.end() && gearIt->second == 1;
        const bool waitLegs = p.phase == 2 && p.autoLanding && !legsOut && (!p.hangar || p.hangarStep >= 2);
        if (waitLegs)
        {
            want = 0.0;
            if (serverTime() - p.gearNagS >= 15.0)
            {
                p.gearNagS = serverTime();
                m_padNotices[id] = {"pad:gear_up", serverTime()};
            }
        }
        p.speedMps = std::clamp(want, p.speedMps - accel * dt, p.speedMps + accel * dt);
        glm::dvec3 vLocal(0.0);
        if (p.phase < 3)
        {
            if (waitLegs)
                p.speedMps = 0.0;
            const double step = p.speedMps * dt;
            if (dist <= std::max(0.3, step))
            {
                q = target;
                if (p.phase == 1 && p.leg + 1 < p.route.size())
                {
                    ++p.leg;   // the next detour corner: keep the speed, turn the corner
                    continue;
                }
                if (p.phase == 2 && p.hangar && p.hangarStep < 2)
                {
                    ++p.hangarStep;
                    p.speedMps = 0.0;
                    std::cerr << "[PadLanding] ship " << id.value << " hangar " << p.address << ": "
                              << (p.hangarStep == 1 ? "at its door, flying in" : "over the bay, setting down") << std::endl;
                    continue;
                }
                const int next = p.phase + 1;
                std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": "
                          << (next == 1 ? "at the transit height, across" : next == 2 ? "over the pad, descending" : p.autoLanding ? "LANDED" : "at its zone top")
                          << " (" << static_cast<long long>(serverTime() - p.sinceS) << " s)" << std::endl;
                p.phase = next;
                if (p.phase == 3)
                {
                    p.touchdownS = serverTime();          // the legs take the ship: the damped sag below
                    p.touchdownMps = std::max(0.5, p.speedMps);
                }
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
        // turned there by its thrusters, not snapped (author 2026-11-21: «очень резкий разворот ... не через управление
        // кораблём, а тупой подачей нового угла»): at most 20 deg/s, easing in the last 10 deg
        {
            const glm::quat from = glm::quat_cast(glm::mat3(tr.orientation));
            glm::quat to = glm::quat_cast(glm::mat3(orientation));
            if (glm::dot(from, to) < 0.0f)
                to = -to;
            const float angle = 2.0f * std::acos(std::clamp(std::abs(glm::dot(from, to)), 0.0f, 1.0f));
            const float maxStep = glm::radians(turnDegPerS) * static_cast<float>(dt) * std::clamp(angle / glm::radians(10.0f), 0.15f, 1.0f);
            const glm::quat turned = angle <= maxStep ? to : glm::slerp(from, to, maxStep / angle);
            tr.orientation = glm::mat4_cast(glm::normalize(turned));
        }
        // the touchdown on the damped legs (author 2026-11-21: «при посадке на лапы корабль должен слегка сдемпфировать.
        // просесть на лапах, а потом стать ровно»): the legs a spring + damper - the hull sinks with the touchdown speed
        // (2 m/s -> ~0.25 m), rocks a little nose-down / up and settles level in ~2 s
        glm::dvec3 sagLocal(0.0);
        if (p.phase == 3 && p.touchdownS > 0.0)
        {
            constexpr double Omega = 5.0, Zeta = 0.45;
            const double t = serverTime() - p.touchdownS;
            const double wd = Omega * std::sqrt(1.0 - Zeta * Zeta);
            const double decay = std::exp(-Zeta * Omega * t);
            const double x = p.touchdownMps / wd * decay * std::sin(wd * t);                       // the compression, m
            const double xv = p.touchdownMps * decay * (std::cos(wd * t) - Zeta * Omega / wd * std::sin(wd * t));
            if (t < 4.0)
            {
                sagLocal = -out * x;
                vLocal += -out * xv;
                // the rock: applied as its change since the last tick (the attitude persists between ticks)
                const double pitch = glm::radians(0.8) * (p.touchdownMps / 2.0) * decay * std::sin(wd * t);
                const glm::vec3 axis = glm::normalize(glm::vec3(tr.orientation[0]));                  // the hull's right
                tr.orientation = glm::rotate(glm::mat4(1.0f), static_cast<float>(pitch - p.touchdownPitch), axis) * tr.orientation;
                p.touchdownPitch = pitch;
            }
        }
        const glm::dvec3 position = cpos + R * (q + sagLocal);
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
        // taking off from a pad: the legs stand on the deck - the landing attitude held, no turning, until 3 m up
        if (const auto t = g_padTakeoffs.find(id.value); t != g_padTakeoffs.end())
        {
            auto& tr = shipPtr->core().transform();
            const StaticObject* city = nullptr;
            for (const auto& [oid, obj] : m_staticObjects)
                if (obj.hubModuleId == t->second.frameModuleId)
                    city = &obj;
            const auto& dims = shipPtr->core().desc().logicalDimensions();
            const double clearance = (dims.enabled && dims.height > 0.0 ? 0.5 * dims.height : 3.0) + 0.5;
            if (!city || tr.motion.systemId != m_activeCelestialSystemId)
                g_padTakeoffs.erase(t);
            else
            {
                const glm::dmat3 R(glm::mat3(city->orientation));
                const glm::dvec3 q = glm::transpose(R) * (tr.fullWorldMeters() - world::coordinates::fullMeters(city->worldPosition));
                const double h = glm::dot(q - t->second.pad, t->second.out) - clearance;
                if (h > 3.0 || now - t->second.since > 60.0)
                {
                    std::cerr << "[PadLanding] ship " << id.value << " off the pad (" << h << " m up): free to turn" << std::endl;
                    g_padTakeoffs.erase(t);
                }
                else
                {
                    tr.orientation = glm::mat4(glm::mat3(R * t->second.attitude));
                    tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
                }
            }
        }
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
        bool inZone = false;
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
                inZone = true;
                if (m.autoLanding && g_padAwaitZoneExit.count(id.value) != 0)
                    break;   // just released from a pad: not taken again before it has left the zone
                if (m.autoLanding)
                {
                    // the capture zone: the pad's service sets the ship down (its legs out first) - only a ship coming
                    // DOWN to it: one taking off / hovering is not grabbed again (live 2026-11-21: the docking lift-off
                    // from a pad was re-captured and set back down every 15 s)
                    if (glm::dot(v, out) > -0.3 || glm::length(v) > 15.0)   // ... and slow: a route passing over it at speed is not landing (live 2026-11-22: B4-18 grabbed a ship on its way to B2-04)
                        break;
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
                if (gear == 1 && speed < game::navigation::HubPadTouchdownMaxMps)
                {
                    m_padNotices[id] = {"pad:landed_manual:" + m.address, now};
                    if (startPadService(id, obj.hubModuleId, m.address, 3))
                        if (const auto landed = m_padLandings.find(id); landed != m_padLandings.end())
                        {
                            landed->second.touchdownS = now;   // the damped sag from the real touchdown speed
                            landed->second.touchdownMps = std::max(0.3, speed);
                        }
                    std::cerr << "[PadLanding] ship " << id.value << " LANDED by hand on " << m.address << " at " << speed << " m/s"
                              << std::endl;
                }
                else if (gear == 1 && speed >= game::navigation::HubPadLegsBreakMps)
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
        if (!inZone)
            g_padAwaitZoneExit.erase(id.value);
    }
}

std::vector<std::string> GameSimulation::cityOccupiedBerths(EntityId cityId) const
{
    std::vector<std::string> out;
    const auto it = m_staticObjects.find(cityId);
    if (it == m_staticObjects.end())
        return out;
    for (const auto& [ship, p] : m_padLandings)
        if (p.frameModuleId == it->second.hubModuleId)
            out.push_back(p.address + "#" + std::to_string(p.bay) + "@" + std::to_string(ship.value) + (p.phase < 3 ? "!" : ""));
    std::sort(out.begin(), out.end());
    return out;
}
