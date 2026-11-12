// The interstellar jump, server side (author 2026-11-08; rules: navigation/rules/JumpRules.h).
// requestJump: the checks, then a silent charge-up of 5-10 min in normal flight; at its end the
// jump waits until the ship is clear of objects and above every J floor, then is instant: the
// charge is spent, the ship's system id changes (GameServer switches the simulated system and the
// client its sky / bodies / maps on the next tick), and once the new system's bodies are in the
// gravity context the ship is put on a circular orbit of the exit planet (version 1: no hubs in
// other systems yet - the orbit of the planet with the most world sites, else the heaviest).
#include "src/game/simulation/JFieldIntegrity.h"
#include "src/game/simulation/GameSimulation.h"

#include <limits>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <random>

#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace
{
namespace jr = game::navigation::jump;
constexpr double PlaceholderOrbitM = 4.5e11;   // ~3 AU from the new star until its bodies are loaded

// "the system has a beacon" for the exit error's lock (version 1: any world site in it - no beacon
// runtime yet, JumpRules.h exitLockFactor)
bool systemHasSites(int systemId)
{
    static world::celestial::WorldSitesCatalog sites;
    static const bool loaded = sites.loadFromRuntimeOrSource();
    return loaded && systemId >= 0 && !sites.sitesInSystem(systemId).empty();
}
}

