// The interstellar jump, server side (author 2026-11-08; rules: navigation/rules/JumpRules.h).
// requestJump: the checks, then a silent charge-up of 5-10 min in normal flight; at its end the
// jump waits until the ship is clear of objects and above every J floor, then is instant: the
// charge is spent, the ship's system id changes (GameServer switches the simulated system and the
// client its sky / bodies / maps on the next tick), and once the new system's bodies are in the
// gravity context the ship is put on a circular orbit of the exit planet (version 1: no hubs in
// other systems yet - the orbit of the planet with the most world sites, else the heaviest).
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace
{
namespace jr = game::navigation::jump;
constexpr double PlaceholderOrbitM = 4.5e11;   // ~3 AU from the new star until its bodies are loaded
}

bool GameSimulation::requestJump(EntityId shipId, int targetSystemId, const std::string& targetName, double distanceLy,
                                 const std::string& exitBodyId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& motion = ship->core().transform().motion;
    const auto refuse = [&](jr::Refusal r) {
        motion.jumpRefusal = static_cast<std::uint8_t>(r);
        ++motion.jumpRefusalSerial;
        std::cerr << "[Jump] ship " << shipId.value << ": jump to " << targetName << " (" << targetSystemId << ", " << distanceLy
                  << " ly) REFUSED: " << jr::refusalText(r) << std::endl;
        return false;
    };
    if (targetSystemId < 0 || targetName.empty())
        return refuse(jr::Refusal::UnknownSystem);
    if (targetSystemId == motion.systemId)
        return refuse(jr::Refusal::SameSystem);
    if (!motion.freeFlight || m_freeFlights.count(shipId) == 0 || m_freeFlights.at(shipId).localFrame)
        return refuse(jr::Refusal::NotInNormalFlight);
    const auto physics = ship->core().effectivePhysics();
    const double massT = physics.massKg / 1000.0;
    if (motion.jChargeT < 0.0)
    {
        motion.jChargeCapacityT = physics.jumpChargeCapacityT;
        motion.jChargeT = motion.jChargeCapacityT;
    }
    if (const auto r = jr::check(distanceLy, massT, physics.jumpCoreRatingT, motion.jChargeT); r != jr::Refusal::None)
        return refuse(r);

    ActiveJump j;
    j.targetSystemId = targetSystemId;
    j.targetName = targetName;
    j.exitBodyId = exitBodyId;
    j.distanceLy = distanceLy;
    j.costT = jr::chargeT(distanceLy, massT);
    j.durationS = jr::chargeUpSeconds(distanceLy);
    if (const char* dev = std::getenv("ELITE_DEV_JUMP_CHARGE_S"); dev && *dev)
        j.durationS = std::max(1.0, std::atof(dev));   // DEV: a short charge-up for the test runs
    m_jumps[shipId] = j;
    motion.jumpState = 1;
    motion.jumpTargetSystemId = targetSystemId;
    motion.jumpTargetName = targetName;
    motion.jumpDistanceLy = distanceLy;
    motion.jumpCostT = j.costT;
    motion.jumpChargeDurationS = j.durationS;
    motion.jumpChargeElapsedS = 0.0;
    motion.jumpRefusal = 0;
    std::cerr << "[Jump] ship " << shipId.value << ": CHARGING for " << targetName << " (system " << targetSystemId << ", "
              << distanceLy << " ly): " << j.durationS << " s, charge " << j.costT << " t of " << motion.jChargeT << " t" << std::endl;
    return true;
}

void GameSimulation::cancelJump(EntityId shipId, std::uint8_t why)
{
    const auto it = m_jumps.find(shipId);
    if (it == m_jumps.end() || it->second.jumped)
        return;
    if (Ship* ship = getShip(shipId))
    {
        auto& motion = ship->core().transform().motion;
        motion.jumpState = 0;
        motion.jumpRefusal = why;
        ++motion.jumpRefusalSerial;
    }
    std::cerr << "[Jump] ship " << shipId.value << ": charge-up for " << it->second.targetName << " stopped: "
              << jr::refusalText(static_cast<jr::Refusal>(why)) << std::endl;
    m_jumps.erase(it);
}

