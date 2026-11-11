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
// doubling every 2 s up to ELITE_J_AUTOPILOT_MAX_ACCEL, default ~1e5 g, JDrive.h -
// the J drive, speed cap 0.9 c, thrust both ways: the nose stays on the target);
// near any object (hub, station, beacon, ship: JDrive.h keep-out) the J field does
// not work - the drive is a gentle push out of the keep-out first. A landing (author
// 2026-11-05): J brings the ship TANGENTIALLY onto a circular parking orbit at the
// target's J floor, in the plane of the approach and the site, the main-engine braking
// distance before the site; a coast on the orbit (stabilisation), then J OFF: the
// de-orbit and the landing are flown in NORMAL flight (author 2026-11-05: "скорость
// 400 м/с как в обычном режиме, а ты пользуешься J") - the body frame below, the
// landing virtual pilot (SurfaceLandingPilot) emitting ordinary Newtonian controls;
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
// Below a body's J floor J off is the BODY FRAME instead: normal flight against the
// body's surface. The travel frame moves with the ground under the ship (body velocity
// + its turn), so the local velocity the ordinary flight laws see and hold is the
// velocity over the ground; each tick the server folds what the laws did into the
// world motion, adds real gravity, re-bases the frame on the ship and splits again.
// Atmosphere, ground contact, rest and lift-off are the same as in J.
//
// FLIGHT ARCHITECTURE (author 2026-11-05 cc - the names above are the older ones). One physics;
// the modes change only what is called zero velocity, never the motion:
//   NORMAL flight  = what this file calls free flight (motion.freeFlight): no speed envelope,
//                    the velocity against space (the star) or the body of the sphere of influence;
//   STABILIZATION  = leaving it (Ctrl+K): a hub frame (capture), the ground frame (body frame,
//                    low and slow) or the ship's own ballistic frame (orbit) with the 500 m/s
//                    manoeuvre envelope; it engages by itself after a coast on a safe closed orbit
//                    (armed by the J engine going off / a new sphere / the orbit closing);
//   the J ENGINE   = Ctrl+J (motion.jEngine): the J field's acceleration (1 g doubling every 2 s
//                    up to the cap) along the main-engine thrust in place of it; only in normal
//                    flight, never near objects (JDrive.h keep-out) or below a body's J floor -
//                    refused there, and it drops out when the ship enters such a zone. The Assisted
//                    velocity bending by the J field works only with the J engine on.
#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/simulation/GameSimulation.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/game/simulation/JFieldIntegrity.h"
#include "src/game/navigation/rules/JumpRules.h"
#include "src/game/catalog/ships/ShipDesignCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/navigation/landing/SurfaceLandingPilot.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/core/ShipDynamics.h"

using game::navigation::AutopilotDisplayPhase;
using game::navigation::FlightRefusal;
using game::navigation::code;

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
    // ONE rule for the server, the client planner and the maps (BodyFlightRules.h)
    using B = game::navigation::GravityBody;
    const std::size_t k = game::navigation::rules::dominantBody(
        bodies, point, [](const B& b) -> const std::string& { return b.id; }, [](const B&) { return std::string(); },
        [](const B& b) { return b.gravitationalParameterM3s2; }, [](const B& b) { return b.centerMeters; }, &isRoot);
    return k < bodies.size() ? &bodies[k] : nullptr;
}
}

namespace
{
double mainEngineAccelMps2(const Ship& ship)
{
    return game::navigation::globalflight::mainEngineAccelMps2(ship.core().effectivePhysics());
}

double halfHeightAlong(const Ship& ship, const glm::dvec3& vertical)
{
    const auto& tr = ship.core().transform();
    return game::navigation::globalflight::halfHeightAlong(ship.core().effectivePhysics(), glm::dvec3(tr.right()), glm::dvec3(tr.up()),
                                                           glm::dvec3(tr.forward()), vertical);
}
}

namespace
{
game::navigation::jdrive::JFieldIntegrity jFieldOf(const Ship& ship)
{
    return game::simulation::jFieldIntegrity(
        static_cast<const world::modules::ObjectModuleRuntime&>(ship.core().moduleRuntime()));
}
} // namespace

bool GameSimulation::jKeepOut(EntityId shipId, int systemId, const glm::dvec3& worldMeters, JKeepOutHit& hit) const
{
    namespace jd = game::navigation::jdrive;
    bool found = false;
    double best = std::numeric_limits<double>::infinity();
    for (const auto& [hubId, frame] : m_hubNavigationFrames)
    {
        if (!frame.valid || frame.systemId != systemId)
            continue;
        const double d = glm::length(frame.originMeters - worldMeters);
        if (d < jd::KeepOutObjectM && d < best)
        {
            best = d;
            hit = JKeepOutHit {hubId, frame.originMeters, frame.velocityMetersPerSecond, d, false};
            found = true;
        }
    }
    for (const auto& [id, other] : m_ships)
    {
        if (id == shipId || !other)
            continue;
        const auto& m = other->core().transform().motion;
        if (m.systemId != systemId || !m.travelFrame.valid)
            continue;
        const glm::dvec3 p = m.travelFrame.localToWorldPosition(m.localPositionMeters);
        const double d = glm::length(p - worldMeters);
        if (d < jd::KeepOutShipM && d < best)
        {
            best = d;
            hit = JKeepOutHit {"ship " + std::to_string(id.value) + " (" + m.travelFrame.frameId + ", hub " + m.hubId + ")", p, m.travelFrame.localToWorldVelocity(m.localPositionMeters, m.localVelocityMps), d, true};
            found = true;
        }
    }
    return found;
}

