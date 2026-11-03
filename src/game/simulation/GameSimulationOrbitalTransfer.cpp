// Orbital Transfer: authoritative hub -> hub travel (server).
//
// Hub Tactical and Orbital Transfer are different jobs. Hub Tactical keeps
// the ship's local flight inside the moving hub frame (docking, queues,
// 500 m/s controlled envelope). Orbital Transfer moves the FRAME: on request
// the ship's travel frame is detached from the source hub (the existing
// TravelFrameSystem seam, "later J propulsion will detach and accelerate this
// frame"), re-based at the ship (local state 0), and its origin is then driven
// by the carrier law of game::navigation::orbital - plan feed-forward + real
// gravity of the central body + PD, clamped to the ship's main engine. The
// ordinary tactical integrator keeps running inside that frame, so the pilot's
// local envelope is untouched and maxCombatSpeed is never raised. At the
// arrival epoch the ship is re-based into the destination hub frame with its
// actual relative position and velocity (no snap): the plan arrives at the
// hub's approach point with the hub's velocity at that epoch.
//
// A destination around another body (Earth -> Ceres) is an ABSTRACTED,
// time-compressed transit: the physical brachistochrone (days) is computed
// and reported, the ship waits in a transit state for a short real time and
// is then placed at the destination's approach point with the destination's
// velocity. That leg is labelled as an abstraction, not flight.
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/TravelFrameSystem.h"

namespace
{
namespace orb = game::navigation::orbital;

constexpr double TransitRealSeconds = 90.0;      // abstracted interplanetary leg, real time
constexpr double CarrierStepSeconds = 0.05;
}

bool GameSimulation::requestOrbitalTransfer(EntityId shipId, const std::string& targetHubId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;
    const auto targetIt = m_orbitalHubs.find(targetHubId);
    if (targetIt == m_orbitalHubs.end() || targetIt->second.systemId != motion.systemId)
    {
        std::cerr << "[OrbitalTransfer] ship " << shipId.value << ": unknown destination " << targetHubId << "\n";
        return false;
    }
    if (motion.mode != game::navigation::MotionMode::HubTactical || !motion.travelFrame.valid || motion.hubId == targetHubId ||
        m_orbitalTransfers.count(shipId) || isDockStationControlledShip(shipId))
    {
        std::cerr << "[OrbitalTransfer] ship " << shipId.value << ": not in a state to depart (mode/frame/docked/already in transfer)\n";
        return false;
    }
    const auto sourceIt = m_orbitalHubs.find(motion.hubId);
    const auto& target = targetIt->second;
    const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    const glm::dvec3 shipWorldVel = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    const double forwardMain = std::max(1.0, static_cast<double>(ship->core().effectivePhysics().forwardMainEngineAccelerationMps2));

    ActiveOrbitalTransfer t;
    t.sourceHubId = motion.hubId;
    t.targetHubId = targetHubId;
    t.parentBodyId = target.parentBodyId;
    t.engineMaxAccelMps2 = forwardMain;
    t.lastOrbitalTimeSeconds = m_orbitalUniverseTimeSeconds;
    t.limits.maxThrustAccelMps2 = std::min(18.0, 0.25 * forwardMain);

    const bool sameBody = sourceIt != m_orbitalHubs.end() && sourceIt->second.parentBodyId == target.parentBodyId;
    const auto parentPosIt = m_celestialBodyPositionsAu.find(target.parentBodyId);
    const glm::dvec3 parentPos = parentPosIt != m_celestialBodyPositionsAu.end()
        ? parentPosIt->second * world::celestial::MetersPerAu : glm::dvec3(0.0);
    const auto parentVelIt = m_celestialBodyVelocitiesMetersPerSecond.find(target.parentBodyId);
    const glm::dvec3 parentVel = parentVelIt != m_celestialBodyVelocitiesMetersPerSecond.end() ? parentVelIt->second : glm::dvec3(0.0);
    const auto muIt = m_celestialBodyGravityParameters.find(target.parentBodyId);
    t.body.radiusMeters = target.motion.parentRadiusMeters;
    double atmosphereTop = t.body.radiusMeters;
    if (muIt != m_celestialBodyGravityParameters.end())
    {
        t.body.muM3s2 = muIt->second.gravitationalParameterM3s2;
        t.body.radiusMeters = muIt->second.radiusMeters > 0.0 ? muIt->second.radiusMeters : t.body.radiusMeters;
        atmosphereTop = std::max(muIt->second.atmosphereOuterRadiusMeters, t.body.radiusMeters);
    }
    // catalog atmosphere top + 150 km of margin (never below 250 km over Earth-size bodies)
    t.body.minAltitudeMeters = std::max(atmosphereTop - t.body.radiusMeters + 150000.0, 0.039 * t.body.radiusMeters);

    if (sameBody)
    {
        t.plan = orb::planOrbitalTransfer(t.body, shipWorld - parentPos, shipWorldVel - parentVel, m_orbitalUniverseTimeSeconds, target.motion, t.limits);
        if (!t.plan.feasible)
        {
            std::cerr << "[OrbitalTransfer] ship " << shipId.value << " -> " << targetHubId << ": no plan (" << t.plan.reason << ")\n";
            return false;
        }
        t.carrier.positionMeters = shipWorld - parentPos;
        t.carrier.velocityMps = shipWorldVel - parentVel;
        t.carrier.timeSeconds = m_orbitalUniverseTimeSeconds;
    }
    else
    {
        // abstracted interplanetary transit: the physical leg it stands for
        const glm::dvec3 targetWorld = parentPos + orb::targetPositionAt(target.motion, m_orbitalUniverseTimeSeconds);
        t.abstracted = true;
        t.estimate = orb::brachistochrone(glm::length(targetWorld - shipWorld), t.limits.maxThrustAccelMps2);
        t.transitEndRealSeconds = m_serverTimelineClock.timeSeconds() + TransitRealSeconds;
    }

    // Detach: the ship's own travel frame stops following the source hub and is re-based at the
    // ship (non-rotating, same instantaneous kinematics); local flight state starts at rest in it.
    game::navigation::KinematicFrame frame = motion.travelFrame;
    frame.originMeters = shipWorld;
    frame.linearVelocityMps = shipWorldVel;
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0);
    game::navigation::TravelFrameSystem::detach(motion);
    motion.travelFrame = frame;
    motion.travelFrame.valid = true;
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    if (t.abstracted)
        motion.mode = game::navigation::MotionMode::Cruise;

    if (t.abstracted)
        std::cerr << "[OrbitalTransfer] ship " << shipId.value << " " << t.sourceHubId << " -> " << targetHubId
                  << ": ABSTRACTED transit, physical brachistochrone " << t.estimate.durationSeconds / 86400.0 << " d at "
                  << t.estimate.accelMps2 << " m/s2, peak " << t.estimate.peakSpeedMps / 1000.0 << " km/s; real " << TransitRealSeconds << " s\n";
    else
        std::cerr << "[OrbitalTransfer] ship " << shipId.value << " " << t.sourceHubId << " -> " << targetHubId << ": ETA "
                  << t.plan.durationSeconds() << " s, path " << t.plan.pathLengthMeters / 1000.0 << " km, peak " << t.plan.peakSpeedMps / 1000.0
                  << " km/s (vs target " << t.plan.peakSpeedVsTargetMps / 1000.0 << "), dv " << t.plan.deltaVMps / 1000.0 << " km/s, min alt "
                  << t.plan.minAltitudeMeters / 1000.0 << " km\n";
    m_orbitalTransfers[shipId] = std::move(t);
    return true;
}