bool GameSimulation::shipAwaitingJumpExit(EntityId shipId) const
{
    const auto it = m_jumps.find(shipId);
    return it != m_jumps.end() && it->second.jumped;
}

void GameSimulation::updateJumps(double dt)
{
    for (auto it = m_jumps.begin(); it != m_jumps.end();)
    {
        const EntityId id = it->first;
        auto& j = it->second;
        Ship* ship = getShip(id);
        if (!ship)
        {
            it = m_jumps.erase(it);
            continue;
        }
        auto& tr = ship->core().transform();
        auto& motion = tr.motion;
        auto& frame = motion.travelFrame;

        if (!j.jumped)
        {
            const auto ff = m_freeFlights.find(id);
            if (!motion.freeFlight || ff == m_freeFlights.end() || ff->second.localFrame)
            {
                ++it;
                cancelJump(id, static_cast<std::uint8_t>(jr::Refusal::Interrupted));
                continue;
            }
            j.elapsedS = std::min(j.durationS, j.elapsedS + dt);
            motion.jumpChargeElapsedS = j.elapsedS;
            if (j.elapsedS < j.durationS)
            {
                ++it;
                continue;
            }
            std::string why;
            if (jEngineBlocked(id, motion.systemId, frame.originMeters, why) != 0)
            {
                if (motion.jumpState != 2)
                {
                    motion.jumpState = 2;
                    motion.jumpRefusal = static_cast<std::uint8_t>(jr::Refusal::NearObjects);
                    ++motion.jumpRefusalSerial;
                    std::cerr << "[Jump] ship " << id.value << ": charged, waiting - " << why << std::endl;
                }
                ++it;
                continue;
            }
            // ---- the jump: instant
            motion.jChargeT = std::max(0.0, motion.jChargeT - j.costT);
            const int from = motion.systemId;
            motion.systemId = j.targetSystemId;
            frame.systemId = j.targetSystemId;   // the replicated frame identity carries the system (the client's)
            motion.hubId.clear();
            motion.parentBodyId.clear();
            motion.freeFlightReferenceId.clear();
            motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
            motion.freeFlightOrbitBodyId.clear();
            motion.jEngine = false;
            motion.jStopping = false;
            motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
            motion.freeFlightThrust = 0;
            frame.originMeters = glm::dvec3(PlaceholderOrbitM, 0.0, 0.0);
            frame.linearVelocityMps = glm::dvec3(0.0);
            frame.linearAccelerationMps2 = glm::dvec3(0.0);
            motion.localPositionMeters = glm::dvec3(0.0);
            motion.localVelocityMps = glm::dvec3(0.0);
            tr.setWorldPositionMeters(frame.originMeters);
            motion.jumpState = 3;
            ++motion.jumpSerial;
            auto& f = m_freeFlights[id];
            f.lastPrimaryId.clear();
            f.primaryBodyId.clear();
            f.autoStabilizeArmed = false;
            j.jumped = true;
            j.waitTicks = 0;
            std::cerr << "[Jump] ship " << id.value << ": JUMP " << from << " -> " << j.targetSystemId << " (" << j.targetName << ", "
                      << j.distanceLy << " ly), charge spent " << j.costT << " t, left " << motion.jChargeT << " t" << std::endl;
            ++it;
            continue;
        }

        // ---- after the jump: wait for the new system's bodies, then the exit orbit
        if (m_activeCelestialSystemId != j.targetSystemId || m_gravityBodies.empty())
        {
            if (++j.waitTicks % 500 == 0)
                std::cerr << "[Jump] ship " << id.value << ": waiting for system " << j.targetSystemId << " (active "
                          << m_activeCelestialSystemId << ")" << std::endl;
            ++it;
            continue;
        }
        const std::string prefix = "system_" + std::to_string(j.targetSystemId) + ".";
        double starMu = 0.0;
        for (const auto& b : m_gravityBodies)
            starMu = std::max(starMu, b.gravitationalParameterM3s2);
        const game::navigation::GravityBody* exit = nullptr;
        if (!j.exitBodyId.empty())
            for (const auto& b : m_gravityBodies)
                if (b.id == j.exitBodyId)
                    exit = &b;
        if (!exit)
        {
            // the planet with the most world sites (where people are), else the heaviest planet
            std::map<std::string, int> sitesOnBody;
            world::celestial::WorldSitesCatalog sites;
            if (sites.loadFromRuntimeOrSource())
                for (const auto& s : sites.sitesInSystem(j.targetSystemId))
                    ++sitesOnBody[s.parentBodyId];
            int bestSites = -1;
            double bestMu = -1.0;
            for (const auto& b : m_gravityBodies)
            {
                if (b.gravitationalParameterM3s2 >= starMu || b.id.rfind(prefix, 0) != 0 || b.radiusMeters < 1.0e6)
                    continue;
                const int n = sitesOnBody.count(b.id) ? sitesOnBody[b.id] : 0;
                if (n > bestSites || (n == bestSites && b.gravitationalParameterM3s2 > bestMu))
                {
                    bestSites = n;
                    bestMu = b.gravitationalParameterM3s2;
                    exit = &b;
                }
            }
        }
        if (!exit)
        {
            // no planet: in space, 1 AU from the star, at rest against it
            for (const auto& b : m_gravityBodies)
                if (b.gravitationalParameterM3s2 >= starMu)
                    exit = &b;
        }
        if (!exit)
        {
            ++it;
            continue;
        }
        // the body's velocity: two samples
        if (j.exitSampleId != exit->id)
        {
            j.exitSampleId = exit->id;
            j.exitLastCentre = exit->centerMeters;
            ++it;
            continue;
        }
        const glm::dvec3 bodyVel = (exit->centerMeters - j.exitLastCentre) / std::max(1.0e-6, dt);
        const bool star = exit->gravitationalParameterM3s2 >= starMu;
        const double floor = game::navigation::rules::floorAltitudeM(exit->radiusMeters, exit->atmosphereRadiusMeters);
        const double r = star ? 1.495978707e11 : exit->radiusMeters + floor + jr::ExitOrbitAboveFloorM;
        // the exit point: on the star's side of the planet (lit), the orbit in the planet's orbital plane
        glm::dvec3 toStar(0.0);
        for (const auto& b : m_gravityBodies)
            if (b.gravitationalParameterM3s2 >= starMu)
                toStar = b.centerMeters - exit->centerMeters;
        glm::dvec3 radial = glm::length(toStar) > 1.0 ? glm::normalize(toStar) : glm::dvec3(1.0, 0.0, 0.0);
        if (star)
            radial = glm::dvec3(1.0, 0.0, 0.0);
        glm::dvec3 normal = glm::cross(radial, bodyVel - glm::dvec3(0.0));
        if (glm::length(normal) < 1.0e-9)
            normal = glm::dvec3(0.0, 1.0, 0.0);
        normal = glm::normalize(normal);
        const glm::dvec3 tangent = glm::normalize(glm::cross(normal, radial));
        frame.originMeters = exit->centerMeters + radial * r;
        frame.linearVelocityMps = bodyVel + (star ? glm::dvec3(0.0) : tangent * std::sqrt(exit->gravitationalParameterM3s2 / r));
        motion.localPositionMeters = glm::dvec3(0.0);
        motion.localVelocityMps = glm::dvec3(0.0);
        tr.setWorldPositionMeters(frame.originMeters);
        // the nose along the orbit
        {
            static const bool devNoseDown = std::getenv("ELITE_DEV_JUMP_NOSE_DOWN") != nullptr;   // DEV: look at the exit body
            const glm::dvec3 fwd = star || devNoseDown ? -radial : tangent;
            const glm::dvec3 up = star || devNoseDown ? tangent : radial;   // never along the nose
            const glm::dvec3 right = glm::cross(fwd, up);
            tr.orientation = glm::mat4(glm::mat3(glm::dmat3(right, up, -fwd)));
            tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
        }
        motion.jumpState = 0;
        auto& f = m_freeFlights[id];
        f.lastPrimaryId.clear();
        f.autoStabilizeArmed = true;   // a closed orbit: the automatic stabilization may take it
        std::cerr << "[Jump] ship " << id.value << ": EXIT in system " << j.targetSystemId << " - "
                  << (star ? "in space, 1 AU from the star" : "orbit of " + exit->id + " at " + std::to_string(static_cast<int>((r - exit->radiusMeters) / 1000.0)) + " km")
                  << std::endl;
        it = m_jumps.erase(it);
    }
}