bool GameSimulation::requestJump(EntityId shipId, int targetSystemId, const std::string& targetName, double distanceLy,
                                 const std::string& exitBodyId, const glm::dvec3& interstellarDirWorld)
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
    const bool inSystem = targetSystemId == motion.systemId;
    if (inSystem && exitBodyId.empty())
        return refuse(jr::Refusal::SameSystem);
    if (!motion.freeFlight || m_freeFlights.count(shipId) == 0 || m_freeFlights.at(shipId).localFrame)
        return refuse(jr::Refusal::NotInNormalFlight);
    if (!game::navigation::jdrive::jumpFieldComplete(game::simulation::jFieldIntegrity(
            static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()))))
        return refuse(jr::Refusal::FieldIncomplete);
    const auto physics = ship->core().effectivePhysics();
    const double massT = physics.massKg / 1000.0;
    if (motion.jChargeT < 0.0)
    {
        motion.jChargeCapacityT = physics.jumpChargeCapacityT;
        motion.jChargeT = motion.jChargeCapacityT;
    }
    ActiveJump j;
    j.inSystem = inSystem;
    j.targetSystemId = targetSystemId;
    j.targetName = targetName;
    j.exitBodyId = exitBodyId;
    const glm::dvec3 shipM = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    if (inSystem)
    {
        // the in-system jump: straight steps to a body of this system; a step through the star's capture sphere or a
        // body (with its J floor) goes around it through an intermediate point (JUMP_MECHANICS.md 10b)
        const game::navigation::GravityBody* target = nullptr;
        const game::navigation::GravityBody* star = nullptr;
        for (const auto& b : m_gravityBodies)
        {
            if (b.id == exitBodyId)
                target = &b;
            if (!star || b.gravitationalParameterM3s2 > star->gravitationalParameterM3s2)
                star = &b;
        }
        if (!target || !star || target == star)
            return refuse(jr::Refusal::UnknownBody);
        j.starId = star->id;
        if (const auto r = jr::checkInSystem(shipM, target->centerMeters, star->centerMeters, 0.0, massT, physics.jumpCoreRatingT,
                                             std::numeric_limits<double>::max());
            r == jr::Refusal::TooClose || r == jr::Refusal::CoreOverloaded)
            return refuse(r);
        // the start zone first (the departure body is its problem: the zone sees past it), then the way around from
        // there: up to two intermediate points beside whatever else blocks the line (the star, another body)
        if (!planJumpZone(shipId, j, shipM, target->centerMeters, false))
            return refuse(jr::Refusal::LineBlocked);
        glm::dvec3 from = shipM;
        for (const auto& b : m_gravityBodies)
            if (b.id == j.zoneBodyId)
                from = b.centerMeters + j.zoneOffsetM;
        if (j.zoneBodyId.empty())
            from = j.zoneOffsetM;
        const std::string departureBody = j.zoneBodyId == star->id ? std::string() : j.zoneBodyId;
        for (int pass = 0; pass < 3; ++pass)
        {
            const glm::dvec3 legFrom = j.waypointsStarRel.empty() ? from : star->centerMeters + j.waypointsStarRel.back();
            const std::string blocker = jumpLineBlockedBy(legFrom, target->centerMeters, target->id, departureBody);
            if (blocker.empty())
                break;
            if (pass == 2)
                return refuse(jr::Refusal::LineBlocked);
            const game::navigation::GravityBody* b = nullptr;
            for (const auto& g : m_gravityBodies)
                if (g.id == blocker)
                    b = &g;
            if (!b)
                return refuse(jr::Refusal::LineBlocked);
            const double sphere = b == star ? std::max(jr::starCaptureRadiusM(star->gravitationalParameterM3s2), b->radiusMeters)
                                            : b->radiusMeters + game::navigation::rules::floorAltitudeM(b->radiusMeters, b->atmosphereRadiusMeters);
            const glm::dvec3 line = glm::normalize(target->centerMeters - legFrom);
            glm::dvec3 perp = (legFrom - b->centerMeters) - line * glm::dot(legFrom - b->centerMeters, line);
            if (glm::length(perp) < 1.0)
                perp = glm::cross(line, glm::dvec3(0.0, 1.0, 0.0));
            const glm::dvec3 w = b->centerMeters + glm::normalize(perp) * sphere * 2.5;
            j.waypointsStarRel.push_back(w - star->centerMeters);
        }
        // the whole way's charge from the zone (every step pays its own field tear)
        glm::dvec3 a = from;
        double total = 0.0, length = 0.0;
        for (const auto& w : j.waypointsStarRel)
        {
            const glm::dvec3 p = star->centerMeters + w;
            total += jr::inSystemChargeT(glm::length(p - a) / jr::AuM, massT);
            length += glm::length(p - a);
            a = p;
        }
        total += jr::inSystemChargeT(glm::length(target->centerMeters - a) / jr::AuM, massT);
        length += glm::length(target->centerMeters - a);
        if (motion.jChargeT + 1.0e-9 < total)
            return refuse(jr::Refusal::NotEnoughCharge);
        j.totalCostT = total;
        j.distanceAu = length / jr::AuM;
        distanceLy = length / jr::MetersPerLy;
    }
    else
    {
        if (const auto r = jr::check(distanceLy, massT, physics.jumpCoreRatingT, motion.jChargeT); r != jr::Refusal::None)
            return refuse(r);
        j.interstellarDir = glm::length(interstellarDirWorld) > 1.0e-12 ? glm::normalize(interstellarDirWorld) : glm::dvec3(1.0, 0.0, 0.0);
        j.totalCostT = jr::chargeT(distanceLy, massT);
    }
    j.distanceLy = distanceLy;
    // the start zone for the first step (the ship may already be in one); the step's cost and charge-up from there
    // (recomputed when the nose is on the line, JumpProcedure phase 1 -> 2)
    if (!planJumpZone(shipId, j, shipM, jumpLegEnd(j, shipM)))
        return refuse(jr::Refusal::LineBlocked);
    glm::dvec3 zoneM = j.zoneOffsetM;
    for (const auto& b : m_gravityBodies)
        if (b.id == j.zoneBodyId)
            zoneM = b.centerMeters + j.zoneOffsetM;
    j.costT = inSystem ? jumpLegCostT(j, zoneM, massT) : j.totalCostT;
    j.durationS = inSystem ? jr::inSystemChargeUpSeconds(glm::length(jumpLegEnd(j, zoneM) - zoneM) / jr::AuM)
                           : jr::chargeUpSeconds(distanceLy);
    if (const char* dev = std::getenv("ELITE_DEV_JUMP_CHARGE_S"); dev && *dev)
        j.durationS = std::max(1.0, std::atof(dev));   // DEV: a short charge-up for the test runs
    motion.jEngine = false;
    motion.jStopping = false;
    m_jumps[shipId] = j;
    motion.jumpState = j.phase == 0 ? 4 : 5;   // 4 to the start zone, 5 aiming, 1 charging
    motion.jumpTargetSystemId = targetSystemId;
    motion.jumpTargetName = targetName;
    motion.jumpDistanceLy = distanceLy;
    motion.jumpCostT = j.costT;
    motion.jumpChargeDurationS = j.durationS;
    motion.jumpChargeElapsedS = 0.0;
    motion.jumpRefusal = 0;
    std::cerr << "[Jump] ship " << shipId.value << ": PROCEDURE for " << targetName << " (system " << targetSystemId << ", "
              << distanceLy << " ly, " << j.waypointsStarRel.size() + 1 << " step(s)): zone " << (j.phase == 0 ? "to reach" : "here")
              << " (" << j.zoneBodyId << "), charge-up " << j.durationS << " s, step " << j.costT << " t, whole way " << j.totalCostT
              << " t of " << motion.jChargeT << " t" << std::endl;
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
        // the charge drawn into the core is lost (a failed jump costs too - author 2026-11-12)
        if (it->second.phase == 2 && it->second.durationS > 0.0)
        {
            const double lost = it->second.costT * std::clamp(it->second.elapsedS / it->second.durationS, 0.0, 1.0);
            motion.jChargeT = std::max(0.0, motion.jChargeT - lost);
            std::cerr << "[Jump] ship " << shipId.value << ": core discharged, " << lost << " t of charge lost" << std::endl;
        }
        // the procedure held the ship at rest over its zone body: the J field lets it go on a circular orbit there
        // (left at rest it would fall - 2026-11-12 live)
        if (!it->second.zoneBodyId.empty() && it->second.zoneBodyId != it->second.starId)
            for (const auto& b : m_gravityBodies)
                if (b.id == it->second.zoneBodyId)
                {
                    auto& frame = motion.travelFrame;
                    const glm::dvec3 shipM = frame.localToWorldPosition(motion.localPositionMeters);
                    const glm::dvec3 r = shipM - b.centerMeters;
                    if (glm::length(r) < 1.0)
                        break;
                    const glm::dvec3 radial = glm::normalize(r);
                    const glm::dvec3 bodyVel = it->second.velocities.count(b.id) ? it->second.velocities.at(b.id) : glm::dvec3(0.0);
                    glm::dvec3 normal = glm::cross(radial, bodyVel);
                    if (glm::length(normal) < 1.0e-9)
                        normal = glm::cross(radial, glm::dvec3(0.0, 1.0, 0.0));
                    const glm::dvec3 tangent = glm::normalize(glm::cross(glm::normalize(normal), radial));
                    frame.originMeters = shipM;
                    frame.linearVelocityMps = bodyVel + tangent * std::sqrt(b.gravitationalParameterM3s2 / glm::length(r));
                    motion.localPositionMeters = glm::dvec3(0.0);
                    motion.localVelocityMps = glm::dvec3(0.0);
                    break;
                }
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
        if (!passOwns(ship))
        {
            ++it;   // another system's pass (a jumped ship waits for its new system's pass)
            continue;
        }

        if (!j.jumped)
        {
            const auto ff = m_freeFlights.find(id);
            if (!motion.freeFlight || ff == m_freeFlights.end() || ff->second.localFrame)
            {
                ++it;
                cancelJump(id, static_cast<std::uint8_t>(jr::Refusal::Interrupted));
                continue;
            }
            if (!game::navigation::jdrive::jumpFieldComplete(game::simulation::jFieldIntegrity(
                    static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()))))
            {
                ++it;
                cancelJump(id, static_cast<std::uint8_t>(jr::Refusal::FieldIncomplete));   // an emitter lost mid charge-up
                continue;
            }
            // ---- the procedure (JUMP_MECHANICS.md 10b): zone -> aim -> charge -> the jump 1 s after the charge
            const glm::dvec3 shipM = frame.localToWorldPosition(motion.localPositionMeters);
            const glm::dvec3 shipV = frame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
            for (const auto& b : m_gravityBodies)
                if (b.id == j.zoneBodyId || b.id == j.starId || b.id == j.exitBodyId)
                {
                    if (const auto last = j.lastCenters.find(b.id); last != j.lastCenters.end() && dt > 0.0)
                        j.velocities[b.id] = (b.centerMeters - last->second) / dt;
                    j.lastCenters[b.id] = b.centerMeters;
                }
            glm::dvec3 zoneBase(0.0), zoneVel(0.0);
            for (const auto& b : m_gravityBodies)
                if (b.id == j.zoneBodyId)
                {
                    zoneBase = b.centerMeters;
                    zoneVel = j.velocities.count(b.id) ? j.velocities[b.id] : glm::dvec3(0.0);
                }
            const glm::dvec3 zoneM = zoneBase + j.zoneOffsetM;
            const glm::dvec3 legEnd = jumpLegEnd(j, shipM);
            const glm::dvec3 dir = j.inSystem ? (glm::length(legEnd - shipM) > 1.0 ? glm::normalize(legEnd - shipM) : j.interstellarDir)
                                              : j.interstellarDir;
            const glm::dvec3 nose = glm::normalize(glm::dvec3(tr.forward()));
            const double noseOff = std::acos(std::clamp(glm::dot(nose, dir), -1.0, 1.0));
            std::string why;
            const int blockedHere = jEngineBlocked(id, motion.systemId, shipM, why);

            ShipControlState cmd;
            cmd.globalFlightCommandValid = true;
            cmd.globalAttitudeTargetValid = true;
            cmd.globalAttitudeForward = dir;
            cmd.globalAttitudeUp = glm::dvec3(tr.up());
            const glm::dvec3 toZone = zoneM - shipM;
            const double d = glm::length(toZone);
            if (j.phase == 0)
            {
                // to the start zone: in J (under a J floor on the main engines), braking to rest against its body.
                // Never through the body: a zone on its far side is reached along an arc at the zone's height, at most
                // 20 deg ahead (such a chord stays above the J floor - 2026-11-12: a straight line flew into the Earth).
                glm::dvec3 aim = zoneM;
                if (!j.zoneBodyId.empty() && glm::length(j.zoneOffsetM) > 1.0 && glm::length(shipM - zoneBase) > 1.0)
                {
                    const glm::dvec3 u0 = glm::normalize(shipM - zoneBase);
                    const glm::dvec3 u1 = glm::normalize(j.zoneOffsetM);
                    const double arc = std::acos(std::clamp(glm::dot(u0, u1), -1.0, 1.0));
                    constexpr double MaxArcStep = 20.0 * 3.14159265358979323846 / 180.0;
                    if (arc > MaxArcStep)
                    {
                        glm::dvec3 axis = glm::cross(u0, u1);
                        if (glm::length(axis) < 1.0e-9)
                            axis = glm::cross(u0, glm::dvec3(0.0, 1.0, 0.0));
                        axis = glm::normalize(axis);
                        const glm::dvec3 ahead = u0 * std::cos(MaxArcStep) + glm::cross(axis, u0) * std::sin(MaxArcStep);
                        aim = zoneBase + glm::normalize(ahead) * glm::length(j.zoneOffsetM);
                    }
                }
                const glm::dvec3 toAim = aim - shipM;
                const double da = glm::length(toAim);
                // braking only for the zone itself; along the arc the speed is what the remaining way allows
                const double speed = std::min(0.8 * std::sqrt(2.0 * jr::ZoneTransitAccelMps2 * d), 0.5 * game::navigation::jdrive::MediumOnsetMps);
                const glm::dvec3 wanted = zoneVel + (da > 1.0 ? toAim / da : glm::dvec3(0.0)) * speed;
                if (blockedHere == game::navigation::code(game::navigation::FlightRefusal::JRefusedBelowFloor))
                {
                    glm::dvec3 a = (wanted - shipV) / 5.0;
                    cmd.globalMainAccelWorldMps2 = a;
                }
                else
                {
                    cmd.globalJVelocityTargetValid = true;
                    cmd.globalJVelocityTargetWorldMps = wanted;
                    cmd.globalJAccelLimitMps2 = jr::ZoneTransitAccelMps2;
                }
                if (d < jr::ZoneArrivalM && glm::length(shipV - zoneVel) < jr::ZoneArrivalSpeedMps)
                {
                    j.phase = 1;
                    motion.jumpState = 5;
                    std::cerr << "[Jump] ship " << id.value << ": in the start zone (" << j.zoneBodyId << "), aiming" << std::endl;
                }
            }
            else
            {
                // hold the zone against its body
                cmd.globalJVelocityTargetValid = true;
                cmd.globalJVelocityTargetWorldMps = zoneVel + toZone / 10.0;
                cmd.globalJAccelLimitMps2 = jr::ZoneHoldAccelMps2;
            }
            j.command = cmd;
            j.commandValid = true;
            if (j.phase == 1 && noseOff < jr::AlignToleranceRad)
            {
                if (j.inSystem)
                {
                    // the step from where the ship really is
                    j.costT = jumpLegCostT(j, shipM, ship->core().effectivePhysics().massKg / 1000.0);
                    j.durationS = jr::inSystemChargeUpSeconds(glm::length(legEnd - shipM) / jr::AuM);
                    if (const char* dev = std::getenv("ELITE_DEV_JUMP_CHARGE_S"); dev && *dev)
                        j.durationS = std::max(1.0, std::atof(dev));
                }
                j.phase = 2;
                j.elapsedS = 0.0;
                j.chargedHoldS = 0.0;
                motion.jumpState = 1;
                motion.jumpChargeElapsedS = 0.0;
                motion.jumpChargeDurationS = j.durationS;
                motion.jumpCostT = j.costT;
                std::cerr << "[Jump] ship " << id.value << ": aimed, CHARGING " << j.durationS << " s, step " << j.costT << " t" << std::endl;
            }
            if (j.phase < 2)
            {
                ++it;
                continue;
            }
            // charging: every condition holds or the charge is lost
            j.lineCheckS += dt;
            std::string blocker;
            if (j.lineCheckS >= jr::LineCheckPeriodS)
            {
                j.lineCheckS = 0.0;
                blocker = jumpLineBlockedBy(shipM, legEnd, j.inSystem ? j.exitBodyId : std::string());
            }
            if (blockedHere != 0 || noseOff > jr::ChargeNoseToleranceRad || !blocker.empty())
            {
                std::cerr << "[Jump] ship " << id.value << ": ABORT during the charge-up - "
                          << (blockedHere ? why : !blocker.empty() ? "the line is blocked by " + blocker : "the nose left the line")
                          << std::endl;
                ++it;
                cancelJump(id, static_cast<std::uint8_t>(jr::Refusal::ZoneViolated));
                continue;
            }
            j.elapsedS = std::min(j.durationS, j.elapsedS + dt);
            motion.jumpChargeElapsedS = j.elapsedS;
            if (j.elapsedS < j.durationS || (j.chargedHoldS += dt) < jr::JumpDelayAfterChargeS)
            {
                ++it;
                continue;
            }
            j.commandValid = false;
            // ---- the jump: instant
            motion.jChargeT = std::max(0.0, motion.jChargeT - j.costT);
            if (j.inSystem && j.legIndex < static_cast<int>(j.waypointsStarRel.size()))
            {
                // an intermediate step: out at the step's end (with the exit error) at rest against the star; the
                // next step starts there - aim, charge, jump (open space is a start zone)
                const double lock = jr::exitLockFactor(systemHasSites(motion.systemId), systemHasSites(motion.systemId));
                const double stepAu = glm::length(legEnd - shipM) / jr::AuM;
                const double sigma = jr::inSystemExitSigmaM(stepAu, lock);
                glm::dvec3 offset(0.0);
                if (sigma > 0.0)
                {
                    std::mt19937_64 rng(static_cast<std::uint64_t>(id.value) * 0x9E3779B97F4A7C15ull + motion.jumpSerial * 7919ull + 29ull);
                    std::normal_distribution<double> n(0.0, sigma / std::sqrt(3.0));
                    offset = glm::dvec3(n(rng), n(rng), n(rng));
                }
                const glm::dvec3 starVel = j.velocities.count(j.starId) ? j.velocities[j.starId] : glm::dvec3(0.0);
                frame.originMeters = legEnd + offset;
                frame.linearVelocityMps = starVel;
                motion.localPositionMeters = glm::dvec3(0.0);
                motion.localVelocityMps = glm::dvec3(0.0);
                tr.setWorldPositionMeters(frame.originMeters);
                motion.hubId.clear();
                motion.parentBodyId.clear();
                motion.freeFlightReferenceId.clear();
                motion.freeFlightOrbitBodyId.clear();
                m_freeFlights[id].lastPrimaryId.clear();
                ++motion.jumpSerial;
                std::cerr << "[Jump] ship " << id.value << ": STEP " << j.legIndex + 1 << " of " << j.waypointsStarRel.size() + 1 << " done ("
                          << stepAu << " AU, " << glm::length(offset) / 1000.0 << " km off), charge spent " << j.costT << " t, left "
                          << motion.jChargeT << " t" << std::endl;
                ++j.legIndex;
                const auto physics = ship->core().effectivePhysics();
                j.costT = jumpLegCostT(j, frame.originMeters, physics.massKg / 1000.0);
                j.durationS = jr::inSystemChargeUpSeconds(glm::length(jumpLegEnd(j, frame.originMeters) - frame.originMeters) / jr::AuM);
                if (const char* dev = std::getenv("ELITE_DEV_JUMP_CHARGE_S"); dev && *dev)
                    j.durationS = std::max(1.0, std::atof(dev));
                j.zoneBodyId = j.starId;
                for (const auto& b : m_gravityBodies)
                    if (b.id == j.starId)
                        j.zoneOffsetM = frame.originMeters - b.centerMeters;
                j.phase = 1;
                j.elapsedS = 0.0;
                motion.jumpState = 5;
                ++it;
                continue;
            }
            if (j.inSystem)
            {
                // in-system, the last step: the exit on the target's side facing the line (placed below)
                for (const auto& b : m_gravityBodies)
                    if (b.id == j.exitBodyId && glm::length(shipM - b.centerMeters) > 1.0)
                        j.exitRadial = glm::normalize(shipM - b.centerMeters);
                j.lastStepFromM = shipM;
                j.fromSystemId = motion.systemId;
                motion.hubId.clear();
                motion.parentBodyId.clear();
                motion.freeFlightReferenceId.clear();
                motion.freeFlightOrbitBodyId.clear();
                m_freeFlights[id].lastPrimaryId.clear();
                motion.jEngine = false;
                motion.jStopping = false;
                motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
                motion.freeFlightThrust = 0;
                motion.jumpState = 3;
                ++motion.jumpSerial;
                j.jumped = true;
                j.waitTicks = 0;
                std::cerr << "[Jump] ship " << id.value << ": IN-SYSTEM JUMP to " << j.exitBodyId << " (" << j.distanceAu
                          << " AU), charge spent " << j.costT << " t, left " << motion.jChargeT << " t" << std::endl;
                ++it;
                continue;
            }
            const int from = motion.systemId;
            j.fromSystemId = from;
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
        if (m_gravityBodies.empty())
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
        glm::dvec3 tangent = glm::normalize(glm::cross(normal, radial));
        if (j.inSystem && glm::length(j.exitRadial) > 0.5)
        {
            // the straight step ends on the target's side facing it (JUMP_MECHANICS.md 10b)
            radial = j.exitRadial;
            normal = glm::cross(radial, bodyVel);
            if (glm::length(normal) < 1.0e-9)
                normal = glm::cross(radial, glm::dvec3(0.0, 1.0, 0.0));
            normal = glm::normalize(normal);
            tangent = glm::normalize(glm::cross(normal, radial));
        }
        else if (!star)
        {
            // every ship its own exit point on the lit side (load test 2026-11-08: all the ships
            // jumping to one planet exited at ONE point - inside each other, and each then blocked
            // the other's J engine / next jump for good): +-60 deg along the orbit by the ship id
            const double a = glm::radians(static_cast<double>((id.value * 37u) % 121u) - 60.0);
            radial = glm::normalize(std::cos(a) * radial + std::sin(a) * tangent);
            tangent = glm::normalize(glm::cross(normal, radial));
        }
        // the exit error (author 2026-11-09: the report tells how far from the calculated point the
        // ship came out): a deterministic draw per ship and jump around the calculated point, one
        // sigma of JumpRules.h exitSigmaM; the ship then holds a circular orbit at the radius it
        // came out at (kept above the floor and well inside the body's Hill sphere)
        const glm::dvec3 planned = exit->centerMeters + radial * r;
        const double lock = jr::exitLockFactor(systemHasSites(j.fromSystemId), systemHasSites(j.targetSystemId));
        const double sigma = j.inSystem ? jr::inSystemExitSigmaM(glm::length(planned - j.lastStepFromM) / jr::AuM, lock)
                                        : jr::exitSigmaM(j.distanceLy, lock);
        glm::dvec3 offset(0.0);
        // a zero-distance exit (DEV placement: ELITE_DEV_START / the load test) has no scatter;
        // std::normal_distribution requires sigma > 0 (debug libstdc++ asserts)
        if (sigma > 0.0)
        {
            std::mt19937_64 rng(static_cast<std::uint64_t>(id.value) * 0x9E3779B97F4A7C15ull + motion.jumpSerial * 7919ull + 17ull);
            std::normal_distribution<double> n(0.0, sigma / std::sqrt(3.0));
            offset = glm::dvec3(n(rng), n(rng), n(rng));
        }
        glm::dvec3 actual = planned + offset;
        double rActual = r;
        if (!star)
        {
            const double hill = glm::length(toStar) * std::cbrt(exit->gravitationalParameterM3s2 / std::max(1.0, 3.0 * starMu));
            const double rMin = exit->radiusMeters + floor + jr::ExitOrbitAboveFloorM;
            const double rMax = std::max(rMin * 1.5, 0.3 * hill);
            glm::dvec3 rel = actual - exit->centerMeters;
            rActual = std::clamp(glm::length(rel), rMin, rMax);
            radial = glm::length(rel) > 1.0 ? glm::normalize(rel) : radial;
            actual = exit->centerMeters + radial * rActual;
            glm::dvec3 t = glm::cross(normal, radial);
            if (glm::length(t) < 1.0e-6)
                t = glm::cross(glm::dvec3(0.0, 0.0, 1.0), radial);
            tangent = glm::normalize(t);
        }
        motion.jumpExitDeviationM = glm::length(actual - planned);
        motion.jumpExitSigmaM = sigma;
        motion.jumpExitAltitudeM = star ? rActual : rActual - exit->radiusMeters;
        motion.jumpExitBodyName = exit->id;   // the client shows the body's name from its snapshot
        frame.originMeters = actual;
        frame.linearVelocityMps = bodyVel + (star ? glm::dvec3(0.0) : tangent * std::sqrt(exit->gravitationalParameterM3s2 / rActual));
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
        ++motion.jumpExitSerial;
        auto& f = m_freeFlights[id];
        f.lastPrimaryId.clear();
        f.autoStabilizeArmed = true;   // a closed orbit: the automatic stabilization may take it
        std::cerr << "[Jump] ship " << id.value << ": EXIT in system " << j.targetSystemId << " - "
                  << (star ? "in space, 1 AU from the star" : "orbit of " + exit->id + " at " + std::to_string(static_cast<int>((rActual - exit->radiusMeters) / 1000.0)) + " km")
                  << ", " << motion.jumpExitDeviationM / 1000.0 << " km off the calculated point (sigma " << sigma / 1000.0 << " km)" << std::endl;
        it = m_jumps.erase(it);
    }
}