void GameSimulation::cancelOrbitalTransfer(EntityId shipId)
{
    auto it = m_orbitalTransfers.find(shipId);
    if (it == m_orbitalTransfers.end())
        return;
    // physical transfer: the engine stops, the carrier keeps coasting on gravity
    if (!it->second.abstracted)
        it->second.thrustEnabled = false;
    std::cerr << "[OrbitalTransfer] ship " << shipId.value << ": transfer cancelled (coasting)\n";
}

const GameSimulation::ActiveOrbitalTransfer* GameSimulation::orbitalTransfer(EntityId shipId) const
{
    const auto it = m_orbitalTransfers.find(shipId);
    return it == m_orbitalTransfers.end() ? nullptr : &it->second;
}

void GameSimulation::handOverToHub(EntityId shipId, Ship& ship, const std::string& hubId, const glm::dvec3* worldOverride, const glm::dvec3* velOverride)
{
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    const auto* hubFrame = hubNavigationFrame(hubId);
    if (!hubFrame || !hubFrame->valid)
        return;
    const game::navigation::KinematicFrame dest = hubFrame->kinematicFrame();
    game::navigation::WorldKinematicState world;
    world.positionMeters = worldOverride ? *worldOverride : motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    world.velocityMps = velOverride ? *velOverride : motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    const game::navigation::LocalKinematicState local = game::navigation::worldToLocalKinematics(dest, world);
    game::navigation::TravelFrameSystem::matchToReference(motion, dest, "ship_travel_" + std::to_string(shipId.value), hubId);
    motion.hubId = hubId;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.localPositionMeters = local.positionMeters;
    motion.localVelocityMps = local.velocityMps;
    tr.setWorldPositionMeters(world.positionMeters);
    motion.worldVelocityMps = world.velocityMps;
    std::cerr << "[OrbitalTransfer] ship " << shipId.value << " handed over to " << hubId << ": local offset "
              << glm::length(local.positionMeters) / 1000.0 << " km, relative speed " << glm::length(local.velocityMps) << " m/s\n";
}

