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
// J autopilot (O on a body on the System Map, PAGE DOWN = land here): the ship
// flies itself to a body and lands. Its acceleration ramps exponentially (1 g
// doubling every 2 s up to ELITE_J_AUTOPILOT_MAX_ACCEL, default 100 km/s2 - a
// game drive, no speed cap, thrust both ways: the nose stays on the target);
// the velocity setpoint toward a point 12 km above the body on the ship's side
// is sqrt(2 a d) (braking at 0.4 of the cap) with an exponential approach tail
// d / 6 s; the departure body is never cut through. Then a powered vertical
// descent: down speed min(400, sqrt(2 15 h), h / 4) m/s, at least 1.5 m/s,
// horizontal velocity killed, the hull level (belly down, nose to the horizon),
// 40 m/s2 authority. Contact is the ordinary J surface contact. The drive is a
// game J drive applied to the free-flight frame - not the ship's main engine.
//
// HOME / END / INSERT latch the main thrust (full / brake flip-and-burn / off);
// they act through the ordinary '+' command and alignment controller
// (SharedShipPhysics). The pilot's velocity reference is the source hub's
// world velocity (HUD vector, Backspace exit, brake) - not the heliocentric one.
//
// Leaving free flight near a hub is a capture: only a hub whose capture
// envelope contains the ship (FreeFlightCaptureRadiusMeters) and only below
// the safe capture speed (the tactical envelope); it re-bases the ship into the
// hub frame with its actual relative position and velocity (no snap).
// Anywhere else J off keeps the global motion in the ship's own ballistic frame
// (non-rotating, free fall under gravity) and the pilot flies locally from rest
// inside it: global and local speed are separate (user order 2026-11-04).
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/ship/core/ShipDynamics.h"

namespace
{
// HubCaptureEnvelope (PROPOSAL until hubs carry their own traffic-control radius): the hub's
// approach / traffic zone in which its frame may capture a ship.
constexpr double FreeFlightCaptureRadiusMeters = 50000.0;

// J reference body (patched conics): the deepest body whose sphere of influence
// contains the point - Laplace r = d(parent) * (mu / mu_parent)^0.4. The parent
// is the catalog one ("...Земля.Луна" belongs to "...Земля": the Earth-Moon pair
// is one system, the Sun is not part of it even though it pulls the Moon
// harder); a planet's parent is the star. Earth's sphere ~925 000 km, the
// Moon's ~66 000 km. Beyond every planet's sphere the root (the star) holds the
// ship: planetary gravity negligible, the speed is the solar-system one.
const game::navigation::GravityBody* referenceBody(const glm::dvec3& point,
                                                   const std::vector<game::navigation::GravityBody>& bodies, bool& isRoot)
{
    const game::navigation::GravityBody* root = nullptr;
    for (const auto& b : bodies)
        if (b.gravitationalParameterM3s2 > 0.0 && (!root || b.gravitationalParameterM3s2 > root->gravitationalParameterM3s2))
            root = &b;
    const auto byId = [&](const std::string& id) -> const game::navigation::GravityBody* {
        for (const auto& b : bodies)
            if (b.id == id)
                return &b;
        return nullptr;
    };
    const game::navigation::GravityBody* best = root;
    double bestSphere = std::numeric_limits<double>::infinity();
    for (const auto& b : bodies)
    {
        if (&b == root || b.gravitationalParameterM3s2 <= 0.0)
            continue;
        // the catalog parent: the nearest enclosing id that is a body, else the star
        const game::navigation::GravityBody* parent = nullptr;
        for (std::string id = b.id; !parent;)
        {
            const auto dot = id.rfind('.');
            if (dot == std::string::npos)
                break;
            id.resize(dot);
            parent = byId(id);
        }
        if (!parent || parent->gravitationalParameterM3s2 <= b.gravitationalParameterM3s2)
            parent = root;
        if (!parent)
            continue;
        const double sphere = glm::length(parent->centerMeters - b.centerMeters) *
                              std::pow(b.gravitationalParameterM3s2 / parent->gravitationalParameterM3s2, 0.4);
        if (sphere < bestSphere && glm::length(point - b.centerMeters) < sphere)
        {
            bestSphere = sphere;
            best = &b;
        }
    }
    isRoot = best == root;
    return best;
}
}