// ---- the jump procedure's helpers (JUMP_MECHANICS.md 10b)

// the first body (with its J floor; the star with its capture sphere) the straight segment passes through
std::string GameSimulation::jumpLineBlockedBy(const glm::dvec3& from, const glm::dvec3& to, const std::string& excludeBodyId,
                                              const std::string& excludeBodyId2) const
{
    double starMu = 0.0;
    for (const auto& b : m_gravityBodies)
        starMu = std::max(starMu, b.gravitationalParameterM3s2);
    for (const auto& b : m_gravityBodies)
    {
        if (b.id == excludeBodyId || (!excludeBodyId2.empty() && b.id == excludeBodyId2) || b.radiusMeters <= 0.0)
            continue;
        const bool star = b.gravitationalParameterM3s2 >= starMu;
        const double sphere = star ? std::max(jr::starCaptureRadiusM(b.gravitationalParameterM3s2), b.radiusMeters)
                                   : b.radiusMeters + game::navigation::rules::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters);
        // the segment starts outside every sphere (a zone is above the floors): a start inside one is the zone's problem
        if (glm::length(from - b.centerMeters) < sphere)
            continue;
        if (jr::segmentDistanceM(from, to, b.centerMeters) < sphere)
            return b.id;
    }
    return {};
}