void GameSimulation::setFreeFlightThrust(EntityId shipId, int command)
{
    Ship* ship = getShip(shipId);
    if (!ship || command < 0 || command > 2)
        return;
    auto& motion = ship->core().transform().motion;
    if (!motion.freeFlight)
        return;
    if (motion.jEngine && !motion.jStopping)
    {
        // J: HOME = full positive acceleration, END = full negative, INSERT = none
        namespace jd = game::navigation::jdrive;
        const double full = jd::MaxAccelMps2 * jd::jFieldFactor(jFieldOf(*ship));   // one emitter lost: half
        motion.jAccelCommandMps2 = command == 1 ? full : command == 2 ? -full : 0.0;
        motion.freeFlightThrust = 0;
        motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
        std::cerr << "[Flight] ship " << shipId.value << " J command "
                  << (command == 1 ? "FULL + (HOME)" : command == 2 ? "FULL - (END)" : "ZERO (INSERT)") << std::endl;
        return;
    }
    motion.freeFlightThrust = static_cast<std::uint8_t>(command);
    motion.freeFlightCaptureRefusal = code(FlightRefusal::None);      // a new pilot action replaces the last capture message
    if (command != 2 && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    std::cerr << "[FreeFlight] ship " << shipId.value << " thrust "
              << (command == 1 ? "FULL (HOME)" : command == 2 ? "BRAKE (END)" : "OFF (INSERT)") << std::endl;
}

void GameSimulation::applyGlobalFlightCommand(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const ShipControlState& cmd, double dt)
{
    namespace jd = game::navigation::jdrive;
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    if (dt <= 0.0 || f.localFrame)
        return;
    bool jField = false;
    if (cmd.globalJVelocityTargetValid)
    {
        // the command supersedes the pilot's J engine, main thrust latch and alignment
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
        motion.jEngine = false;
        motion.jStopping = false;
        motion.jAccelCommandMps2 = 0.0;
        motion.freeFlightThrust = 0;
        // the field's own ramp: from 1 g, doubling every DriveRampDoublingS, within the client's limit
        const double field = jd::jFieldFactor(jFieldOf(ship));   // emitter / core damage halves or removes J
        double cap = std::min({std::max(0.0, cmd.globalJAccelLimitMps2), jd::MaxAccelMps2 * field,
                               jd::StandardGravityMps2 * std::exp2(f.globalRampSeconds / jd::DriveRampDoublingS)});
        std::string why;
        const int blocked = jEngineBlocked(shipId, motion.systemId, frame.originMeters, why);
        if (blocked == code(FlightRefusal::JRefusedNearObject))
        {
            cap = std::min(cap, jd::GentleAccelMps2);   // only the gentle push out of a keep-out
            f.globalRampSeconds = 0.0;
        }
        else if (blocked != 0)
        {
            cap = 0.0;                                   // no J field below a J floor
            f.globalRampSeconds = 0.0;
            motion.freeFlightCaptureRefusal = code(FlightRefusal::BelowJFloorMainEngines);
        }
        else
            f.globalRampSeconds += dt;
        if (motion.jChargeT == 0.0 || motion.jumpCoreRatingT <= 0.0)
        {
            cap = 0.0;
            motion.freeFlightCaptureRefusal = code(FlightRefusal::JChargeEmpty);
        }
        glm::dvec3 dv = cmd.globalJVelocityTargetWorldMps - frame.linearVelocityMps;
        const double wantedDv = glm::length(dv);
        if (glm::length(dv) > cap * dt)
            dv *= cap * dt / glm::length(dv);
        frame.linearVelocityMps += dv;
        motion.jAccelMps2 = glm::length(dv) / dt;
        jField = true;
        f.globalLogSeconds += dt;
        if (f.globalLogSeconds >= 2.0)
        {
            f.globalLogSeconds = 0.0;
            std::cerr << "[Plant] ship " << shipId.value << ": J target " << wantedDv << " m/s off, client limit "
                      << cmd.globalJAccelLimitMps2 << " m/s2, cap " << cap << " m/s2 (block " << blocked << ", ramp "
                      << f.globalRampSeconds << " s) -> " << motion.jAccelMps2 << " m/s2" << std::endl;
        }
    }
    else
    {
        if (!motion.jEngine)
            motion.jAccelMps2 = 0.0;
        if (glm::length(cmd.globalMainAccelWorldMps2) > 0.0)
        {
            // the main engines, within the hull's real thrust
            const double main = mainEngineAccelMps2(ship);
            glm::dvec3 a = cmd.globalMainAccelWorldMps2;
            if (glm::length(a) > main)
                a *= main / glm::length(a);
            // on the ground: the engines lift the ship off once they push up harder than the weight
            if (motion.freeFlightSurfaceState == 1)
                for (const auto& b : m_gravityBodies)
                    if (b.id == f.primaryBodyId)
                    {
                        const glm::dvec3 r = frame.originMeters - b.centerMeters;
                        const double r2 = std::max(1.0, glm::dot(r, r));
                        if (glm::dot(a, r / std::sqrt(r2)) > b.gravitationalParameterM3s2 / r2)
                        {
                            motion.freeFlightSurfaceState = 0;
                            std::cerr << "[Plant] ship " << shipId.value << ": LIFT-OFF on the main engines from " << b.id << std::endl;
                        }
                        break;
                    }
            frame.linearVelocityMps += a * dt;
            motion.mainEngineAccelerationMps2 = a;
        }
    }
    if (cmd.globalAttitudeTargetValid && glm::length(cmd.globalAttitudeForward) > 1.0e-9)
    {
        // the J field turns the hull freely (~115 deg/s); on the main engines its own rate limits it
        const glm::dvec3 fwd = glm::normalize(cmd.globalAttitudeForward);
        glm::dvec3 up = cmd.globalAttitudeUp - fwd * glm::dot(cmd.globalAttitudeUp, fwd);
        up = glm::length(up) > 1.0e-9 ? glm::normalize(up) : glm::dvec3(tr.up());
        const glm::dvec3 right = glm::cross(fwd, up);
        const glm::dquat current = glm::quat_cast(glm::dmat3(glm::mat3(tr.orientation)));
        glm::dquat wanted = glm::quat_cast(glm::dmat3(right, up, -fwd));
        if (glm::dot(current, wanted) < 0.0)
            wanted = -wanted;   // the short way round
        const double angle = glm::angle(wanted * glm::inverse(current));
        const double rate = jField ? 2.0 : std::clamp(static_cast<double>(ship.core().effectivePhysics().maxPitchRate), 0.2, 2.0);
        const double step = rate * dt;
        const glm::dquat next = angle > step ? glm::slerp(current, wanted, step / angle) : wanted;
        tr.orientation = glm::mat4(glm::mat3(glm::mat3_cast(next)));
        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    }
}

void GameSimulation::applyGlobalFrameRequests()
{
    // the client's frame requests (ShipControlState::globalFrameRequest), idempotent: outside the
    // free-flight pass, which they may change (a hub frame / the ground frame <-> normal flight)
    namespace gf = game::navigation::globalflight;
    std::vector<std::pair<EntityId, std::uint8_t>> requests;
    for (const auto& [id, ship] : m_ships)
    {
        if (!ship || !passOwns(ship.get()))
            continue;
        const auto& control = ship->core().control();
        if (control.globalFrameRequest != 0)
            requests.emplace_back(id, control.globalFrameRequest);
        // Assisted free rotation (the landing pilot's Backspace mode), idempotent
        auto& motion = ship->core().transform().motion;
        if (control.globalFreeRotationRequest == 1 && !motion.assistedFreeRotation &&
            motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted)
        {
            motion.assistedFreeRotation = true;
            motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
        }
        else if (control.globalFreeRotationRequest == 2 && motion.assistedFreeRotation)
            motion.assistedFreeRotation = false;
    }
    for (const auto& [id, request] : requests)
    {
        Ship* ship = getShip(id);
        if (!ship)
            continue;
        auto& motion = ship->core().transform().motion;
        auto ff = m_freeFlights.find(id);
        if (request == static_cast<std::uint8_t>(gf::FrameRequest::NormalFlight))
        {
            if (ff == m_freeFlights.end() || ff->second.localFrame)
                if (toggleStabilization(id) && motion.freeFlightSurfaceState != 0)
                    motion.freeFlightSurfaceState = 0;   // lift off: the client's autopilot climbs away
        }
        else if (request == static_cast<std::uint8_t>(gf::FrameRequest::Stabilization))
        {
            // a leg's stop: the hub capture / the ground / the own orbit (toggleFreeFlight decides)
            if (ff != m_freeFlights.end() && !ff->second.localFrame)
                (void)toggleStabilization(id);
        }
        else if (request == static_cast<std::uint8_t>(gf::FrameRequest::GroundStabilization))
        {
            if (ff == m_freeFlights.end() || ff->second.localFrame || ff->second.ground.active)
                continue;
            for (const auto& b : m_gravityBodies)
                if (b.id == ff->second.primaryBodyId && b.radiusMeters > 0.0 &&
                    ff->second.primaryAltitudeMeters < game::navigation::jdrive::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters))
                {
                    enterBodyFrame(id, *ship, ff->second, b, motion.freeFlightReferenceVelocityMps);
                    break;
                }
        }
    }
}