void GameSimulation::setFreeFlightThrust(EntityId shipId, int command)
{
    Ship* ship = getShip(shipId);
    if (!ship || command < 0 || command > 2)
        return;
    auto& motion = ship->core().transform().motion;
    if (!motion.freeFlight)
        return;
    motion.freeFlightThrust = static_cast<std::uint8_t>(command);
    motion.freeFlightCaptureRefusal = 0;      // a new pilot action replaces the last capture message
    if (command != 2 && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    std::cerr << "[FreeFlight] ship " << shipId.value << " thrust "
              << (command == 1 ? "FULL (HOME)" : command == 2 ? "BRAKE (END)" : "OFF (INSERT)") << std::endl;
}

void GameSimulation::setFreeFlightAutopilot(EntityId shipId, const std::string& bodyId, bool engage)
{
    Ship* ship = getShip(shipId);
    const auto active = m_freeFlights.find(shipId);
    if (!ship || active == m_freeFlights.end())
        return;
    auto& f = active->second;
    auto& motion = ship->core().transform().motion;
    if (!engage)
    {
        if (!f.autopilotTargetId.empty())
            std::cerr << "[Autopilot] ship " << shipId.value << ": cancelled (" << f.autopilotTargetId << ")" << std::endl;
        f.autopilotTargetId.clear();
        f.autopilotPhase = 0;
        motion.freeFlightAutopilotPhase = 0;
        return;
    }
    const std::string target = bodyId.empty() ? f.primaryBodyId : bodyId;
    const bool known = std::any_of(m_gravityBodies.begin(), m_gravityBodies.end(),
                                   [&](const game::navigation::GravityBody& b) { return b.id == target; });
    if (target.empty() || !known)
    {
        std::cerr << "[Autopilot] ship " << shipId.value << ": no gravity body '" << target << "'" << std::endl;
        return;
    }
    f.autopilotTargetId = target;
    f.autopilotDepartureId = f.primaryBodyId;
    f.autopilotPhase = 2;
    f.autopilotSeconds = 0.0;
    f.autopilotHaveLastCenter = false;
    f.autopilotLastLogSeconds = -1.0e30;
    motion.freeFlightThrust = 0;
    motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    motion.freeFlightAutopilotTargetId = target;
    motion.freeFlightAutopilotPhase = 2;
    if (motion.freeFlightSurfaceState != 0)
    {
        motion.freeFlightSurfaceState = 0;   // lift off: the autopilot climbs away
    }
    std::cerr << "[Autopilot] ship " << shipId.value << ": ENGAGED -> " << target << " (departure " << f.autopilotDepartureId
              << ")" << std::endl;
}

void GameSimulation::updateFreeFlightAutopilot(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt)
{
    using game::navigation::GravityBody;
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    const auto find = [&](const std::string& id) -> const GravityBody* {
        for (const auto& b : m_gravityBodies)
            if (b.id == id)
                return &b;
        return nullptr;
    };
    const GravityBody* target = find(f.autopilotTargetId);
    if (!target)
    {
        f.autopilotTargetId.clear();
        motion.freeFlightAutopilotPhase = 0;
        return;
    }
    const bool haveVelocity = f.autopilotHaveLastCenter;
    const glm::dvec3 targetVelocity = haveVelocity ? (target->centerMeters - f.autopilotLastTargetCenter) / dt : glm::dvec3(0.0);
    f.autopilotLastTargetCenter = target->centerMeters;
    f.autopilotHaveLastCenter = true;
    if (!haveVelocity)
        return;
    f.autopilotSeconds += dt;

    static const double maxAccel = [] {
        const char* v = std::getenv("ELITE_J_AUTOPILOT_MAX_ACCEL");
        const double a = v && *v ? std::atof(v) : 0.0;
        return a > 0.0 ? a : 100000.0;
    }();
    constexpr double AimAltitudeM = 12000.0;
    constexpr double ApproachTauS = 6.0;
    constexpr double DescentAccelMps2 = 40.0;

    const glm::dvec3 rel = frame.originMeters - target->centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    const glm::dvec3 w = frame.linearVelocityMps - targetVelocity;
    const double ground = motion.freeFlightOrbitBodyId == target->id ? motion.freeFlightGroundHeightM : 0.0;
    const double height = rLen - target->radiusMeters - ground;
    // the drive: 1 g doubling every 2 s up to the cap
    const double ramp = std::min(maxAccel, 9.81 * std::exp2(f.autopilotSeconds / 2.0));

    glm::dvec3 wanted(0.0);
    double limit = ramp;
    int phase = f.autopilotPhase;
    if (phase == 4)
    {
        const double h = std::max(0.0, height);
        const double down = std::max(1.5, std::min({400.0, std::sqrt(2.0 * 15.0 * h), h / 4.0}));
        wanted = -up * down;
        limit = DescentAccelMps2;
    }
    else
    {
        const glm::dvec3 aim = target->centerMeters + up * (target->radiusMeters + AimAltitudeM);
        const glm::dvec3 toAim = aim - frame.originMeters;
        const double d = glm::length(toAim);
        glm::dvec3 dir = d > 1e-3 ? toAim / d : glm::dvec3(0.0);
        phase = 2;
        // departure: never cut through the body left behind - first move off the line
        if (const GravityBody* departure = find(f.autopilotDepartureId); departure && departure != target && d > 1.0)
        {
            const glm::dvec3 s = frame.originMeters - departure->centerMeters;
            const double t = std::clamp(-glm::dot(s, toAim) / (d * d), 0.0, 1.0);
            if (glm::length(s + toAim * t) < departure->radiusMeters * 1.3)
            {
                glm::dvec3 side = s - dir * glm::dot(s, dir);
                if (glm::length(side) < 1.0)
                    side = glm::cross(dir, std::abs(dir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
                dir = glm::normalize(side);
                phase = 1;
            }
        }
        const double braking = std::sqrt(2.0 * 0.4 * maxAccel * d);
        const double tail = d / ApproachTauS;
        if (phase == 2 && tail < braking)
            phase = 3;
        wanted = dir * std::min(braking, tail);
        if (phase == 3 && d < 1500.0 && glm::length(w) < 300.0)
        {
            phase = 4;
            std::cerr << "[Autopilot] ship " << shipId.value << ": DESCENT to " << target->id << " from " << height / 1000.0
                      << " km above the ground after " << f.autopilotSeconds << " s" << std::endl;
        }
    }
    f.autopilotPhase = phase;

    // deadbeat velocity servo within the drive's authority (gravity was integrated this tick)
    glm::dvec3 dv = wanted - w;
    const double maxDv = limit * dt;
    if (glm::length(dv) > maxDv)
        dv *= maxDv / glm::length(dv);
    frame.linearVelocityMps += dv;
    const glm::dvec3 accel = dv / dt;

    // the hull: in the transfer the nose points the drive (flip to brake); in the
    // descent the ship hangs level - belly to the ground, nose to the horizon -
    // so the pilot sees the surface coming (the drive pushes through the belly)
    glm::dvec3 noseWanted(0.0), upWanted = glm::dvec3(tr.up());
    if (phase == 4)
    {
        glm::dvec3 heading = glm::dvec3(tr.forward()) - up * glm::dot(glm::dvec3(tr.forward()), up);
        if (glm::length(heading) < 1.0e-3)
            heading = glm::dvec3(tr.up()) - up * glm::dot(glm::dvec3(tr.up()), up);   // nose was vertical
        if (glm::length(heading) < 1.0e-3)
            heading = glm::cross(up, std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
        noseWanted = glm::normalize(heading);
        upWanted = up;
    }
    else
    {
        // the J drive pushes both ways: the nose stays on the target the whole
        // transfer (the pilot watches the body grow), braking is reverse drive
        const glm::dvec3 toTarget = target->centerMeters + up * target->radiusMeters - frame.originMeters;
        if (phase == 1 && glm::length(accel) > 0.5)
            noseWanted = glm::normalize(accel);
        else if (glm::length(toTarget) > 1.0)
            noseWanted = glm::normalize(toTarget);
    }
    if (glm::length(noseWanted) > 0.5)
    {
        upWanted = upWanted - noseWanted * glm::dot(upWanted, noseWanted);
        if (glm::length(upWanted) < 1.0e-3)
            upWanted = glm::cross(noseWanted, std::abs(noseWanted.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
        upWanted = glm::normalize(upWanted);
        const glm::dvec3 rightWanted = glm::cross(noseWanted, upWanted);
        const glm::dquat current = glm::quat_cast(glm::dmat3(glm::mat3(tr.orientation)));
        glm::dquat wantedQ = glm::quat_cast(glm::dmat3(rightWanted, upWanted, -noseWanted));
        if (glm::dot(current, wantedQ) < 0.0)
            wantedQ = -wantedQ;   // the short way round
        const double angle = glm::angle(wantedQ * glm::inverse(current));
        const double step = 2.0 * dt;   // ~115 deg/s
        const glm::dquat next = angle > step ? glm::slerp(current, wantedQ, step / angle) : wantedQ;
        tr.orientation = glm::mat4(glm::mat3(glm::mat3_cast(next)));
        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    }

    motion.freeFlightAutopilotPhase = static_cast<std::uint8_t>(phase);
    motion.freeFlightAutopilotTargetId = target->id;
    motion.freeFlightAutopilotRangeM = height;
    motion.freeFlightAutopilotRelSpeedMps = glm::length(w + dv);
    // signed along the velocity: braking is a negative acceleration (the speed still
    // grows while it is positive, however small)
    const glm::dvec3 wAfter = w + dv;
    motion.freeFlightAutopilotAccelMps2 = static_cast<float>(
        glm::length(wAfter) > 1.0e-3 ? glm::dot(accel, glm::normalize(wAfter)) : glm::length(accel));
    if (f.autopilotSeconds - f.autopilotLastLogSeconds >= (phase == 4 ? 1.0 : 2.0))
    {
        f.autopilotLastLogSeconds = f.autopilotSeconds;
        std::cerr << "[Autopilot] ship " << shipId.value << " t+" << f.autopilotSeconds << " s phase " << phase << " -> "
                  << target->id << ": height " << height / 1000.0 << " km, speed " << glm::length(w + dv) / 1000.0
                  << " km/s, accel " << glm::length(accel) << " m/s2 (drive " << ramp << ")" << std::endl;
    }
}

bool GameSimulation::toggleFreeFlight(EntityId shipId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;

    auto active = m_freeFlights.find(shipId);
    if (active != m_freeFlights.end() && active->second.localFrame)
    {
        // J again from the local ballistic frame: back to free flight (re-based below)
        m_freeFlights.erase(active);
        active = m_freeFlights.end();
    }
    if (active != m_freeFlights.end())
    {
        // off = a capture: the nearest hub whose capture envelope contains the ship, at a safe speed
        const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        const glm::dvec3 shipVel = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
        std::string nearest;
        double best = std::numeric_limits<double>::infinity();
        glm::dvec3 nearestVel(0.0);
        for (const auto& [hubId, frame] : m_hubNavigationFrames)
        {
            if (!frame.valid || frame.systemId != motion.systemId)
                continue;
            const double d = glm::length(frame.originMeters - shipWorld);
            if (d < best)
            {
                best = d;
                nearest = hubId;
                nearestVel = frame.velocityMetersPerSecond;
            }
        }
        const double relSpeed = nearest.empty() ? 0.0 : glm::length(shipVel - nearestVel);
        const double safeSpeed = game::ship::controlledSpeedLimitMps(ship->core().effectivePhysics());
        if (nearest.empty() || best > FreeFlightCaptureRadiusMeters || relSpeed > safeSpeed)
        {
            // No hub can take the ship: it keeps its global motion in its own
            // ballistic frame (non-rotating, falling freely under gravity) and the
            // pilot flies locally from rest inside it - global and local speed apart.
            auto& f = active->second;
            f.localFrame = true;
            f.autopilotTargetId.clear();
            f.autopilotPhase = 0;
            motion.travelFrame.originMeters = shipWorld;
            motion.travelFrame.linearVelocityMps = shipVel;
            motion.localPositionMeters = glm::dvec3(0.0);
            motion.localVelocityMps = glm::dvec3(0.0);
            motion.freeFlight = false;
            motion.freeFlightThrust = 0;
            motion.freeFlightCaptureRefusal = 0;
            motion.freeFlightAutopilotPhase = 0;
            if (motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity ||
                motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity)
                motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
            std::cerr << "[FreeFlight] ship " << shipId.value << " OFF -> local ballistic frame: global speed "
                      << glm::length(shipVel) / 1000.0 << " km/s kept, local speed 0 (nearest hub " << nearest << " at "
                      << best / 1000.0 << " km, " << relSpeed << " m/s relative)" << std::endl;
            return true;
        }
        std::cerr << "[FreeFlight] ship " << shipId.value << " OFF after " << active->second.elapsedSeconds << " s, peak "
                  << active->second.peakSpeedMps / 1000.0 << " km/s; nearest hub " << nearest << " at " << best / 1000.0 << " km\n";
        motion.freeFlight = false;
        motion.freeFlightThrust = 0;
        motion.freeFlightCaptureRefusal = 0;
        motion.freeFlightReferenceId.clear();
        motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
        motion.freeFlightOrbitBodyId.clear();
        if (motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
            motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
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
    motion.freeFlightThrust = 0;
    motion.freeFlightCaptureRefusal = 0;
    // the pilot's velocity reference: the hub just left (relative speed ~0 at the start)
    motion.freeFlightReferenceId = motion.hubId;
    if (const auto* hub = hubNavigationFrame(motion.hubId); hub && hub->valid)
        motion.freeFlightReferenceVelocityMps = hub->velocityMetersPerSecond;
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
        if (!ship || (!ship->core().transform().motion.freeFlight && !it->second.localFrame))
        {
            it = m_freeFlights.erase(it);
            continue;
        }
        auto& f = it->second;
        auto& tr = ship->core().transform();
        auto& motion = tr.motion;
        auto& frame = motion.travelFrame;

        // 1. fold last tick's local motion into the frame (non-rotating frame: a pure
        //    translation) - not in the local ballistic frame, where the local state is the pilot's
        if (!f.localFrame)
        {
            frame.originMeters = frame.localToWorldPosition(motion.localPositionMeters);
            frame.linearVelocityMps = frame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
            motion.localPositionMeters = glm::dvec3(0.0);
            motion.localVelocityMps = glm::dvec3(0.0);
        }

        // 2. the frame advances by its velocity under the real gravity of all bodies (velocity Verlet)
        const auto g0 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.originMeters += frame.linearVelocityMps * dt + 0.5 * g0.accelerationMps2 * dt * dt;
        const auto g1 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.linearVelocityMps += 0.5 * (g0.accelerationMps2 + g1.accelerationMps2) * dt;
        frame.linearAccelerationMps2 = g1.accelerationMps2;
        frame.valid = true;
        if (f.localFrame)
        {
            // local flight inside the falling frame: nothing is folded, the frame
            // carries the global motion, the flight laws own the local state
            ++it;
            continue;
        }

        // the reference hub moves on its orbit: keep its current velocity
        if (const auto* hub = hubNavigationFrame(motion.freeFlightReferenceId); hub && hub->valid)
            motion.freeFlightReferenceVelocityMps = hub->velocityMetersPerSecond;

        f.elapsedSeconds += dt;
        const double speed = glm::length(frame.linearVelocityMps);
        f.peakSpeedMps = std::max(f.peakSpeedMps, speed);
        bool referenceIsRoot = false;
        const game::navigation::GravityBody* reference = referenceBody(frame.originMeters, m_gravityBodies, referenceIsRoot);
        f.primaryBodyId = reference ? reference->id : g1.primaryBodyId;
        f.primaryAltitudeMeters = reference ? glm::length(frame.originMeters - reference->centerMeters) - reference->radiusMeters
                                            : g1.primaryAltitudeMeters;
        motion.freeFlightReferenceIsRoot = referenceIsRoot ? 1 : 0;

        // orbit relative to the reference body (sphere of influence) (HUD: falling / climbing, circular speed, Pe / Ap)
        {
            // the J reference: speed, altitude, orbit, atmosphere and ground are all
            // against the body whose sphere of influence holds the ship
            const game::navigation::GravityBody* primary = reference;
            if (primary && f.lastPrimaryId != primary->id)
                std::cerr << "[FreeFlight] ship " << it->first.value << ": reference body -> " << primary->id
                          << (referenceIsRoot ? " (root: planetary gravity negligible, global speed)" : " (its sphere of influence)")
                          << " at " << glm::length(frame.originMeters - primary->centerMeters) / 1000.0 << " km from its centre"
                          << std::endl;
            const bool continuous = primary && f.lastPrimaryId == primary->id && dt > 0.0;
            if (primary && continuous && primary->gravitationalParameterM3s2 > 0.0)
            {
                const double mu = primary->gravitationalParameterM3s2;
                const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 r = frame.originMeters - primary->centerMeters;
                const glm::dvec3 v = frame.linearVelocityMps - bodyVelocity;
                const double rLen = std::max(1.0, glm::length(r));
                const double speed = glm::length(v);
                const double energy = 0.5 * speed * speed - mu / rLen;
                const double h = glm::length(glm::cross(r, v));
                motion.freeFlightOrbitBodyId = primary->id;
                motion.freeFlightOrbitAltitudeM = rLen - primary->radiusMeters;
                // J speed is flown against the reference body (HUD, alignment,
                // brake): Earth all the way to the Moon, the Moon inside its sphere, the star (global speed) between the planets
                motion.freeFlightReferenceId = primary->id;
                motion.freeFlightReferenceVelocityMps = bodyVelocity;
                motion.freeFlightOrbitSpeedMps = speed;
                motion.freeFlightOrbitRadialMps = glm::dot(v, r) / rLen;
                motion.freeFlightOrbitCircularMps = std::sqrt(mu / rLen);
                motion.freeFlightOrbitEscapeMps = std::sqrt(2.0 * mu / rLen);
                motion.freeFlightOrbitBound = energy < 0.0;
                if (motion.freeFlightOrbitBound)
                {
                    const double a = -mu / (2.0 * energy);
                    const double e = std::sqrt(std::max(0.0, 1.0 + 2.0 * energy * h * h / (mu * mu)));
                    motion.freeFlightOrbitPeriapsisAltM = a * (1.0 - e) - primary->radiusMeters;
                    motion.freeFlightOrbitApoapsisAltM = a * (1.0 + e) - primary->radiusMeters;
                }
                else
                {
                    const double p = h * h / mu;   // hyperbola: periapsis only
                    const double e = std::sqrt(std::max(0.0, 1.0 + 2.0 * energy * h * h / (mu * mu)));
                    motion.freeFlightOrbitPeriapsisAltM = p / (1.0 + e) - primary->radiusMeters;
                    motion.freeFlightOrbitApoapsisAltM = 0.0;
                }
            }
            else if (!primary)
            {
                motion.freeFlightOrbitBodyId.clear();
            }
            // DEV: ELITE_DEV_J_DROP="<altitude km>,<fraction of circular speed>" places the
            // ship once at that altitude, horizontal, at that speed (atmosphere / impact tests)
            if (primary && continuous && !f.devDropDone)
            {
                f.devDropDone = true;
                if (const char* drop = std::getenv("ELITE_DEV_J_DROP"); drop && *drop)
                {
                    double altKm = 120.0, fraction = 0.9;
                    std::sscanf(drop, "%lf,%lf", &altKm, &fraction);
                    const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                    const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                    glm::dvec3 vRel = frame.linearVelocityMps - bodyVelocity;
                    glm::dvec3 horizontal = vRel - rHat * glm::dot(vRel, rHat);
                    horizontal = glm::length(horizontal) > 1e-6 ? glm::normalize(horizontal) : glm::dvec3(1, 0, 0);
                    const double r = primary->radiusMeters + altKm * 1000.0;
                    frame.originMeters = primary->centerMeters + rHat * r;
                    frame.linearVelocityMps = bodyVelocity + horizontal * std::sqrt(primary->gravitationalParameterM3s2 / r) * fraction;
                    std::cerr << "[FreeFlight] DEV drop: altitude " << altKm << " km, " << fraction << " x circular" << std::endl;
                }
            }

            // ---- Assisted J: an orbital "aircraft" against the reference body
            if (primary && continuous && motion.freeFlightSurfaceState == 0)
            {
                const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                glm::dvec3 v = frame.linearVelocityMps - bodyVelocity;
                const auto& control = ship->core().control();
                const bool steering = std::abs(control.pitchInput) > 0.05f || std::abs(control.yawInput) > 0.05f;
                if (motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted && f.autopilotTargetId.empty())
                {
                    // the hull holds the velocity (prograde) when the pilot does not steer:
                    // thrust stays along the velocity while gravity bends the path
                    // roll is free: it releases the prograde hold like steering does (the
                    // alignment owns the whole attitude and swallowed the roll input), but
                    // it does not bend the velocity
                    const bool rolling = std::abs(control.rollInput) > 0.05f;
                    if (!steering && !rolling && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::None &&
                        motion.freeFlightThrust != 2)
                        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::ForwardToVelocity;
                    if ((steering || rolling) &&
                        motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity)
                        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
                    // Assisted J steers like bending the main velocity, not like Newton:
                    // the hull turns stiffly (pitch / yaw rate <= 4 deg/s) and may lead the
                    // velocity by at most 10 deg - the velocity's inertia holds it there;
                    // the velocity against the body follows the nose at the same speed
                    // (angle / 2 s, so ~4 deg/s in a held turn), in 3D, by the J drive.
                    // (Newtonian J keeps the free hull: thrust direction does the turning.)
                    // Not while braking (END flips the hull) and not slower than 50 m/s
                    // (hovering / landing: the velocity has no inertia to bend).
                    if (motion.freeFlightThrust != 2 &&
                        motion.velocityAlignmentMode != game::navigation::VelocityAlignmentMode::BackwardToVelocity &&
                        glm::length(v) > 50.0)
                    {
                        constexpr double HullRate = 4.0 * 3.14159265358979 / 180.0;
                        constexpr double LeadLimit = 10.0 * 3.14159265358979 / 180.0;
                        const double pr = tr.pitchRate, yr = tr.yawRate;
                        const double turn = std::sqrt(pr * pr + yr * yr);
                        if (turn > HullRate)
                        {
                            tr.pitchRate = static_cast<float>(pr * HullRate / turn);
                            tr.yawRate = static_cast<float>(yr * HullRate / turn);
                        }
                        const double speedRel = glm::length(v);
                        glm::dvec3 nose = glm::normalize(glm::dvec3(tr.forward()));
                        if (speedRel > 1.0)
                        {
                            const glm::dvec3 vHat = v / speedRel;
                            double angle = std::acos(std::clamp(glm::dot(vHat, nose), -1.0, 1.0));
                            glm::dvec3 axis = glm::cross(vHat, nose);
                            if (glm::length(axis) < 1.0e-9)
                                axis = glm::dvec3(tr.up());
                            axis = glm::normalize(axis);
                            if (angle > LeadLimit)
                            {
                                // the velocity's inertia: the nose stops at the lead limit
                                tr.orientation = glm::rotate(glm::mat4(1.0f), static_cast<float>(-(angle - LeadLimit)),
                                                             glm::vec3(axis)) * tr.orientation;
                                tr.pitchRate = tr.yawRate = 0.0f;
                                angle = LeadLimit;
                            }
                            if (steering && angle > 1.0e-5)
                            {
                                const double rate = std::max(glm::radians(0.5), angle / 2.0);
                                const double step = std::min(angle, rate * dt);
                                const glm::dvec3 turned = vHat * std::cos(step) + glm::cross(axis, vHat) * std::sin(step);
                                const glm::dvec3 dv = turned * speedRel - v;
                                frame.linearVelocityMps += dv;
                                v += dv;
                                if (std::fmod(f.elapsedSeconds, 1.0) < dt)
                                    std::cerr << "[FreeFlight] ship " << it->first.value << " steering: nose leads the velocity by "
                                              << glm::degrees(angle) << " deg, velocity turning " << glm::degrees(rate)
                                              << " deg/s at " << speedRel << " m/s vs " << primary->id << std::endl;
                            }
                        }
                    }
                }

                // ---- atmosphere, dense layers, solid surface
                // Exponential atmosphere rho = rho0 exp(-h / H), H = top / 12 (Earth: 100 km
                // top -> 8.3 km). rho0 = 1.225 kg/m3 for every body with an atmosphere until
                // the server reads the catalog surface pressure. Drag a = rho v^2 / (2 beta),
                // beta = m / (Cd A) = 800 kg/m2 (a blunt ship). The two thresholds are physical:
                //   safe altitude  - drag at circular speed reaches 0.1 m/s2 (the orbit decays fast)
                //   dense layers   - drag at circular speed reaches 1 g (heating, structural load)
                // Earth: ~108 km / ~71 km. Airless bodies: a 0.1 % R (min 1 km) terrain margin.
                constexpr double SeaLevelDensity = 1.225;
                constexpr double BallisticCoefficient = 800.0;
                const double atmosphereTop = std::max(0.0, primary->atmosphereRadiusMeters - primary->radiusMeters);
                const double scaleHeight = atmosphereTop / 12.0;
                const double vCircSurface = std::sqrt(primary->gravitationalParameterM3s2 / std::max(primary->radiusMeters, 1.0));
                const auto dragThresholdAltitude = [&](double accel) {
                    return scaleHeight * std::log(std::max(1.0, SeaLevelDensity * vCircSurface * vCircSurface /
                                                                    (2.0 * BallisticCoefficient * accel)));
                };
                const double safeAlt = atmosphereTop > 0.0 ? dragThresholdAltitude(0.1) : std::max(1000.0, primary->radiusMeters * 0.001);
                const double denseAlt = atmosphereTop > 0.0 ? dragThresholdAltitude(9.81) : 0.0;
                const double alt = motion.freeFlightOrbitAltitudeM;
                motion.freeFlightOrbitSafeAltM = safeAlt;
                motion.freeFlightOrbitDenseAltM = denseAlt;
                motion.freeFlightDragMps2 = 0.0;
                const double speed = glm::length(v);
                if (atmosphereTop > 0.0 && alt < atmosphereTop && speed > 0.1)
                {
                    const double density = SeaLevelDensity * std::exp(-std::max(alt, 0.0) / scaleHeight);
                    const double drag = std::min(density * speed * speed / (2.0 * BallisticCoefficient), speed / dt * 0.5);
                    frame.linearVelocityMps -= v / speed * drag * dt;
                    motion.freeFlightDragMps2 = drag;
                    // dense layers at speed: an impact on entry, then heating damage
                    // dense layers: deceleration above 1 g - an impact on entry, then heating
                    const bool dense = drag > 9.81;
                    if (dense && !f.insideDenseLayers)
                    {
                        ++motion.freeFlightImpactSerial;
                        for (const auto& m : ship->core().descriptor().modules)
                            if (m.subsystemId == "hull" || m.structuralKind == ModuleStructuralKind::StructuralFrame)
                                ship->core().moduleRuntime().addEquipmentDamage(
                                    m.moduleId, game::equipment::EquipmentDamageKind::Impact, 0.15);
                        std::cerr << "[FreeFlight] ship " << it->first.value << ": DENSE LAYERS of " << primary->id << " at "
                                  << speed / 1000.0 << " km/s, altitude " << alt / 1000.0 << " km - impact" << std::endl;
                    }
                    f.insideDenseLayers = dense;
                    if (dense)
                    {
                        f.heatingDamageSeconds += dt;
                        if (f.heatingDamageSeconds >= 1.0)
                        {
                            f.heatingDamageSeconds = 0.0;
                            const double heat = std::min(0.25, drag / 400.0 * (speed / 3000.0));
                            for (const auto& m : ship->core().descriptor().modules)
                                ship->core().moduleRuntime().addEquipmentDamage(
                                    m.moduleId, game::equipment::EquipmentDamageKind::Thermal, heat);
                        }
                    }
                }
                else
                {
                    f.insideDenseLayers = false;
                }

                // the real ground under the ship (terrain / sea of the rendered field) when low
                TerrainSample terrain;
                const bool terrainKnown = alt < 15000.0 && m_terrainHeight && m_terrainHeight(primary->id, rHat, terrain);
                // The hull, not its centre, meets the ground: the highest terrain under
                // the footprint (centre, nose / tail, both sides) plus the half height and
                // a gear clearance - the centre at ground + 1 m sank half the hull (and
                // the up-slope end) into the rendered surface.
                double ground = terrainKnown ? terrain.heightM : 0.0;
                {
                    glm::dvec3 half = ship->core().effectivePhysics().physicalActuatorHalfExtentsMeters;
                    if (half.x <= 0.0 || half.y <= 0.0 || half.z <= 0.0)
                        half = glm::dvec3(8.0, 3.0, 15.0);
                    if (terrainKnown && alt < ground + 2000.0)
                    {
                        const glm::dvec3 fwd = glm::dvec3(tr.forward()) - rHat * glm::dot(glm::dvec3(tr.forward()), rHat);
                        const glm::dvec3 side = glm::cross(rHat, glm::length(fwd) > 1.0e-6 ? glm::normalize(fwd) : glm::dvec3(tr.right()));
                        const glm::dvec3 nose = glm::length(fwd) > 1.0e-6 ? glm::normalize(fwd) : glm::cross(side, rHat);
                        const double r = primary->radiusMeters;
                        for (const glm::dvec3 offset : {nose * half.z, -nose * half.z, side * half.x, -side * half.x})
                        {
                            TerrainSample under;
                            if (m_terrainHeight(primary->id, glm::normalize(rHat * r + offset), under))
                                ground = std::max(ground, under.heightM);
                        }
                    }
                    ground += half.y + 0.5;   // the hull bottom on its gear
                }
                motion.freeFlightGroundHeightM = ground;
                motion.freeFlightGroundMaterial = static_cast<std::int16_t>(terrainKnown ? terrain.material : -1);
                motion.freeFlightGroundSlopeDeg = static_cast<float>(terrainKnown ? terrain.slopeDeg : 0.0);
                motion.freeFlightGroundFeature = static_cast<std::uint8_t>(terrainKnown ? terrain.feature : 0);

                // climbing out of the ground (lift-off over rising terrain): stay on top, no contact
                if (alt <= ground && glm::dot(v, rHat) > 0.0)
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + ground);
                // solid ground and sea: contact by vertical speed (crush deceleration over ~2 m)
                else if (alt <= ground)
                {
                    const double vVert = -glm::dot(v, rHat);
                    const double vHor = glm::length(v - rHat * glm::dot(v, rHat));
                    // the ground decides: soft regolith / sand / snow / soil give way (~3 m),
                    // rock / basalt / ice ~1.5 m, water a splashdown (~4 m, but the hull takes
                    // a beating), lava is lethal; a steep slope tips the ship over
                    const int material = terrainKnown ? terrain.material : 0;
                    double crushM = 1.5;
                    if (material == TerrainWater) crushM = 4.0;
                    else if (material == 2 || material == 3 || material == 4 || material == 7) crushM = 3.0;   // regolith, sand, soil, snow
                    const double crushG = vVert * vVert / (2.0 * crushM) / 9.81;
                    const double slope = terrainKnown ? terrain.slopeDeg : 0.0;
                    const bool lava = material == 9;
                    const bool soft = !lava && vVert < 8.0 && vHor < 40.0 && slope < 25.0 && material != TerrainWater;
                    // a soft touchdown is not an impact: no hull shake for it
                    if (!soft)
                        ++motion.freeFlightImpactSerial;
                    if (soft)
                        motion.freeFlightSurfaceState = 1;                 // landed
                    else
                    {
                        const bool survivable = !lava && crushG < 30.0 && vHor < 150.0;
                        motion.freeFlightSurfaceState = survivable ? 1 : 2;
                        double hit = survivable ? 0.35 : 1.0;
                        if (survivable && slope >= 25.0)
                            hit = std::max(hit, 0.5);                      // tipped over on the slope
                        if (survivable && material == TerrainWater)
                            hit = std::max(hit, 0.25);
                        for (const auto& m : ship->core().descriptor().modules)
                            ship->core().moduleRuntime().addEquipmentDamage(
                                m.moduleId, game::equipment::EquipmentDamageKind::Impact, hit);
                    }
                    std::cerr << "[FreeFlight] ship " << it->first.value << ": SURFACE CONTACT on " << primary->id
                              << " vertical " << vVert << " m/s horizontal " << vHor << " m/s (" << crushG << " g), ground material "
                              << material << " slope " << slope << " deg feature " << (terrainKnown ? terrain.feature : 0) << " - "
                              << (motion.freeFlightSurfaceState == 1 ? "landed / survivable" : "CRASH") << std::endl;
                    // rest on the surface, moving with the body
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + ground);
                    frame.linearVelocityMps = bodyVelocity;
                    motion.freeFlightThrust = 0;
                    ship->core().refreshRuntimeState();
                }
            }
            if (primary && motion.freeFlightSurfaceState != 0)
            {
                // on the surface: carried with the body until thrust lifts the ship off
                const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                const glm::dvec3 bodyVelocity = f.lastPrimaryId == primary->id && dt > 0.0
                    ? (primary->centerMeters - f.lastPrimaryCenterMeters) / dt : frame.linearVelocityMps;
                if (motion.freeFlightSurfaceState == 1 && motion.freeFlightThrust == 1)
                    motion.freeFlightSurfaceState = 0;                     // lift off
                else
                {
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + motion.freeFlightGroundHeightM);
                    frame.linearVelocityMps = bodyVelocity;
                }
            }
            // ---- J autopilot
            if (!f.autopilotTargetId.empty())
            {
                if (motion.freeFlightSurfaceState == 0)
                    updateFreeFlightAutopilot(it->first, *ship, f, dt);
                else
                {
                    std::cerr << "[Autopilot] ship " << it->first.value << ": "
                              << (motion.freeFlightSurfaceState == 1 ? "LANDED on " : "CRASHED on ") << f.autopilotTargetId << " after "
                              << f.autopilotSeconds << " s; ground material " << motion.freeFlightGroundMaterial << ", slope "
                              << motion.freeFlightGroundSlopeDeg << " deg" << std::endl;
                    f.autopilotTargetId.clear();
                    f.autopilotPhase = 0;
                    motion.freeFlightAutopilotPhase = 5;
                }
            }
            f.lastPrimaryId = primary ? primary->id : std::string();
            f.lastPrimaryCenterMeters = primary ? primary->centerMeters : glm::dvec3(0.0);
        }

        if (g1.insideAtmosphere || (!g1.primaryBodyId.empty() && g1.primaryAltitudeMeters < 0.0))
        {
            if (f.elapsedSeconds - f.lastLogSeconds >= 2.0)
            {
                f.lastLogSeconds = f.elapsedSeconds;
                std::cerr << "[FreeFlight] ship " << it->first.value << ": " << (g1.primaryAltitudeMeters < 0.0 ? "BELOW THE SURFACE of " : "in the atmosphere of ")
                          << g1.primaryBodyId << ", altitude " << g1.primaryAltitudeMeters / 1000.0 << " km" << std::endl;
            }
        }
        else if (f.elapsedSeconds - f.lastLogSeconds >= 5.0)
        {
            f.lastLogSeconds = f.elapsedSeconds;
            std::cerr << "[FreeFlight] ship " << it->first.value << " t+" << f.elapsedSeconds << " s: global speed " << speed / 1000.0
                      << " km/s, gravity " << glm::length(g1.accelerationMps2) << " m/s2 (primary " << g1.primaryBodyId << ", alt "
                      << g1.primaryAltitudeMeters / 1000.0 << " km), main " << glm::length(motion.mainEngineAccelerationMps2) << " m/s2 (requested "
                      << glm::length(motion.requestedMainEngineAccelerationMps2) << "), thrust cmd " << int(motion.freeFlightThrust)
                      << ", align " << int(motion.velocityAlignmentMode) << ", '+' " << tr.targetSpeedRate << ", vs reference "
                      << glm::length(frame.linearVelocityMps - motion.freeFlightReferenceVelocityMps) << " m/s" << std::endl;
        }
        ++it;
    }
}