// the current step's end: an intermediate point, the target's exit side (in-system) or far along the star's direction
glm::dvec3 GameSimulation::jumpLegEnd(const ActiveJump& j, const glm::dvec3& shipM) const
{
    if (!j.inSystem)
        return shipM + j.interstellarDir * 1.0e15;   // ~6 700 AU: out of the system along the line
    const game::navigation::GravityBody* star = nullptr;
    const game::navigation::GravityBody* target = nullptr;
    for (const auto& b : m_gravityBodies)
    {
        if (b.id == j.starId)
            star = &b;
        if (b.id == j.exitBodyId)
            target = &b;
    }
    if (star && j.legIndex < static_cast<int>(j.waypointsStarRel.size()))
        return star->centerMeters + j.waypointsStarRel[static_cast<std::size_t>(j.legIndex)];
    if (!target)
        return shipM;
    const double floor = game::navigation::rules::floorAltitudeM(target->radiusMeters, target->atmosphereRadiusMeters);
    const glm::dvec3 side = glm::length(shipM - target->centerMeters) > 1.0 ? glm::normalize(shipM - target->centerMeters)
                                                                           : glm::dvec3(1.0, 0.0, 0.0);
    return target->centerMeters + side * (target->radiusMeters + floor + jr::ExitOrbitAboveFloorM);
}

