// Ctrl+J free flight: the ship's own global motion (server).
//
// Hub Tactical keeps the pilot inside a moving hub frame with the 500 m/s
// controlled envelope. Free flight releases that envelope without raising
// maxCombatSpeed: the ship's travel frame is detached from the hub (the
// TravelFrameSystem seam, as Orbital Transfer does) and from then on carries
// the ship's GLOBAL motion - position and velocity in the system's world
// frame (the galaxy reference), integrated under the real gravity of every
// body (GravityFieldSystem). Each tick, before the ship's own flight step,
// the local motion of the previous tick is folded into the frame and the
// frame advances by its velocity and gravity; the ordinary flight laws then
// act on a local state that starts at rest, so thrust accumulates without a
// cap and nothing pulls the velocity back to the envelope. The laws read
// motion.freeFlight: +/- are direct main thrust, alignment (Backspace exit)
// follows the global velocity, turns under thrust are rate-limited.
//
// Leaving free flight re-bases the ship into the nearest hub frame with its
// actual relative position and velocity (no snap, no velocity erase).
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/TravelFrameSystem.h"

bool GameSimulation::toggleFreeFlight(EntityId shipId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;

    const auto active = m_freeFlights.find(shipId);
    if (active != m_freeFlights.end())
    {
        // off: re-base into the nearest hub of this system with the actual relative kinematics
        const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        std::string nearest;
        double best = std::numeric_limits<double>::infinity();
        for (const auto& [hubId, frame] : m_hubNavigationFrames)
        {
            if (!frame.valid || frame.systemId != motion.systemId)
                continue;
            const double d = glm::length(frame.originMeters - shipWorld);
            if (d < best)
            {
                best = d;
                nearest = hubId;
            }
        }
        if (nearest.empty())
        {
            std::cerr << "[FreeFlight] ship " << shipId.value << ": no hub frame in this system to return to; staying in free flight\n";
            return false;
        }
        std::cerr << "[FreeFlight] ship " << shipId.value << " OFF after " << active->second.elapsedSeconds << " s, peak "
                  << active->second.peakSpeedMps / 1000.0 << " km/s; nearest hub " << nearest << " at " << best / 1000.0 << " km\n";
        motion.freeFlight = false;
        m_freeFlights.erase(active);
        handOverToHub(shipId, *ship, nearest, nullptr, nullptr);
        return true;
    }

    if (motion.mode != game::navigation::MotionMode::HubTactical || !motion.travelFrame.valid ||
        m_orbitalTransfers.count(shipId) || isDockStationControlledShip(shipId))
    {
        std::cerr << "[FreeFlight] ship " << shipId.value << ": not in a state for free flight (mode/frame/docked/in transfer)\n";
        return false;
    }

    // on: detach and re-base at the ship (non-rotating, same instantaneous world kinematics)
    const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    const glm::dvec3 shipWorldVel = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    game::navigation::KinematicFrame frame = motion.travelFrame;
    frame.originMeters = shipWorld;
    frame.linearVelocityMps = shipWorldVel;
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0);
    frame.frameId = "ship_free_" + std::to_string(shipId.value);
    game::navigation::TravelFrameSystem::detach(motion);
    motion.travelFrame = frame;
    motion.travelFrame.valid = true;
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    motion.freeFlight = true;
    // the Assisted VREL servo means nothing here: no held setpoint survives into free flight
    motion.assistedTargetSpeedHold = false;
    motion.assistedThrottleTrimWasActive = false;
    motion.targetForwardSpeedMps = 0.0;

    ActiveFreeFlight f;
    f.sourceHubId = motion.hubId;
    m_freeFlights[shipId] = f;
    std::cerr << "[FreeFlight] ship " << shipId.value << " ON at hub " << motion.hubId << ": global speed "
              << glm::length(shipWorldVel) / 1000.0 << " km/s\n";
    return true;
}

const GameSimulation::ActiveFreeFlight* GameSimulation::freeFlight(EntityId shipId) const
{
    const auto it = m_freeFlights.find(shipId);
    return it == m_freeFlights.end() ? nullptr : &it->second;
}

void GameSimulation::updateFreeFlights(double dt)
{
    if (dt <= 0.0)
        return;
    for (auto it = m_freeFlights.begin(); it != m_freeFlights.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship || !ship->core().transform().motion.freeFlight)
        {
            it = m_freeFlights.erase(it);
            continue;
        }
        auto& f = it->second;
        auto& tr = ship->core().transform();
        auto& motion = tr.motion;
        auto& frame = motion.travelFrame;

        // 1. fold last tick's local motion into the frame (non-rotating frame: a pure translation)
        frame.originMeters = frame.localToWorldPosition(motion.localPositionMeters);
        frame.linearVelocityMps = frame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
        motion.localPositionMeters = glm::dvec3(0.0);
        motion.localVelocityMps = glm::dvec3(0.0);

        // 2. the frame advances by its velocity under the real gravity of all bodies (velocity Verlet)
        const auto g0 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.originMeters += frame.linearVelocityMps * dt + 0.5 * g0.accelerationMps2 * dt * dt;
        const auto g1 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.linearVelocityMps += 0.5 * (g0.accelerationMps2 + g1.accelerationMps2) * dt;
        frame.linearAccelerationMps2 = g1.accelerationMps2;
        frame.valid = true;

        f.elapsedSeconds += dt;
        const double speed = glm::length(frame.linearVelocityMps);
        f.peakSpeedMps = std::max(f.peakSpeedMps, speed);
        f.primaryBodyId = g1.primaryBodyId;
        f.primaryAltitudeMeters = g1.primaryAltitudeMeters;

        if (g1.insideAtmosphere || (!g1.primaryBodyId.empty() && g1.primaryAltitudeMeters < 0.0))
        {
            if (f.elapsedSeconds - f.lastLogSeconds >= 2.0)
            {
                f.lastLogSeconds = f.elapsedSeconds;
                std::cerr << "[FreeFlight] ship " << it->first.value << ": " << (g1.primaryAltitudeMeters < 0.0 ? "BELOW THE SURFACE of " : "in the atmosphere of ")
                          << g1.primaryBodyId << ", altitude " << g1.primaryAltitudeMeters / 1000.0 << " km (no atmosphere / impact model yet)\n";
            }
        }
        else if (f.elapsedSeconds - f.lastLogSeconds >= 30.0)
        {
            f.lastLogSeconds = f.elapsedSeconds;
            std::cerr << "[FreeFlight] ship " << it->first.value << " t+" << f.elapsedSeconds << " s: global speed " << speed / 1000.0
                      << " km/s, gravity " << glm::length(g1.accelerationMps2) << " m/s2 (primary " << g1.primaryBodyId << ", alt "
                      << g1.primaryAltitudeMeters / 1000.0 << " km), main " << glm::length(motion.mainEngineAccelerationMps2) << " m/s2\n";
        }
        ++it;
    }
}