void GameSimulation::enterBodyFrame(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const game::navigation::GravityBody& body,
                                    const glm::dvec3& bodyVelocityMps)
{
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    const glm::dvec3 shipWorld = frame.localToWorldPosition(motion.localPositionMeters);
    const glm::dvec3 shipVel = frame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    f.localFrame = true;
    f.ground.active = true;
    f.ground.id = body.id;
    f.ground.previousCentre = body.centerMeters;
    f.ground.havePrevious = true;
    f.ground.previousBasisValid = m_bodyOrientation && m_bodyOrientation(body.id, f.ground.previousBasis);
    frame.originMeters = shipWorld;
    frame.linearVelocityMps = bodyVelocityMps;   // the turn of the ground joins on the next split
    // the body frame folds and splits world vectors directly (local == world): an identity basis.
    // The frame inherited the hub's basis at the normal-flight start; with it the laws saw a
    // rotated velocity (live 2026-11-06: the terminal pilot pushed away from the site to 490 m/s).
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0);
    frame.valid = true;
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = shipVel - bodyVelocityMps;
    motion.freeFlight = false;
    motion.freeFlightThrust = 0;
    motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
    motion.jEngine = false;
    motion.velocityReferenceKind = 4;   // stabilization against the ground
    if (motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity ||
        motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity)
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    // the Assisted servo holds what the ship already does (no braking jump on the switch)
    motion.assistedTargetSpeedHold = false;
    motion.targetForwardSpeedMps = glm::dot(motion.localVelocityMps, glm::dvec3(tr.forward()));
    const glm::dvec3 rHat = glm::normalize(shipWorld - body.centerMeters);
    std::cerr << "[FreeFlight] ship " << shipId.value << " J OFF -> normal flight against the surface of " << body.id << ": altitude "
              << (glm::length(shipWorld - body.centerMeters) - body.radiusMeters) / 1000.0 << " km, speed over the body "
              << glm::length(motion.localVelocityMps) << " m/s (vertical " << glm::dot(motion.localVelocityMps, rHat) << ")" << std::endl;
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
            // below a body's J floor and within the manoeuvre envelope over it: stabilization
            // against the ground (the body frame); faster (an orbit) - the own ballistic frame
            const double overBody = glm::length(shipVel - motion.freeFlightReferenceVelocityMps);
            for (const auto& b : m_gravityBodies)
                if (b.id == active->second.primaryBodyId && b.radiusMeters > 0.0 && overBody <= safeSpeed &&
                    active->second.primaryAltitudeMeters <
                        game::navigation::jdrive::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters))
                {
                    auto& f = active->second;
                    enterBodyFrame(shipId, *ship, f, b, motion.freeFlightReferenceVelocityMps);
                    return true;
                }
            // No hub can take the ship: it keeps its global motion in its own
            // ballistic frame (non-rotating, falling freely under gravity) and the
            // pilot flies locally from rest inside it - global and local speed apart.
            auto& f = active->second;
            f.localFrame = true;
            motion.jEngine = false;
            motion.velocityReferenceKind = 3;   // stabilization on the own trajectory (orbit)
            motion.travelFrame.originMeters = shipWorld;
            motion.travelFrame.linearVelocityMps = shipVel;
            motion.localPositionMeters = glm::dvec3(0.0);
            motion.localVelocityMps = glm::dvec3(0.0);
            motion.freeFlight = false;
            motion.freeFlightThrust = 0;
            motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
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
        motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
        motion.jEngine = false;
        motion.velocityReferenceKind = 0;   // stabilization in the hub frame
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
        isDockStationControlledShip(shipId))
    {
        std::cerr << "[FreeFlight] ship " << shipId.value << ": not in a state for free flight (mode/frame/docked)\n";
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
    motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
    motion.jEngine = false;
    motion.velocityReferenceKind = 2;   // normal flight; space / the body from the first pass
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

int GameSimulation::jEngineBlocked(EntityId shipId, int systemId, const glm::dvec3& shipWorld, std::string& why) const
{
    namespace jd = game::navigation::jdrive;
    JKeepOutHit hit;
    if (jKeepOut(shipId, systemId, shipWorld, hit))
    {
        why = hit.objectId + " at " + std::to_string(static_cast<int>(hit.distanceMeters / 1000.0)) + " km (J needs " +
              std::to_string(static_cast<int>((hit.ship ? jd::KeepOutShipM : jd::KeepOutObjectM) / 1000.0)) + " km)";
        return code(FlightRefusal::JRefusedNearObject);
    }
    for (const auto& b : m_gravityBodies)
    {
        if (b.radiusMeters <= 0.0)
            continue;
        const double altitude = glm::length(shipWorld - b.centerMeters) - b.radiusMeters;
        const double floor = jd::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters);
        if (altitude < floor)
        {
            why = b.id + " altitude " + std::to_string(static_cast<int>(altitude / 1000.0)) + " km below its J floor " +
                  std::to_string(static_cast<int>(floor / 1000.0)) + " km";
            return code(FlightRefusal::JRefusedBelowFloor);
        }
    }
    return 0;
}