double GameSimulation::jumpLegCostT(const ActiveJump& j, const glm::dvec3& fromM, double massT) const
{
    if (!j.inSystem)
        return j.totalCostT;
    return jr::inSystemChargeT(glm::length(jumpLegEnd(j, fromM) - fromM) / jr::AuM, massT);
}

// The start zone: where the ship is if it is safe there and the step's line is free; else over the dominant body on the
// side facing the step's end, above its J floor + ZoneAboveFloorM (higher if a hub's keep-out is there); in open
// space beside the object that blocks it.
bool GameSimulation::planJumpZone(EntityId shipId, ActiveJump& j, const glm::dvec3& shipM, const glm::dvec3& legEndM, bool lineMustBeClear)
{
    const Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    const auto& motion = ship->core().transform().motion;
    std::string why;
    const bool safeHere = jEngineBlocked(shipId, motion.systemId, shipM, why) == 0;
    const std::string blocked = lineMustBeClear ? jumpLineBlockedBy(shipM, legEndM, j.inSystem ? j.exitBodyId : std::string()) : std::string();
    double starMu = 0.0;
    for (const auto& b : m_gravityBodies)
        starMu = std::max(starMu, b.gravitationalParameterM3s2);
    // the dominant body: the primary of the free flight (the star in open space)
    const game::navigation::GravityBody* body = nullptr;
    if (const auto f = m_freeFlights.find(shipId); f != m_freeFlights.end())
        for (const auto& b : m_gravityBodies)
            if (b.id == f->second.primaryBodyId)
                body = &b;
    if (safeHere && blocked.empty())
    {
        j.zoneBodyId = body ? body->id : std::string();
        j.zoneOffsetM = body ? shipM - body->centerMeters : shipM;
        j.phase = 1;
        return true;
    }
    j.phase = 0;
    if (body && body->gravitationalParameterM3s2 < starMu)
    {
        // over a body: the nearest point from which the straight line to the step's end misses the body (with its J floor).
        // On the side facing the step's end the line goes away from the body: the ship's own radial. Behind the body:
        // straight up the radial until the line passes beside it (r >= 1.2 R' / sin of the angle off the line); right
        // behind it the radial is first turned <= 17.5 deg aside. Then higher in 25 km steps until clear of keep-outs.
        const double floor = game::navigation::rules::floorAltitudeM(body->radiusMeters, body->atmosphereRadiusMeters);
        const double rFloor = body->radiusMeters + floor;
        const glm::dvec3 rel = shipM - body->centerMeters;
        const double r0 = glm::length(rel);
        const glm::dvec3 u0 = r0 > 1.0 ? rel / r0 : glm::dvec3(1.0, 0.0, 0.0);
        const glm::dvec3 toEnd = legEndM - body->centerMeters;
        const glm::dvec3 lineDir = glm::length(toEnd) > 1.0 ? glm::normalize(toEnd) : glm::dvec3(1.0, 0.0, 0.0);
        glm::dvec3 u = u0;
        double r = std::max(r0, rFloor + 50000.0);
        const double c = glm::dot(u0, lineDir);
        if (c < 0.0)
        {
            glm::dvec3 perp = u0 - lineDir * c;
            double sinOff = glm::length(perp);
            constexpr double MinSinOff = 0.3;
            if (sinOff < MinSinOff)
            {
                glm::dvec3 n = sinOff > 1.0e-6 ? perp / sinOff : glm::cross(lineDir, glm::dvec3(0.0, 1.0, 0.0));
                if (glm::length(n) < 1.0e-6)
                    n = glm::cross(lineDir, glm::dvec3(1.0, 0.0, 0.0));
                n = glm::normalize(n);
                u = glm::normalize(-lineDir * std::sqrt(1.0 - MinSinOff * MinSinOff) + n * MinSinOff);
                sinOff = MinSinOff;
            }
            r = std::max(r, 1.2 * rFloor / sinOff);
        }
        for (int i = 0; i < 200; ++i, r += 25000.0)
        {
            const glm::dvec3 p = body->centerMeters + u * r;
            JKeepOutHit hit;
            if (!jKeepOut(shipId, motion.systemId, p, hit) &&
                (!lineMustBeClear || jumpLineBlockedBy(p, legEndM, j.inSystem ? j.exitBodyId : std::string(), body->id == j.zoneBodyId ? std::string() : std::string()).empty()))
            {
                j.zoneBodyId = body->id;
                j.zoneOffsetM = p - body->centerMeters;
                return true;
            }
        }
        return false;
    }
    // open space: step aside from the object (its keep-out + 10 km), or along the line
    JKeepOutHit hit;
    glm::dvec3 p = shipM;
    if (jKeepOut(shipId, motion.systemId, shipM, hit))
    {
        const glm::dvec3 away = glm::length(shipM - hit.positionMeters) > 1.0 ? glm::normalize(shipM - hit.positionMeters)
                                                                               : glm::normalize(legEndM - shipM);
        p = hit.positionMeters + away * ((hit.ship ? game::navigation::jdrive::KeepOutShipM : game::navigation::jdrive::KeepOutObjectM) + 10000.0);
    }
    j.zoneBodyId = body ? body->id : std::string();
    j.zoneOffsetM = body ? p - body->centerMeters : p;
    return !lineMustBeClear || jumpLineBlockedBy(p, legEndM, j.inSystem ? j.exitBodyId : std::string()).empty();
}