void GameSimulation::updateOrbitalTransfers()
{
    for (auto it = m_orbitalTransfers.begin(); it != m_orbitalTransfers.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship)
        {
            it = m_orbitalTransfers.erase(it);
            continue;
        }
        auto& t = it->second;
        auto& motion = ship->core().transform().motion;
        const auto targetIt = m_orbitalHubs.find(t.targetHubId);
        if (targetIt == m_orbitalHubs.end())
        {
            it = m_orbitalTransfers.erase(it);
            continue;
        }
        const auto parentPosIt = m_celestialBodyPositionsAu.find(t.parentBodyId);
        const glm::dvec3 parentPos = parentPosIt != m_celestialBodyPositionsAu.end()
            ? parentPosIt->second * world::celestial::MetersPerAu : glm::dvec3(0.0);
        const auto parentVelIt = m_celestialBodyVelocitiesMetersPerSecond.find(t.parentBodyId);
        const glm::dvec3 parentVel = parentVelIt != m_celestialBodyVelocitiesMetersPerSecond.end() ? parentVelIt->second : glm::dvec3(0.0);

        if (t.abstracted)
        {
            if (m_serverTimelineClock.timeSeconds() >= t.transitEndRealSeconds)
            {
                const double now = m_orbitalUniverseTimeSeconds;
                const glm::dvec3 hp = orb::targetPositionAt(targetIt->second.motion, now);
                const glm::dvec3 hv = orb::targetVelocityAt(targetIt->second.motion, now);
                const glm::dvec3 prograde = glm::normalize(hv);
                const glm::dvec3 p = parentPos + hp - prograde * t.limits.approachOffsetMeters;
                const glm::dvec3 v = parentVel + hv + prograde * t.limits.closingSpeedMps;
                handOverToHub(it->first, *ship, t.targetHubId, &p, &v);
                it = m_orbitalTransfers.erase(it);
                continue;
            }
            ++it;
            continue;
        }

        // physical transfer: advance the carrier on the orbital clock (the destination ephemeris runs on it)
        double remaining = m_orbitalUniverseTimeSeconds - t.lastOrbitalTimeSeconds;
        t.lastOrbitalTimeSeconds = m_orbitalUniverseTimeSeconds;
        bool arrived = false;
        while (remaining > 1.0e-9)
        {
            double step = std::min(CarrierStepSeconds, remaining);
            if (t.thrustEnabled && t.carrier.timeSeconds + step >= t.plan.arrivalTimeSeconds)
            {
                step = std::max(0.0, t.plan.arrivalTimeSeconds - t.carrier.timeSeconds);
                orb::stepCarrier(t.plan, t.body, t.carrier, step, t.engineMaxAccelMps2, true);
                arrived = true;
                break;
            }
            orb::stepCarrier(t.plan, t.body, t.carrier, step, t.engineMaxAccelMps2, t.thrustEnabled);
            remaining -= step;
        }
        if (m_orbitalUniverseTimeSeconds - t.lastLogOrbitalSeconds >= 30.0)
        {
            t.lastLogOrbitalSeconds = m_orbitalUniverseTimeSeconds;
            const glm::dvec3 tv = orb::targetVelocityAt(targetIt->second.motion, t.carrier.timeSeconds);
            const auto ref = t.plan.evaluate(t.carrier.timeSeconds);
            std::cerr << "[OrbitalTransfer] ship " << it->first.value << " t+" << t.carrier.timeSeconds - t.plan.startTimeSeconds << " / "
                      << t.plan.durationSeconds() << " s: speed " << glm::length(t.carrier.velocityMps) / 1000.0 << " km/s (vs destination "
                      << glm::length(t.carrier.velocityMps - tv) / 1000.0 << "), alt " << (glm::length(t.carrier.positionMeters) - t.body.radiusMeters) / 1000.0
                      << " km, thrust " << glm::length(t.carrier.thrustMps2) << " m/s2, track err " << glm::length(ref.positionMeters - t.carrier.positionMeters) << " m\n";
        }
        motion.travelFrame.originMeters = parentPos + t.carrier.positionMeters;
        motion.travelFrame.linearVelocityMps = parentVel + t.carrier.velocityMps;
        motion.travelFrame.linearAccelerationMps2 = t.carrier.accelerationMps2;
        motion.travelFrame.valid = true;
        t.peakSpeedMps = std::max(t.peakSpeedMps, glm::length(t.carrier.velocityMps));
        if (arrived)
        {
            handOverToHub(it->first, *ship, t.targetHubId, nullptr, nullptr);
            it = m_orbitalTransfers.erase(it);
            continue;
        }
        // a coasting (cancelled) carrier that hits the atmosphere stops being a transfer
        if (glm::length(t.carrier.positionMeters) < t.body.radiusMeters + 80000.0)
        {
            std::cerr << "[OrbitalTransfer] ship " << it->first.value << ": entered the atmosphere while coasting\n";
            it = m_orbitalTransfers.erase(it);
            continue;
        }
        ++it;
    }
}
