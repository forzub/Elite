// GameSimulation: the scheduled traffic of the hub cities on the server side (author 2026-11-18: «расписание, которое
// будет лежать и на сервере и на клиенте ... что серверу надо знать - игрок нападает на корабль. тот становится
// настоящим и сервер начинает его симуляцию»). The schedule (assets/data/navigation/hub_schedules/<hub>.json) and the
// visit poses (game::navigation::traffic::HubTrafficSchedule) are shared with the clients; the server keeps no state
// for a visit until a weapon meets it - then it spawns that visit's catalog ship where the client draws it.

#include "src/game/simulation/GameSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/traffic/HubTrafficSchedule.h"
#include "src/game/ship/ShipDescriptorRegistry.h"

namespace
{
bool isCityModule(const std::string& moduleId)
{
    return moduleId.size() > 5 && moduleId.compare(moduleId.size() - 5, 5, ".city") == 0;
}
} // namespace

bool GameSimulation::castScheduledVisits(const glm::dvec3& startW, const glm::dvec3& endW, double dt, ScheduledVisitHit& out) const
{
    namespace tr = game::navigation::traffic;
    bool any = false;
    for (const auto& [oid, obj] : m_staticObjects)
    {
        if (obj.type != ObjectType::HubBeltCity || !isCityModule(obj.hubModuleId))
            continue;
        const tr::HubTrafficSchedule* schedule = tr::HubTrafficScheduleCatalog::forFrameModule(obj.hubModuleId);
        if (!schedule)
            continue;
        // the segment in the city frame; the city moves over the step: the start against where it was dt ago
        const glm::dvec3 cpos = world::coordinates::fullMeters(obj.worldPosition);
        const glm::dvec3 cvel(obj.linearVelocity);
        const glm::dmat3 R(glm::mat3(obj.orientation));
        const glm::dvec3 a = glm::transpose(R) * (startW - (cpos - cvel * dt));
        const glm::dvec3 b = glm::transpose(R) * (endW - cpos);
        const glm::dvec3 seg = b - a;
        const double len = glm::length(seg);
        if (len < 1.0e-6)
            continue;
        const auto materialized = m_materializedVisits.find(oid.value);
        const auto& visits = schedule->visits();
        for (std::size_t v = 0; v < visits.size(); ++v)
        {
            if (materialized != m_materializedVisits.end() &&
                std::find(materialized->second.begin(), materialized->second.end(), static_cast<std::uint32_t>(v)) !=
                    materialized->second.end())
                continue;
            game::navigation::traffic::HubVisitPose pose;
            if (!schedule->pose(visits[v], m_orbitalUniverseTimeSeconds, pose))
                continue;
            // the hull as a sphere of half its length
            const double radius = std::max(5.0, visits[v].lengthMeters * 0.5);
            const double s = std::clamp(glm::dot(pose.position - a, seg) / (len * len), 0.0, 1.0);
            if (glm::length(a + seg * s - pose.position) > radius)
                continue;
            const double t = s * len;
            if (any && t >= out.t)
                continue;
            any = true;
            out.city = oid;
            out.visit = static_cast<int>(v);
            out.t = t;
            out.pointW = cpos + R * (a + seg * s);
        }
    }
    return any;
}

EntityId GameSimulation::materializeScheduledVisit(const ScheduledVisitHit& hit)
{
    namespace tr = game::navigation::traffic;
    const auto it = m_staticObjects.find(hit.city);
    if (it == m_staticObjects.end() || hit.visit < 0)
        return {};
    const StaticObject& city = it->second;
    const tr::HubTrafficSchedule* schedule = tr::HubTrafficScheduleCatalog::forFrameModule(city.hubModuleId);
    if (!schedule || hit.visit >= static_cast<int>(schedule->visits().size()))
        return {};
    auto& done = m_materializedVisits[hit.city.value];
    if (std::find(done.begin(), done.end(), static_cast<std::uint32_t>(hit.visit)) != done.end())
        return {};
    const tr::HubScheduleVisit& visit = schedule->visits()[static_cast<std::size_t>(hit.visit)];
    tr::HubVisitPose pose;
    if (!schedule->pose(visit, m_orbitalUniverseTimeSeconds, pose))
        return {};
    // where the clients draw it: the city frame -> the world; its motion along the schedule this second
    const glm::dmat3 R(glm::mat3(city.orientation));
    const glm::dvec3 cpos = world::coordinates::fullMeters(city.worldPosition);
    const glm::dvec3 position = cpos + R * pose.position;
    tr::HubVisitPose ahead;
    glm::dvec3 localVel(0.0);
    if (schedule->pose(visit, m_orbitalUniverseTimeSeconds + 1.0, ahead))
        localVel = ahead.position - pose.position;
    ShipInitData init;
    init.visual.shipName = visit.name;
    init.registry.instanceId = 980000 + m_nextCombatSerial++;
    init.registry.ownerName = "scheduled traffic " + visit.id;
    init.registry.shipRole = ShipRoleType::Passenger;   // a scheduled line: corporate upkeep
    const ShipDescriptor& descriptor = ShipDescriptorRegistry::resolve(ObjectType::CobraMk1, visit.passport);
    const glm::mat4 orientation(glm::mat3(R * pose.basis));
    const EntityId id = spawnShip(ShipRole::NPC, city.systemId, descriptor, position, init, orientation);
    Ship* ship = id.value ? getShip(id) : nullptr;
    if (!ship)
        return {};
    done.push_back(static_cast<std::uint32_t>(hit.visit));
    // into its hub's frame at the visit's world place: the hub navigation frame has its own axes (not the city's) -
    // the world point goes through that frame (a first try used the city axes: the ship appeared 6.7 km off)
    game::navigation::ReferenceFrame frame;
    frame.type = game::navigation::ReferenceFrameType::OrbitalHub;
    frame.systemId = city.systemId;
    frame.hubId = city.hubId;
    const auto* hubFrame = city.hubId.empty() ? nullptr : hubNavigationFrame(city.hubId);
    bool placed = false;
    if (hubFrame && hubFrame->valid)
    {
        const game::navigation::KinematicFrame K = hubFrame->kinematicFrame();
        frame.localOffsetMeters = K.worldToLocalPosition(position);
        placed = placeShipInReferenceFrame(id, frame);
        if (placed)
            ship->core().transform().motion.localVelocityMps = K.worldToLocalVector(R * localVel);
    }
    auto& t = ship->core().transform();
    t.orientation = orientation;
    (void)promoteShipToDetailedSimulation(id);
    std::cerr << "[ScheduledTraffic] visit " << visit.id << " (" << visit.name << ", " << visit.kind << ") attacked -> real ship "
              << id.value << (placed ? " in hub " + city.hubId : std::string(" (no hub frame)")) << " at city point "
              << pose.position.x << "," << pose.position.y << "," << pose.position.z << ", phase " << pose.phase << std::endl;
    return id;
}

std::vector<std::uint32_t> GameSimulation::cityMaterializedVisits(EntityId objectId) const
{
    const auto it = m_materializedVisits.find(objectId.value);
    return it == m_materializedVisits.end() ? std::vector<std::uint32_t> {} : it->second;
}