bool GameSimulation::devPlaceShipAtPoint(EntityId id, const glm::dvec3& au)
{
    Ship* ship = getShip(id);
    const game::navigation::GravityBody* star = nullptr;
    for (const auto& b : m_gravityBodies)
        if (!star || b.gravitationalParameterM3s2 > star->gravitationalParameterM3s2)
            star = &b;
    if (!ship || !star)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
        (void)toggleFreeFlight(id);
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
        return false;
    auto& frame = motion.travelFrame;
    frame.originMeters = star->centerMeters + au * jr::AuM;
    frame.linearVelocityMps = glm::dvec3(0.0);
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    tr.setWorldPositionMeters(frame.originMeters);
    motion.freeFlightReferenceId.clear();
    motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
    m_shipReferenceBindings.erase(id);
    motion.matchedToReferenceFrame = false;
    m_freeFlights[id].localFrame = false;
    std::cerr << "[DevStart] ship " << id.value << ": normal flight at (" << au.x << ", " << au.y << ", " << au.z << ") AU from "
              << star->id << std::endl;
    return true;
}

bool GameSimulation::devPlaceShipNearHub(EntityId id, const std::string& hubId, double distanceKm)
{
    Ship* ship = getShip(id);
    const auto* hub = hubNavigationFrame(hubId);
    if (!ship || !hub || !hub->valid)
    {
        std::cerr << "[DevStart] hub " << hubId << ": not found / not valid" << std::endl;
        return false;
    }
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
        (void)toggleFreeFlight(id);   // out of the start hub's frame into normal flight
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
    {
        std::cerr << "[DevStart] ship " << id.value << ": not in normal flight" << std::endl;
        return false;
    }
    // radially away from the body the hub orbits (the nearest gravity body), else +X
    glm::dvec3 up(1.0, 0.0, 0.0);
    double best = std::numeric_limits<double>::infinity();
    for (const auto& b : m_gravityBodies)
    {
        const double d = glm::length(hub->originMeters - b.centerMeters) - b.radiusMeters;
        if (b.radiusMeters > 0.0 && d < best)
        {
            best = d;
            up = glm::normalize(hub->originMeters - b.centerMeters);
        }
    }
    auto& frame = motion.travelFrame;
    frame.originMeters = hub->originMeters + up * distanceKm * 1000.0;
    frame.linearVelocityMps = hub->velocityMetersPerSecond;
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    tr.setWorldPositionMeters(frame.originMeters);
    motion.freeFlightReferenceId = hubId;
    motion.freeFlightReferenceVelocityMps = hub->velocityMetersPerSecond;
    m_shipReferenceBindings.erase(id);
    motion.matchedToReferenceFrame = false;
    m_freeFlights[id].localFrame = false;
    std::cerr << "[DevStart] ship " << id.value << ": normal flight " << distanceKm << " km above hub " << hubId << std::endl;
    return true;
}

bool GameSimulation::devPlaceShipInSystem(EntityId id, int systemId, const std::string& systemName, const std::string& exitBodyId)
{
    Ship* ship = getShip(id);
    if (!ship || systemId < 0)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
        (void)toggleFreeFlight(id);   // out of a hub frame into normal flight
    if (!motion.freeFlight || m_freeFlights.count(id) == 0)
    {
        std::cerr << "[Jump] DEV place ship " << id.value << " in system " << systemId << ": not in normal flight" << std::endl;
        return false;
    }
    m_freeFlights[id].localFrame = false;
    auto& frame = motion.travelFrame;
    motion.systemId = systemId;
    frame.systemId = systemId;
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
    m_shipReferenceBindings.erase(id);
    motion.matchedToReferenceFrame = false;
    auto& f = m_freeFlights[id];
    f.lastPrimaryId.clear();
    f.primaryBodyId.clear();
    f.autoStabilizeArmed = false;
    ActiveJump j;
    j.targetSystemId = systemId;
    j.targetName = systemName;
    j.exitBodyId = exitBodyId;
    j.jumped = true;
    m_jumps[id] = j;
    motion.jumpState = 3;
    motion.jumpTargetSystemId = systemId;
    motion.jumpTargetName = systemName;
    return true;
}
