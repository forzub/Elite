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
#include "src/game/navigation/TravelFrameSystem.h"

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
        }
    }
    if (refusal)
    {
        std::cerr << "[PadLanding] ship " << shipId.value << " pad " << frameModuleId << "/" << address << " refused: " << refusal
                  << std::endl;
        return false;
    }
    PadLanding p;
    p.frameModuleId = frameModuleId;
    p.address = address;
    p.padLocal = building->padPosition;
    p.outLocal = building->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
    // the transit height: over the tallest building of its tier by 80 m (the city air lanes fly at 430 m)
    double tallest = 0.0;
    for (const auto& b : plan->blocks())
        tallest = std::max(tallest, building->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
    p.transitLocal = std::min(380.0, tallest + 80.0);
    p.sinceS = serverTime();
    m_padLandings[shipId] = p;
    ship->setControlState(ShipControlState {});
    std::cerr << "[PadLanding] ship " << shipId.value << " cleared for pad " << address << " of " << frameModuleId
              << " (pad " << building->padPosition.x << "," << building->padPosition.y << "," << building->padPosition.z
              << ", transit " << p.transitLocal << " m)" << std::endl;
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
            case 2: target = touchdown; cruise = 6.0; break;                                            // down
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
        p.speedMps = std::clamp(want, p.speedMps - 8.0 * dt, p.speedMps + 8.0 * dt);
        glm::dvec3 vLocal(0.0);
        if (p.phase < 3)
        {
            const double step = p.speedMps * dt;
            if (dist <= std::max(0.3, step))
            {
                q = target;
                const int next = p.phase + 1;
                std::cerr << "[PadLanding] ship " << id.value << " pad " << p.address << ": "
                          << (next == 1 ? "at the transit height, across" : next == 2 ? "over the pad, descending" : "LANDED")
                          << " (" << static_cast<long long>(serverTime() - p.sinceS) << " s)" << std::endl;
                p.phase = next;
                if (p.phase == 2 || p.phase == 3)
                    p.speedMps = 0.0;
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