bool GameSimulation::toggleStabilization(EntityId shipId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& motion = ship->core().transform().motion;
    const auto active = m_freeFlights.find(shipId);
    if (active != m_freeFlights.end() && !active->second.localFrame)
    {
        // normal -> stabilization: a hub capture, the ground or the own orbit (toggleFreeFlight decides)
        motion.jEngine = false;
        std::cerr << "[Flight] ship " << shipId.value << ": STABILIZATION ON (pilot)" << std::endl;
        return toggleFreeFlight(shipId);
    }
    // stabilization -> normal flight (no envelope)
    motion.jEngine = false;
    const bool ok = toggleFreeFlight(shipId);
    if (ok)
        if (const auto f = m_freeFlights.find(shipId); f != m_freeFlights.end())
        {
            f->second.autoStabilizeArmed = false;   // the pilot left it: no automatic return here
            f->second.lastBound = true;
            f->second.coastSeconds = 0.0;
        }
    std::cerr << "[Flight] ship " << shipId.value << ": STABILIZATION OFF - normal flight" << (ok ? "" : " REFUSED") << std::endl;
    return ok;
}

bool GameSimulation::toggleJEngine(EntityId shipId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& motion = ship->core().transform().motion;
    auto active = m_freeFlights.find(shipId);
    if (motion.jEngine)
    {
        // off: the command drops to zero; the field winds down to the main engines' level and
        // only then the ship is in normal flight (the J block finishes it)
        motion.jAccelCommandMps2 = 0.0;
        motion.jStopping = true;
        std::cerr << "[Flight] ship " << shipId.value << ": J engine switching OFF - the field winds down from "
                  << std::abs(motion.jAccelMps2) / 9.81 << " g" << std::endl;
        return true;
    }
    if (game::navigation::jdrive::jFieldFactor(jFieldOf(*ship)) <= 0.0)
    {
        // the core or every field emitter lost (author 2026-11-10): no field to fly J with
        motion.freeFlightCaptureRefusal = code(FlightRefusal::JChargeEmpty);
        std::cerr << "[Flight] ship " << shipId.value << ": J engine REFUSED - no jump field (core / emitters lost)" << std::endl;
        return false;
    }
    if (ship->core().effectivePhysics().jumpCoreRatingT <= 0.0 || motion.jChargeT == 0.0)
    {
        motion.freeFlightCaptureRefusal = code(FlightRefusal::JChargeEmpty);
        std::cerr << "[Flight] ship " << shipId.value << ": J engine REFUSED - the jump-drive charge is empty" << std::endl;
        return false;
    }
    const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    std::string why;
    if (const int blocked = jEngineBlocked(shipId, motion.systemId, shipWorld, why))
    {
        motion.freeFlightCaptureRefusal = static_cast<std::uint8_t>(blocked);
        std::cerr << "[Flight] ship " << shipId.value << ": J engine REFUSED - " << why << std::endl;
        return false;
    }
    if (active == m_freeFlights.end() || active->second.localFrame)
    {
        // J works only in normal flight: leave the stabilization first
        if (!toggleStabilization(shipId))
            return false;
        active = m_freeFlights.find(shipId);
        if (active == m_freeFlights.end() || active->second.localFrame)
            return false;
    }
    motion.jEngine = true;
    motion.jStopping = false;
    motion.jAccelCommandMps2 = 0.0;
    motion.jAccelMps2 = 0.0;
    motion.freeFlightThrust = 0;   // the main engines idle under J
    motion.freeFlightCaptureRefusal = code(FlightRefusal::None);
    active->second.autoStabilizeArmed = false;
    std::cerr << "[Flight] ship " << shipId.value << ": J engine ON (normal flight; the thrust commands drive the J field)" << std::endl;
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
    applyGlobalFrameRequests();
    updateJumps(dt);
    // automatic stabilization decided last pass (a coast on a safe closed orbit)
    std::vector<EntityId> otherPasses;
    for (const EntityId id : std::exchange(m_pendingStabilize, {}))
        if (isShipDestroyed(id))
            continue;   // a wreck
        else if (!passOwns(id) && getShip(id))
            otherPasses.push_back(id);
        else if (const auto f = m_freeFlights.find(id); f != m_freeFlights.end() && !f->second.localFrame)
        {
            std::cerr << "[Flight] ship " << id.value << ": STABILIZATION ON (automatic: a closed orbit, coasting)" << std::endl;
            (void)toggleFreeFlight(id);
        }
    for (const EntityId id : otherPasses)
        m_pendingStabilize.push_back(id);
    updateHubServices(dt);
    for (auto it = m_freeFlights.begin(); it != m_freeFlights.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship || (!ship->core().transform().motion.freeFlight && !it->second.localFrame))
        {
            it = m_freeFlights.erase(it);
            continue;
        }
        if (!passOwns(ship))
        {
            ++it;   // another system's pass
            continue;
        }
        auto& f = it->second;
        auto& tr = ship->core().transform();
        auto& motion = tr.motion;
        auto& frame = motion.travelFrame;
        if (shipAwaitingJumpExit(it->first))
        {
            ++it;   // jumped: held until the new system's bodies place the exit (GameSimulationJump.cpp)
            continue;
        }

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
        f.ground.liftOff = false;
        if (f.ground.active)
        {
            // body frame: the frame moved with the ground (its velocity), the laws moved the
            // ship inside it; the world motion is both plus gravity - the frame is re-based on
            // the ship (local zero) for the shared physics below and split again at the end
            const glm::dvec3 local = motion.localPositionMeters;
            const glm::dvec3 localVel = motion.localVelocityMps;
            const glm::dvec3 upHere = glm::normalize(frame.originMeters - f.ground.previousCentre);
            f.ground.liftOff = glm::dot(localVel, upHere) > 0.3;
            if (std::getenv("ELITE_DEV_BODY_TRACE") && std::fmod(f.elapsedSeconds, 2.0) < dt)
                std::cerr << "[BodyTrace] ship " << it->first.value << " dt " << dt << " local dv " << glm::length(localVel - f.ground.traceLocalStart)
                          << " engines*dt " << glm::length(motion.engineAccelerationMps2) * dt << " requested main "
                          << glm::length(motion.requestedMainEngineAccelerationMps2) << " main " << glm::length(motion.mainEngineAccelerationMps2)
                          << " |local| " << glm::length(localVel) << std::endl;
            frame.originMeters += frame.linearVelocityMps * dt + local + 0.5 * g0.accelerationMps2 * dt * dt;
            frame.linearVelocityMps += localVel + g0.accelerationMps2 * dt;
            motion.localPositionMeters = glm::dvec3(0.0);
            motion.localVelocityMps = glm::dvec3(0.0);
        }
        else
            frame.originMeters += frame.linearVelocityMps * dt + 0.5 * g0.accelerationMps2 * dt * dt;
        const auto g1 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        if (!f.ground.active)
            frame.linearVelocityMps += 0.5 * (g0.accelerationMps2 + g1.accelerationMps2) * dt;
        frame.linearAccelerationMps2 = f.ground.active ? glm::dvec3(0.0) : g1.accelerationMps2;
        frame.valid = true;
        if (f.localFrame && !f.ground.active)
        {
            // local flight inside the falling frame: nothing is folded, the frame
            // carries the global motion, the flight laws own the local state
            motion.velocityReferenceKind = 3;
            f.elapsedSeconds += dt;
            if (f.elapsedSeconds - f.lastLogSeconds >= 10.0)
            {
                f.lastLogSeconds = f.elapsedSeconds;
                std::cerr << "[FreeFlight] ship " << it->first.value << " local frame: altitude over " << g1.primaryBodyId << " "
                          << g1.primaryAltitudeMeters / 1000.0 << " km" << std::endl;
            }
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
        motion.velocityReferenceKind = f.ground.active ? 4 : (referenceIsRoot ? 1 : 2);
        motion.mainEngineMaxMps2 = mainEngineAccelMps2(*ship);

        // ---- the client's global-flight command (NAVIGATION_V2 §1): the plant executes it within
        // the rules (the ground frame flies on the ordinary controls)
        const auto& control = ship->core().control();
        const bool globalCommand = control.globalFlightCommandValid && !f.ground.active;
        if (globalCommand)
        {
            f.globalIdleSeconds = 0.0;
            applyGlobalFlightCommand(it->first, *ship, f, control, dt);
        }
        else if ((f.globalIdleSeconds += dt) > 0.5)
            f.globalRampSeconds = 0.0;   // a short gap (one client pilot handing over to the next) keeps it

        // ---- the J engine: the J field along the main-engine thrust, in place of it
        if (motion.jEngine && !f.ground.active)
        {
            namespace jd = game::navigation::jdrive;
            std::string why;
            if (const int blocked = jEngineBlocked(it->first, motion.systemId, frame.originMeters, why))
            {
                motion.jEngine = false;
                motion.jStopping = false;
                motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
                f.autoStabilizeArmed = true;
                motion.freeFlightCaptureRefusal = code(blocked == code(FlightRefusal::JRefusedNearObject) ? FlightRefusal::JDroppedNearObject
                                                                                                                : FlightRefusal::BelowJFloorMainEngines);
                std::cerr << "[Flight] ship " << it->first.value << ": J engine OFF - the J field does not work here: " << why << std::endl;
            }
            else
            {
                // the pilot sets the acceleration: +/- change it exponentially (x2 per 0.5 s held,
                // from 1 g); the field follows the command exponentially (x2 per s up, x4 per s
                // down) - no step from 0 to 1e5 g
                const double g = jd::StandardGravityMps2;
                const double plus = ship->core().control().targetSpeedRate;
                double cmd = motion.jStopping ? 0.0 : motion.jAccelCommandMps2;
                if (!motion.jStopping && std::abs(plus) > 0.02)
                {
                    const double grow = std::exp2(dt / 0.5 * std::abs(plus));
                    const bool up = plus > 0.0;
                    if ((up && cmd >= 0.0) || (!up && cmd <= 0.0))
                        cmd = (up ? 1.0 : -1.0) * std::min(jd::MaxAccelMps2 * jd::jFieldFactor(jFieldOf(*ship)),
                                                           std::max(g, std::abs(cmd) * grow));
                    else
                        cmd = std::abs(cmd) / grow < g ? 0.0 : cmd / grow;
                    motion.jAccelCommandMps2 = cmd;
                }
                double a = motion.jAccelMps2;
                if (std::abs(cmd) < 1.0e-6 || (a != 0.0 && (a > 0.0) != (cmd > 0.0)))
                {
                    a /= std::exp2(dt * 2.0);                       // winding down (to zero or through it)
                    if (std::abs(a) < 0.5 * g)
                        a = 0.0;
                }
                else
                {
                    const double mag = std::abs(a) < g ? g : std::abs(a) * std::exp2(dt);
                    a = (cmd > 0.0 ? 1.0 : -1.0) * std::min(std::abs(cmd), mag);
                }
                // the J field along the nose; a negative one never pushes past rest against the reference
                const glm::dvec3 nose = glm::normalize(glm::dvec3(tr.forward()));
                const glm::dvec3 rel = frame.linearVelocityMps - motion.freeFlightReferenceVelocityMps;
                glm::dvec3 dv = nose * (a * dt);
                if (a < 0.0)
                {
                    const double along = glm::dot(rel, nose);
                    if (along + a * dt <= 0.0)
                    {
                        dv = -nose * std::max(0.0, along);
                        a = 0.0;
                        motion.jAccelCommandMps2 = 0.0;
                        std::cerr << "[Flight] ship " << it->first.value << ": J negative thrust stopped at rest against the reference"
                                  << std::endl;
                    }
                }
                frame.linearVelocityMps += dv;
                motion.jAccelMps2 = a;
                if (a != 0.0 && std::fmod(f.elapsedSeconds, 2.0) < dt)
                    std::cerr << "[Flight] ship " << it->first.value << ": J engine " << a / g << " g (command " << motion.jAccelCommandMps2 / g
                              << " g), speed vs reference " << glm::length(frame.linearVelocityMps - motion.freeFlightReferenceVelocityMps) / 1000.0
                              << " km/s" << std::endl;
                // switched off: normal flight once the field is down to the main engines' level
                if (motion.jStopping && std::abs(a) <= mainEngineAccelMps2(*ship))
                {
                    motion.jEngine = false;
                    motion.jStopping = false;
                    motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
                    f.autoStabilizeArmed = true;
                    std::cerr << "[Flight] ship " << it->first.value << ": J engine OFF (main engines)" << std::endl;
                }
            }
        }

        // ---- the J charge: the pilot's J field or the autopilot's J drive (not its main-engine de-orbit)
        {
            namespace jd = game::navigation::jdrive;
            const auto shipPhysics = ship->core().effectivePhysics();
            motion.shipMassT = shipPhysics.massKg / 1000.0;
            motion.jumpCoreRatingT = shipPhysics.jumpCoreRatingT;
            if (motion.jChargeT < 0.0)
            {
                motion.jChargeCapacityT = shipPhysics.jumpChargeCapacityT;
                motion.jChargeT = motion.jChargeCapacityT;
            }
            // the pilot's J engine or a client's global-flight command (the plant's J field)
            // (the body autopilot and a client's command: the plant's J field, motion.jAccelMps2)
            const double a = motion.jEngine || globalCommand ? motion.jAccelMps2 : 0.0;
            if (a != 0.0 && motion.jChargeT > 0.0)
            {
                const double massT = ship->core().effectivePhysics().massKg / 1000.0;
                motion.jChargeT = std::max(0.0, motion.jChargeT - jd::jChargeRateTps(a, massT, motion.jumpCoreRatingT) * dt);
                if (motion.jChargeT == 0.0 && motion.jEngine)
                {
                    // the field collapses: normal flight at once
                    motion.jEngine = false;
                    motion.jStopping = false;
                    motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
                    motion.freeFlightCaptureRefusal = code(FlightRefusal::JChargeEmpty);
                    f.autoStabilizeArmed = true;
                    std::cerr << "[Flight] ship " << it->first.value << ": J engine OFF - the jump-drive charge is empty" << std::endl;
                }
            }
        }

        // orbit relative to the reference body (sphere of influence) (HUD: falling / climbing, circular speed, Pe / Ap)
        {
            // the J reference: speed, altitude, orbit, atmosphere and ground are all
            // against the body whose sphere of influence holds the ship
            const game::navigation::GravityBody* primary = reference;
            if (primary && f.lastPrimaryId != primary->id && !f.lastPrimaryId.empty())
                f.autoStabilizeArmed = true;   // a new sphere of influence: its orbit may stabilize
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
                // ... unless the client's autopilot flies against a hub (a hub-to-hub route leg:
                // the departure hub inside its keep-out, then the destination - author 2026-11-08,
                // the stabilization's hub reference is kept; the Assisted nose follows that velocity)
                const auto& routeCmd = ship->core().control();
                const auto* refHub = routeCmd.globalFlightCommandValid && !routeCmd.globalReferenceHubId.empty() && !f.ground.active
                                         ? hubNavigationFrame(routeCmd.globalReferenceHubId)
                                         : nullptr;
                if (refHub && refHub->valid)
                {
                    motion.freeFlightReferenceId = routeCmd.globalReferenceHubId;
                    motion.freeFlightReferenceVelocityMps = refHub->velocityMetersPerSecond;
                    motion.velocityReferenceKind = 0;
                }
                else
                {
                    motion.freeFlightReferenceId = primary->id;
                    motion.freeFlightReferenceVelocityMps = bodyVelocity;
                }
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
                const bool bound = motion.freeFlightOrbitBound && !referenceIsRoot;
                if (bound && !f.lastBound)
                    f.autoStabilizeArmed = true;   // the orbit closed
                f.lastBound = bound;
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
                if (motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted && !!f.ground.active && !globalCommand)
                {
                    // the hull holds the velocity (prograde) when the pilot does not steer:
                    // thrust stays along the velocity while gravity bends the path
                    // roll is free: it releases the prograde hold like steering does (the
                    // alignment owns the whole attitude and swallowed the roll input), but
                    // it does not bend the velocity
                    const bool rolling = std::abs(control.rollInput) > 0.05f;
                    // Backspace (free rotation): the hull turns freely like a Newtonian one - no
                    // prograde hold, no lead limit, the velocity is not bent (author 2026-11-06)
                    const bool freeHull = motion.assistedFreeRotation;
                    if (freeHull && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity)
                        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
                    if (!freeHull && !steering && !rolling && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::None &&
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
                    // Normal flight (main engines) bends the same way, but only as fast as the main
                    // engines can turn the velocity (a_main / v): the nose may not run away from a
                    // velocity that turns slowly (author 2026-11-06: "надо как-то ограничивать угол
                    // поворота корабля")
                    if (!freeHull && motion.freeFlightThrust != 2 &&
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
                                double rate = std::max(glm::radians(0.5), angle / 2.0);
                                if (!motion.jEngine)
                                    rate = std::min(rate, mainEngineAccelMps2(*ship) / speedRel);   // what the main engines can turn
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
                {
                    // automatic stabilization: a coast (no thrust, no input) on a closed orbit whose
                    // periapsis clears the atmosphere / the terrain, once armed
                    const auto& c = ship->core().control();
                    const bool input = motion.freeFlightThrust != 0 || std::abs(tr.targetSpeedRate) > 0.02f ||
                                       std::abs(c.forwardInput) > 0.05f || std::abs(c.strafeInput) > 0.05f ||
                                       std::abs(c.liftInput) > 0.05f;
                    const bool coasting = !f.ground.active && !motion.jEngine && !globalCommand &&
                                          !input && glm::length(motion.engineAccelerationMps2) < 0.05;
                    f.coastSeconds = coasting ? f.coastSeconds + dt : 0.0;
                    const bool safeOrbit = motion.freeFlightOrbitBound && !referenceIsRoot &&
                                           motion.freeFlightOrbitPeriapsisAltM > std::max(safeAlt, motion.freeFlightGroundHeightM + 1000.0);
                    if (f.autoStabilizeArmed && coasting && safeOrbit && f.coastSeconds >= 5.0 && motion.destroyedCause == 0)
                    {
                        f.autoStabilizeArmed = false;
                        m_pendingStabilize.push_back(it->first);
                    }
                }
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
                    // beyond 30 g (the survivable crush of a surface contact) the hull breaks up
                    if (drag > 30.0 * 9.81 && motion.destroyedCause == 0)
                    {
                        const glm::dvec3 up = glm::normalize(frame.originMeters - primary->centerMeters);
                        destroyShipOnImpact(it->first, *ship, 2, speed, drag / 9.81,
                                            game::navigation::capsule::atmosphereBreakup(drag, speed, -glm::dot(v, up) / speed, scaleHeight,
                                                                                         primary->surfacePressurePa));
                    }
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
                        // the footprint: nose / tail, both sides and the four corners (a crater rim
                        // between the edge points poked into the wide hull)
                        for (const glm::dvec3 offset : {nose * half.z, -nose * half.z, side * half.x, -side * half.x,
                                                        nose * half.z + side * half.x, nose * half.z - side * half.x,
                                                        -nose * half.z + side * half.x, -nose * half.z - side * half.x})
                        {
                            TerrainSample under;
                            if (m_terrainHeight(primary->id, glm::normalize(rHat * r + offset), under))
                                ground = std::max(ground, under.heightM);
                        }
                    }
                    ground += halfHeightAlong(*ship, rHat) + 0.5;   // the hull bottom on its gear (any attitude)
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
                    // against the turning ground, not the body centre (Earth's equator
                    // moves 465 m/s: every landing there counted as a crash)
                    glm::dvec3 vg = v;
                    {
                        glm::dmat3 basisNow(1.0);
                        if (m_bodyOrientation && m_bodyOrientation(primary->id, basisNow) && f.haveLastBasis &&
                            f.lastBasisBodyId == primary->id && dt > 0.0)
                        {
                            const glm::dvec3 r = frame.originMeters - primary->centerMeters;
                            vg -= (basisNow * glm::transpose(f.lastBasis) * r - r) / dt;
                        }
                    }
                    const double vVert = -glm::dot(vg, rHat);
                    const double vHor = glm::length(vg - rHat * glm::dot(vg, rHat));
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
                        if (!survivable && motion.destroyedCause == 0)
                            destroyShipOnImpact(it->first, *ship, 1, glm::length(vg), crushG,
                                                game::navigation::capsule::surfaceImpact(vVert, glm::length(vg)));
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
                              << material << " slope " << slope << " deg feature " << (terrainKnown ? terrain.feature : 0)
                              << ", ground " << terrain.heightM << " m (30 m relief " << terrain.smoothHeightM << "), hull rests at "
                              << ground << " m - "
                              << (motion.freeFlightSurfaceState == 1 ? "landed / survivable" : "CRASH") << std::endl;
                    // rest on the surface, moving with the body (body-fixed: it turns with it)
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + ground);
                    {
                        glm::dmat3 worldFromBody(1.0);
                        f.landedBodyKnown = m_bodyOrientation && m_bodyOrientation(primary->id, worldFromBody);
                        f.landedBodyDirection = glm::transpose(worldFromBody) * rHat;
                        f.landedGroundM = ground;
                    }
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
                if (motion.freeFlightSurfaceState == 1 && (motion.freeFlightThrust == 1 || f.ground.liftOff))
                    motion.freeFlightSurfaceState = 0;                     // lift off (J thrust / the flight laws)
                else
                {
                    // body-fixed rest: the spot it touched down on turns with the body (a
                    // world-fixed direction let the Moon turn under it - the ship slid
                    // over the relief and sank where the ground was higher)
                    glm::dvec3 restDir = rHat;
                    double restGround = motion.freeFlightGroundHeightM;
                    glm::dmat3 worldFromBody(1.0);
                    if (f.landedBodyKnown && m_bodyOrientation && m_bodyOrientation(primary->id, worldFromBody))
                    {
                        restDir = glm::normalize(worldFromBody * f.landedBodyDirection);
                        restGround = f.landedGroundM;
                    }
                    frame.originMeters = primary->centerMeters + restDir * (primary->radiusMeters + restGround);
                    frame.linearVelocityMps = bodyVelocity;
                }
            }
            if (primary)
            {
                glm::dmat3 basis(1.0);
                f.haveLastBasis = m_bodyOrientation && m_bodyOrientation(primary->id, basis);
                f.lastBasis = basis;
                f.lastBasisBodyId = primary->id;
            }
            // the J speed cap: the field does not carry the ship above 0.9 c against the reference body
            if (primary && f.lastPrimaryId == primary->id && dt > 0.0 && !f.ground.active)
            {
                const glm::dvec3 refVel = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 rel = frame.linearVelocityMps - refVel;
                // the cap shrinks with the field (one emitter lost: half); no field: the J cap does not bind here,
                // the ship is on its main engines
                const double fieldCap = game::navigation::jdrive::SpeedCapMps *
                    std::max(0.5, game::navigation::jdrive::jFieldFactor(jFieldOf(*ship)));
                if (glm::length(rel) > fieldCap)
                    frame.linearVelocityMps = refVel + rel * (fieldCap / glm::length(rel));
            }
            f.lastPrimaryId = primary ? primary->id : std::string();
            f.lastPrimaryCenterMeters = primary ? primary->centerMeters : glm::dvec3(0.0);
        }

        if (f.ground.active)
        {
            // split again: the frame moves with the ground under the ship (the body's velocity
            // and its turn), the local velocity is the velocity over the ground
            const game::navigation::GravityBody* body = nullptr;
            for (const auto& b : m_gravityBodies)
                if (b.id == f.ground.id)
                    body = &b;
            if (body)
            {
                const glm::dvec3 shipVel = frame.linearVelocityMps + motion.localVelocityMps;
                glm::dvec3 ground = f.ground.havePrevious && dt > 0.0 ? (body->centerMeters - f.ground.previousCentre) / dt
                                                                  : motion.freeFlightReferenceVelocityMps;
                glm::dmat3 basis(1.0);
                const bool basisValid = m_bodyOrientation && m_bodyOrientation(body->id, basis);
                const glm::dvec3 r = frame.originMeters - body->centerMeters;
                if (basisValid && f.ground.previousBasisValid && dt > 0.0)
                    ground += (basis * glm::transpose(f.ground.previousBasis) * r - r) / dt;
                f.ground.previousBasis = basis;
                f.ground.previousBasisValid = basisValid;
                f.ground.previousCentre = body->centerMeters;
                f.ground.havePrevious = true;
                frame.linearVelocityMps = ground;
                motion.localVelocityMps = motion.freeFlightSurfaceState != 0 ? glm::dvec3(0.0) : shipVel - ground;
                f.ground.traceLocalStart = motion.localVelocityMps;
                motion.localPositionMeters = glm::dvec3(0.0);
            }
        }

        // below the ground (the terrain when it is sampled - a mare lies below the reference sphere)
        const double groundRef = motion.freeFlightGroundMaterial >= 0 ? motion.freeFlightGroundHeightM - 5.0 : 0.0;
        const bool belowGround = !g1.primaryBodyId.empty() && g1.primaryAltitudeMeters < groundRef;
        if (g1.insideAtmosphere || belowGround)
        {
            if (f.elapsedSeconds - f.lastLogSeconds >= 2.0)
            {
                f.lastLogSeconds = f.elapsedSeconds;
                std::cerr << "[FreeFlight] ship " << it->first.value << ": " << (belowGround ? "BELOW THE SURFACE of " : "in the atmosphere of ")
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

// re-base a ship into a hub's frame with its actual relative position / velocity (no snap)
void GameSimulation::destroyShipOnImpact(EntityId shipId, Ship& ship, std::uint8_t cause, double speedMps, double decelerationG,
                                         const game::navigation::capsule::Outcome& capsule)
{
    namespace cap = game::navigation::capsule;
    auto& motion = ship.core().transform().motion;
    if (motion.destroyedCause != 0)
        return;
    motion.destroyedCause = cause;
    // the escape capsule (the cockpit of a small frontier ship): only a ship that carries one
    const auto* cockpit = ship.core().moduleRuntime().findModule("ship_cockpit");
    const bool hasCapsule = cockpit && cockpit->detachable;
    motion.capsuleState = static_cast<std::uint8_t>(hasCapsule ? capsule.state : cap::State::NoCapsule);
    motion.capsuleBodyG = static_cast<float>(capsule.bodyG);
    std::cerr << "[FreeFlight] ship " << shipId.value << ": DESTROYED - "
              << (cause == 1 ? "surface impact" : "dense atmosphere") << " at " << speedMps << " m/s, " << decelerationG
              << " g; " << (hasCapsule ? "capsule: " : "") << cap::stateText(static_cast<cap::State>(motion.capsuleState))
              << " (pilot " << capsule.bodyG << " g, limit " << cap::BodyLimitG << " g)" << std::endl;
    cancelJump(shipId, static_cast<std::uint8_t>(game::navigation::jump::Refusal::ShipDestroyed));
    motion.jEngine = false;
    motion.jStopping = false;
    motion.freeFlightThrust = 0;
    motion.freeFlightAutopilotPhase = 0;
    ship.setControlState(ShipControlState {});
    ship.core().destroyWholeShip();
    markShipGraphDirty(shipId);
    // an NPC hull loses its pilot / AI (a player's session keeps its ship: the HUD tells the loss)
    if (!isPlayerControlled(shipId))
        (void)markShipDerelict(shipId);
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
    std::cerr << "[Flight] ship " << shipId.value << " handed over to " << hubId << ": local offset "
              << glm::length(local.positionMeters) / 1000.0 << " km, relative speed " << glm::length(local.velocityMps) << " m/s\n";
}
