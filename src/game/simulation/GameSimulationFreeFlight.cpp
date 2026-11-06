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
#include "src/game/simulation/GameSimulation.h"
#include "src/game/simulation/JDrive.h"
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

namespace
{
// the ship's own main-engine acceleration (forward drive); a legacy descriptor without it
// falls back to its linear g envelope
double mainEngineAccelMps2(const Ship& ship)
{
    const auto p = ship.core().effectivePhysics();
    if (p.forwardMainEngineAccelerationMps2 > 0.0f)
        return p.forwardMainEngineAccelerationMps2;
    const double gs = p.maxLinearGs > 0.0f ? p.maxLinearGs : p.maxGs;
    return std::max(1.0, gs * game::simulation::jdrive::StandardGravityMps2);
}

// the hull's half height along a vertical: the box projected on it (belly down = half.y,
// tail down = half.z)
double halfHeightAlong(const Ship& ship, const glm::dvec3& vertical)
{
    glm::dvec3 half = ship.core().effectivePhysics().physicalActuatorHalfExtentsMeters;
    if (half.x <= 0.0 || half.y <= 0.0 || half.z <= 0.0)
        half = glm::dvec3(8.0, 3.0, 15.0);
    const auto& tr = ship.core().transform();
    return std::abs(glm::dot(glm::dvec3(tr.right()), vertical)) * half.x + std::abs(glm::dot(glm::dvec3(tr.up()), vertical)) * half.y +
           std::abs(glm::dot(glm::dvec3(tr.forward()), vertical)) * half.z;
}
}

bool GameSimulation::jKeepOut(EntityId shipId, int systemId, const glm::dvec3& worldMeters, JKeepOutHit& hit) const
{
    namespace jd = game::simulation::jdrive;
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
        namespace jd = game::simulation::jdrive;
        motion.jAccelCommandMps2 = command == 1 ? jd::MaxAccelMps2 : command == 2 ? -jd::MaxAccelMps2 : 0.0;
        motion.freeFlightThrust = 0;
        motion.freeFlightCaptureRefusal = 0;
        std::cerr << "[Flight] ship " << shipId.value << " J command "
                  << (command == 1 ? "FULL + (HOME)" : command == 2 ? "FULL - (END)" : "ZERO (INSERT)") << std::endl;
        return;
    }
    motion.freeFlightThrust = static_cast<std::uint8_t>(command);
    motion.freeFlightCaptureRefusal = 0;      // a new pilot action replaces the last capture message
    if (command != 2 && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    std::cerr << "[FreeFlight] ship " << shipId.value << " thrust "
              << (command == 1 ? "FULL (HOME)" : command == 2 ? "BRAKE (END)" : "OFF (INSERT)") << std::endl;
}

void GameSimulation::setFreeFlightAutopilot(EntityId shipId, const std::string& bodyId, bool engage, const std::string& landingSpec)
{
    Ship* ship = getShip(shipId);
    auto active = m_freeFlights.find(shipId);
    if (ship && engage && active == m_freeFlights.end())
    {
        // in a hub frame (stabilization): the autopilot leaves it itself - normal flight first
        if (!toggleStabilization(shipId))
            return;
        active = m_freeFlights.find(shipId);
    }
    if (!ship || active == m_freeFlights.end())
        return;
    auto& motion = ship->core().transform().motion;
    if (!engage)
    {
        auto& f = active->second;
        if (!f.autopilotTargetId.empty())
            std::cerr << "[Autopilot] ship " << shipId.value << ": cancelled (" << f.autopilotTargetId << ")" << std::endl;
        f.autopilotTargetId.clear();
        f.autopilotPhase = 0;
        if (f.pilotActive)
        {
            f.pilotActive = false;
            f.pilotRestoreLaw = true;
        }
        motion.freeFlightAutopilotPhase = 0;
        return;
    }
    if (active->second.localFrame)
    {
        // the J autopilot flies with the J field: J on again first
        (void)toggleFreeFlight(shipId);
        active = m_freeFlights.find(shipId);
        if (active == m_freeFlights.end() || active->second.localFrame)
            return;
    }
    auto& f = active->second;
    // a settlement ("site:<catalog id>"): its body from the catalog
    const bool toSettlement = landingSpec.rfind("site:", 0) == 0;
    const std::string settlementBody = toSettlement ? world::celestial::settlements::surfaceSettlementBody(landingSpec.substr(5))
                                                    : std::string();
    if (toSettlement && settlementBody.empty())
    {
        std::cerr << "[Autopilot] ship " << shipId.value << ": no surface settlement '" << landingSpec.substr(5) << "'" << std::endl;
        return;
    }
    const std::string target = toSettlement ? settlementBody : bodyId.empty() ? f.primaryBodyId : bodyId;
    const bool known = std::any_of(m_gravityBodies.begin(), m_gravityBodies.end(),
                                   [&](const game::navigation::GravityBody& b) { return b.id == target; });
    if (target.empty() || !known)
    {
        std::cerr << "[Autopilot] ship " << shipId.value << ": no gravity body '" << target << "'" << std::endl;
        return;
    }
    // a landing must be survivable and reversible: main engines vs surface gravity; an atmosphere takes shuttles only
    for (const auto& b : m_gravityBodies)
    {
        if (b.id != target || b.radiusMeters <= 0.0)
            continue;
        namespace jd = game::simulation::jdrive;
        const double gSurface = b.gravitationalParameterM3s2 / (b.radiusMeters * b.radiusMeters);
        const double main = mainEngineAccelMps2(*ship);
        const auto* design = game::catalog::ships::ShipDesignCatalog::instance().findShip(ship->core().desc().catalog.passportId);
        const bool shuttle = design && design->shipClass == "surface_lifter";
        if (b.atmosphereRadiusMeters > b.radiusMeters && !shuttle)
        {
            motion.freeFlightCaptureRefusal = 6;
            std::cerr << "[Autopilot] ship " << shipId.value << ": REFUSED " << target << " - an atmosphere: only shuttles land" << std::endl;
            return;
        }
        if (main < jd::LandingThrustToWeightMin * gSurface)
        {
            motion.freeFlightCaptureRefusal = 5;
            std::cerr << "[Autopilot] ship " << shipId.value << ": REFUSED " << target << " - main engines " << main / 9.81
                      << " g vs surface gravity " << gSurface / 9.81 << " g (need x" << jd::LandingThrustToWeightMin << ")" << std::endl;
            return;
        }
        // belly down when the ventral RCS the Newtonian law really commands holds the weight
        const bool assistedPlant = game::ship::plant::nativeFlightLaw(ship->core().desc().physicalControlModel) ==
                                   game::navigation::LocalFlightControlLaw::Assisted;
        const double ventral = assistedPlant
            ? game::ship::assistedLateralStabilizationAccelerationLimitMps2(ship->core().effectivePhysics())
            : game::navigation::landing::surfaceRcsMps2(ship->core().effectivePhysics(), 1, 1.0);
        f.bellyLanding = ventral >= jd::LandingThrustToWeightMin * gSurface;
        std::cerr << "[Autopilot] ship " << shipId.value << ": landing check " << target << " - main engines " << main / 9.81
                  << " g, " << (assistedPlant ? "Assisted vectoring lift " : "ventral RCS ") << ventral / 9.81 << " g vs surface " << gSurface / 9.81 << " g -> "
                  << (f.bellyLanding ? "BELLY-DOWN landing" : "tail-down landing (the ventral thrusters cannot hold the weight)")
                  << ", J floor " << jd::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters) / 1000.0
                  << " km" << std::endl;
    }
    f.autopilotTargetId = target;
    f.autopilotDepartureId = f.primaryBodyId;
    if (f.autopilotDepartureId.empty())
    {
        // engaged from a hub (normal flight began this tick): the body of the sphere of influence now
        bool root = false;
        const glm::dvec3 here = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        if (const auto* b = referenceBody(here, m_gravityBodies, root); b && !root)
            f.autopilotDepartureId = b->id;
    }
    // landing: "pad:<lat>,<lon>" (degrees, body-fixed) or a wild-terrain site search
    f.landingRequest = game::navigation::landing::LandingRequest {};
    f.landingStarted = false;
    double lat = 0.0, lon = 0.0;
    if (toSettlement)
    {
        double radius = 0.0;
        for (const auto& b : m_gravityBodies)
            if (b.id == target)
                radius = b.radiusMeters;
        for (const auto& layout : world::celestial::settlements::surfaceSettlementsOn(target, radius))
            if (layout.siteId == landingSpec.substr(5))
                if (const auto* pad = layout.pad(1))
                {
                    f.landingRequest.mode = game::navigation::landing::LandingMode::Pad;
                    f.landingRequest.padBodyDirection = pad->bodyDirection;
                    std::cerr << "[Autopilot] ship " << shipId.value << ": landing at the settlement " << layout.name << " ("
                              << layout.siteId << "), pad 1" << std::endl;
                }
    }
    else if (landingSpec.rfind("pad:", 0) == 0 && std::sscanf(landingSpec.c_str() + 4, "%lf,%lf", &lat, &lon) == 2)
    {
        const double la = glm::radians(lat), lo = glm::radians(lon);
        f.landingRequest.mode = game::navigation::landing::LandingMode::Pad;
        // body basis (prime, north, east)
        f.landingRequest.padBodyDirection = glm::dvec3(std::cos(la) * std::cos(lo), std::sin(la), std::cos(la) * std::sin(lo));
    }
    f.autopilotPhase = 2;
    f.autopilotSeconds = 0.0;
    f.autopilotRampSeconds = 0.0;
    f.orbitNormal = glm::dvec3(0.0);
    f.orbitAcquire = false;
    f.autopilotHaveLastCenter = false;
    f.autopilotLastLogSeconds = -1.0e30;
    motion.freeFlightThrust = 0;
    motion.jEngine = false;          // the autopilot flies its own J drive
    f.autoStabilizeArmed = false;
    motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    motion.freeFlightAutopilotTargetId = target;
    motion.freeFlightAutopilotPhase = 2;
    if (motion.freeFlightSurfaceState != 0)
    {
        motion.freeFlightSurfaceState = 0;   // lift off: the autopilot climbs away
    }
    std::cerr << "[Autopilot] ship " << shipId.value << ": ENGAGED -> " << target << " (departure " << f.autopilotDepartureId
              << "), landing "
              << (f.landingRequest.mode == game::navigation::landing::LandingMode::Pad ? "on a pad " + landingSpec
                                                                                       : std::string("wild: flat-site search"))
              << std::endl;
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
    if (f.autopilotPhase == 11)
    {
        updateNormalDescent(shipId, ship, f, *target, targetVelocity, dt);
        return;
    }
    f.autopilotSeconds += dt;

    static const double maxAccel = [] {
        const char* v = std::getenv("ELITE_J_AUTOPILOT_MAX_ACCEL");
        const double a = v && *v ? std::atof(v) : 0.0;
        return a > 0.0 ? a : game::simulation::jdrive::MaxAccelMps2;
    }();
    namespace jd = game::simulation::jdrive;
    // J ends above the target's J floor; the descent below it is on the main engines
    const double AimAltitudeM = jd::floorAltitudeM(target->radiusMeters, target->atmosphereRadiusMeters);
    constexpr double ApproachTauS = 6.0;
    const double mainAccel = mainEngineAccelMps2(ship);

    const glm::dvec3 rel = frame.originMeters - target->centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    const glm::dvec3 w = frame.linearVelocityMps - targetVelocity;
    const double ground = motion.freeFlightOrbitBodyId == target->id ? motion.freeFlightGroundHeightM : 0.0;
    const double height = rLen - target->radiusMeters - ground;
    // the drive: 1 g doubling every 2 s up to the cap
    const double ramp = std::min(maxAccel, 9.81 * std::exp2(f.autopilotRampSeconds / 2.0));

    glm::dvec3 wanted(0.0);
    double limit = ramp;
    int phase = f.autopilotPhase;
    int shownPhase = phase;
    double shownRange = height;
    glm::dvec3 orbitTangent(0.0);
    double orbitSpeed = 0.0;
    if (phase == 9)
    {
        // parking orbit: J off, no thrust - the orbit holds by itself; then the de-orbit
        f.orbitCoastSeconds += dt;
        const double vCirc = std::sqrt(target->gravitationalParameterM3s2 / rLen);
        if (f.autopilotSeconds - f.autopilotLastLogSeconds >= 2.0)
        {
            f.autopilotLastLogSeconds = f.autopilotSeconds;
            std::cerr << "[Autopilot] ship " << shipId.value << " ORBIT " << target->id << ": height " << height / 1000.0
                      << " km, speed " << glm::length(w) << " m/s (circular " << vCirc << "), vertical " << glm::dot(w, up)
                      << " m/s, coast " << f.orbitCoastSeconds << " s" << std::endl;
        }
        if (f.orbitCoastSeconds >= 15.0 && f.landing.site().valid)
        {
            // the orbit held (stabilization), now the de-orbit: NORMAL flight on the main engines
            // (no envelope - the orbital speed is braked by the pilot's ordinary controls)
            std::cerr << "[Autopilot] ship " << shipId.value << ": orbit held " << f.orbitCoastSeconds << " s at "
                      << height / 1000.0 << " km - DE-ORBIT in NORMAL flight (main engines)" << std::endl;
            f.autopilotPhase = 11;
            f.descentNose = glm::dvec3(0.0);
            f.lawBeforeLanding = motion.localControlLaw;
            updateNormalDescent(shipId, ship, f, *target, targetVelocity, dt);
            return;
        }
        motion.freeFlightAutopilotPhase = 12;
        motion.freeFlightAutopilotTargetId = target->id;
        motion.freeFlightAutopilotRangeM = height;
        motion.freeFlightAutopilotRelSpeedMps = glm::length(w);
        motion.freeFlightAutopilotAccelMps2 = 0.0f;
        if (motion.freeFlightCaptureRefusal == 4)
            motion.freeFlightCaptureRefusal = 0;
        return;
    }
    {
        // choose the landing site AHEAD (from 200 000 km: the terrain loads in the
        // background) and aim the approach above it - the lander lands where it chose
        glm::dvec3 aimDirection = up;
        {
            namespace landing = game::navigation::landing;
            glm::dmat3 worldFromBody(1.0);
            if (m_bodyOrientation)
                (void)m_bodyOrientation(target->id, worldFromBody);
            if (rLen < 2.0e8)
            {
                f.landingPlanSeconds -= dt;
                if (!f.landing.site().valid && f.landingPlanSeconds <= 0.0)
                {
                    f.landingPlanSeconds = 2.0;
                    const auto probe = [&](const glm::dvec3& bodyDirection, landing::TerrainPoint& out) {
                        TerrainSample s;
                        if (!m_terrainHeight || !m_terrainHeight(target->id, worldFromBody * bodyDirection, s))
                            return false;
                        out.heightM = s.heightM;
                        out.slopeDeg = s.slopeDeg;
                        out.material = s.material;
                        return true;
                    };
                    glm::dvec3 hull = ship.core().effectivePhysics().physicalActuatorHalfExtentsMeters;
                    if (hull.x <= 0.0 || hull.y <= 0.0 || hull.z <= 0.0)
                        hull = glm::dvec3(8.0, 3.0, 15.0);
                    if (!f.landingStarted)
                    {
                        f.landing.start(f.landingRequest, glm::transpose(worldFromBody) * up);
                        f.landingStarted = true;
                    }
                    if (f.landing.planAhead(glm::transpose(worldFromBody) * up, target->radiusMeters, hull, probe))
                    {
                        const auto& site = f.landing.site();
                        std::cerr << "[Autopilot] ship " << shipId.value << ": landing site chosen AHEAD at "
                                  << height / 1000.0 << " km ("
                                  << (f.landingRequest.mode == landing::LandingMode::Pad ? "pad" : "wild search") << "): "
                                  << site.offsetM << " m from the approach point, slope " << site.slopeDeg << " deg, roughness "
                                  << site.roughnessM << " m, ground " << site.heightM << " m" << std::endl;
                    }
                }
            }
            if (f.landing.site().valid)
                aimDirection = glm::normalize(worldFromBody * f.landing.site().bodyDirection);
        }
        // the parking orbit: circular at the J floor, in the plane of the ship's approach and the
        // site; the insertion point lies the main-engine braking distance before the site, the
        // velocity there is the circular one toward the site
        {
            const double rOrb = target->radiusMeters + AimAltitudeM;
            orbitSpeed = std::sqrt(target->gravitationalParameterM3s2 / rOrb);
            glm::dvec3 n = glm::length(f.orbitNormal) > 0.5 ? f.orbitNormal : glm::cross(up, aimDirection);
            if (glm::length(n) < 1.0e-6)
                n = glm::cross(up, std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
            n = glm::normalize(n);
            if (f.landing.site().valid && glm::length(f.orbitNormal) < 0.5 && rLen < rOrb * 4.0)
                f.orbitNormal = n;   // the plane is fixed once the site is known and the body near
            const double gSurface = target->gravitationalParameterM3s2 / (target->radiusMeters * target->radiusMeters);
            const double brake = std::min(10.0, 0.5 * std::max(0.3, mainAccel - gSurface));
            const double theta = std::min(1.5, orbitSpeed * orbitSpeed / (2.0 * brake) / target->radiusMeters + 0.03);
            const glm::dvec3 insertion = glm::normalize(aimDirection * std::cos(theta) - glm::cross(n, aimDirection) * std::sin(theta));
            orbitTangent = glm::normalize(glm::cross(n, insertion));
            aimDirection = insertion;
            // close to the orbit: the nearest point of the orbit circle (the ship slides onto the
            // orbit instead of chasing a fixed point it overshoots)
            const glm::dvec3 relNow = frame.originMeters - target->centerMeters;
            if (glm::length(target->centerMeters + insertion * rOrb - frame.originMeters) < 50000.0)
                f.orbitAcquire = true;
            if (f.orbitAcquire)
            {
                glm::dvec3 inPlane = relNow - n * glm::dot(relNow, n);
                if (glm::length(inPlane) > 1.0)
                {
                    aimDirection = glm::normalize(inPlane);
                    orbitTangent = glm::normalize(glm::cross(n, aimDirection));
                }
            }
        }
        const glm::dvec3 aim = target->centerMeters + aimDirection * (target->radiusMeters + AimAltitudeM);
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
        // never through the target either (an aim point on its far side): go round
        if (phase == 2 && d > 1.0)
        {
            const glm::dvec3 s = frame.originMeters - target->centerMeters;
            const double t = std::clamp(-glm::dot(s, toAim) / (d * d), 0.0, 1.0);
            if (t < 0.98 && glm::length(s + toAim * t) < target->radiusMeters + AimAltitudeM * 0.5)
            {
                glm::dvec3 side = s - dir * glm::dot(s, dir);
                if (glm::length(side) < 1.0)
                    side = glm::cross(dir, std::abs(dir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
                dir = glm::normalize(glm::normalize(side) + dir * 0.2);
            }
        }
        const double braking = std::sqrt(2.0 * 0.4 * maxAccel * d);
        const double tail = d / ApproachTauS;
        if (phase == 2 && tail < braking)
            phase = 3;
        // tangential: the closing velocity on top of the orbit velocity at the insertion point
        wanted = dir * std::min(braking, tail) + orbitTangent * orbitSpeed;
        if ((phase == 3 || f.orbitAcquire) && d < 1000.0 && glm::length(w - orbitTangent * orbitSpeed) < 5.0)
        {
            phase = 9;
            f.orbitCoastSeconds = 0.0;
            std::cerr << "[Autopilot] ship " << shipId.value << ": ON ORBIT of " << target->id << " at " << height / 1000.0
                      << " km, " << orbitSpeed << " m/s (J off, coast) after " << f.autopilotSeconds << " s" << std::endl;
        }
    }
    f.autopilotPhase = phase;

    // the J speed cap (0.9 c against the target)
    if (glm::length(wanted) > game::simulation::jdrive::SpeedCapMps)
        wanted *= game::simulation::jdrive::SpeedCapMps / glm::length(wanted);
    // deadbeat velocity servo within the drive's authority (gravity was integrated this tick)
    glm::dvec3 dv = wanted - w;
    // no J field near objects: a gentle push out of the nearest keep-out (not in the
    // powered descent, which flies on the main engines)
    if (JKeepOutHit hit; jKeepOut(shipId, motion.systemId, frame.originMeters, hit))
    {
        const glm::dvec3 off = frame.originMeters - hit.positionMeters;
        const glm::dvec3 away = glm::length(off) > 1.0 ? glm::normalize(off) : glm::dvec3(tr.forward());
        dv = hit.velocityMps + away * jd::GentleSpeedMps - frame.linearVelocityMps;
        limit = std::min(limit, jd::GentleAccelMps2);
        if (motion.freeFlightCaptureRefusal != 3 || f.autopilotSeconds - f.autopilotLastLogSeconds >= 1.9)
            std::cerr << "[Autopilot] ship " << shipId.value << ": J keep-out of " << hit.objectId << " at " << hit.distanceMeters / 1000.0
                      << " km (" << glm::length(hit.velocityMps - frame.linearVelocityMps) << " m/s relative)" << std::endl;
        motion.freeFlightCaptureRefusal = 3;
        f.autopilotRampSeconds = 0.0;   // the full drive ramps from 1 g again outside the keep-out
    }
    else
    {
        if (motion.freeFlightCaptureRefusal == 3)
            motion.freeFlightCaptureRefusal = 0;
        f.autopilotRampSeconds += dt;
    }
    // below the J floor of the reference body: no J field - the ship's own main engines
    // (thrust along the hull, the hull turns toward the thrust it needs). Leaving a body: a
    // straight climb against it to its floor first. (A landing never gets here: it leaves J
    // on the parking orbit and flies in normal flight.)
    bool mainEngine = false;
    const GravityBody* floorBody = find(f.primaryBodyId);
    if (floorBody && f.primaryAltitudeMeters < jd::floorAltitudeM(floorBody->radiusMeters, floorBody->atmosphereRadiusMeters))
    {
        mainEngine = true;
        if (floorBody != target && dt > 0.0)
        {
            const glm::dvec3 refVel = f.lastPrimaryId == floorBody->id ? (floorBody->centerMeters - f.lastPrimaryCenterMeters) / dt : glm::dvec3(0.0);
            const glm::dvec3 upRef = glm::normalize(frame.originMeters - floorBody->centerMeters);
            dv = refVel + upRef * jd::FloorClimbSpeedMps - frame.linearVelocityMps;
        }
    }
    glm::dvec3 thrustWanted(0.0);
    if (mainEngine)
    {
        // a thrust-axis lander: the acceleration it wants = velocity error / tau + gravity
        // compensation (gravity is integrated with the frame); the nose turns toward it and the
        // engine gives its projection on the nose, within the real main-engine thrust
        constexpr double TauS = 1.0;
        limit = std::min(limit, mainAccel);
        f.autopilotRampSeconds = 0.0;
        if (motion.freeFlightCaptureRefusal != 3)
            motion.freeFlightCaptureRefusal = 4;
        const glm::dvec3 gravity = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies).accelerationMps2;
        thrustWanted = dv / TauS - gravity;
        const glm::dvec3 noseNow = glm::normalize(glm::dvec3(tr.forward()));
        const double reverse = ship.core().effectivePhysics().reverseMainEngineAvailable ? -limit : 0.0;
        dv = noseNow * (std::clamp(glm::dot(thrustWanted, noseNow), reverse, limit) * dt);
    }
    else if (motion.freeFlightCaptureRefusal == 4)
        motion.freeFlightCaptureRefusal = 0;
    const double maxDv = limit * dt;
    if (glm::length(dv) > maxDv)
        dv *= maxDv / glm::length(dv);
    frame.linearVelocityMps += dv;
    const glm::dvec3 accel = dv / dt;

    // the hull: in the transfer the nose points the drive (flip to brake); in the
    // descent the ship hangs level - belly to the ground, nose to the horizon -
    // so the pilot sees the surface coming (the drive pushes through the belly)
    glm::dvec3 noseWanted(0.0), upWanted = glm::dvec3(tr.up());
    if (mainEngine)
    {
        // main engines: the nose points the thrust it needs (tail down while hovering)
        const glm::dvec3 upLocal = glm::normalize(frame.originMeters - (floorBody ? floorBody->centerMeters : target->centerMeters));
        noseWanted = glm::length(thrustWanted) > 0.05 ? glm::normalize(thrustWanted) : upLocal;
    }
    else
    {
        // the J drive pushes both ways: the nose follows the flight - along the velocity
        // against the target (prograde), the target ahead when nearly at rest
        const glm::dvec3 toTarget = target->centerMeters + up * target->radiusMeters - frame.originMeters;
        const glm::dvec3 wNow = w + dv;
        if (glm::length(wNow) > 20.0)
            noseWanted = glm::normalize(wNow);
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
        // the attitude rate: the J field turns the hull freely (~115 deg/s); on the main
        // engines the hull's own rate limits it
        const double rate = mainEngine ? std::clamp(static_cast<double>(ship.core().effectivePhysics().maxPitchRate), 0.2, 2.0) : 2.0;
        const double step = rate * dt;
        const glm::dquat next = angle > step ? glm::slerp(current, wantedQ, step / angle) : wantedQ;
        tr.orientation = glm::mat4(glm::mat3(glm::mat3_cast(next)));
        tr.pitchRate = tr.yawRate = tr.rollRate = 0.0f;
    }

    motion.freeFlightAutopilotPhase = static_cast<std::uint8_t>(shownPhase);
    motion.freeFlightAutopilotTargetId = target->id;
    motion.freeFlightAutopilotRangeM = shownRange;
    motion.freeFlightAutopilotRelSpeedMps = glm::length(w + dv);
    // signed along the velocity: braking is a negative acceleration (the speed still
    // grows while it is positive, however small)
    const glm::dvec3 wAfter = w + dv;
    motion.freeFlightAutopilotAccelMps2 = static_cast<float>(
        glm::length(wAfter) > 1.0e-3 ? glm::dot(accel, glm::normalize(wAfter)) : glm::length(accel));
    if (f.autopilotSeconds - f.autopilotLastLogSeconds >= 2.0)
    {
        f.autopilotLastLogSeconds = f.autopilotSeconds;
        std::cerr << "[Autopilot] ship " << shipId.value << " t+" << f.autopilotSeconds << " s phase " << phase << " -> "
                  << target->id << ": height " << height / 1000.0 << " km, speed " << glm::length(w + dv) / 1000.0
                  << " km/s, accel " << glm::length(accel) << " m/s2 (drive " << ramp << ")" << std::endl;
    }
}

void GameSimulation::updateNormalDescent(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const game::navigation::GravityBody& body,
                                         const glm::dvec3& bodyVelocityMps, double dt)
{
    namespace landing = game::navigation::landing;
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    if (dt <= 0.0 || !f.landing.site().valid)
        return;
    f.autopilotSeconds += dt;
    const ShipParams params = game::ship::telemetry::applyAuthorityToControlParams(ship.core().effectivePhysics(),
                                                                                   ship.core().controlAuthorityTelemetry());
    const glm::dvec3 rel = frame.originMeters - body.centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    glm::dmat3 worldFromBody(1.0);
    const bool basis = m_bodyOrientation && m_bodyOrientation(body.id, worldFromBody);
    const auto& site = f.landing.site();
    const glm::dvec3 siteDir = glm::normalize(worldFromBody * site.bodyDirection);
    // over the ground: the body's velocity and its turn under the ship
    glm::dvec3 groundVel = bodyVelocityMps;
    if (basis && f.haveLastBasis && f.lastBasisBodyId == body.id)
        groundVel += (worldFromBody * glm::transpose(f.lastBasis) * rel - rel) / dt;
    const glm::dvec3 v = frame.linearVelocityMps - groundVel;
    const double vz = glm::dot(v, up);
    const glm::dvec3 vhVec = v - up * vz;
    const double vh = glm::length(vhVec);
    // the ground: the terrain under the ship when it is sampled (low), else the site's
    const bool terrainKnown = motion.freeFlightOrbitBodyId == body.id && motion.freeFlightGroundMaterial >= 0;
    const double groundM = terrainKnown ? motion.freeFlightGroundHeightM : site.heightM + halfHeightAlong(ship, up);
    const double h = rLen - body.radiusMeters - groundM;
    // to the site along the surface
    const double arc = std::acos(std::clamp(glm::dot(up, siteDir), -1.0, 1.0)) * body.radiusMeters;
    glm::dvec3 toSite = siteDir - up * glm::dot(siteDir, up);
    toSite = glm::length(toSite) > 1.0e-12 ? glm::normalize(toSite) : (vh > 1.0 ? vhVec / vh : glm::dvec3(tr.forward()));
    const double mainAccel = std::max(1.0, landing::surfaceMainEngineAccelMps2(params));
    const double g = body.gravitationalParameterM3s2 / (rLen * rLen);

    // the wanted velocity over the ground: toward the site, braking at 35 % of the main engine
    // (the rest holds the weight and the corrections), an approach tail arc / 15 s; down along a
    // glide that reaches the hand-over height as the arc closes
    constexpr double HandoverHeightM = 1500.0;
    constexpr double HandoverArcM = 1500.0;
    // never faster than the circular speed here: the descent only brakes the orbit
    const double vhWanted = std::min({std::sqrt(2.0 * 0.35 * mainAccel * arc), arc / 15.0,
                                      std::sqrt(body.gravitationalParameterM3s2 / rLen)});
    const double tgo = std::max(10.0, (arc - 0.5 * HandoverArcM) / std::max(vh, 30.0));
    double vzWanted = std::clamp(-(h - HandoverHeightM) / tgo, -250.0, 60.0);
    if (h < 800.0)
        vzWanted = std::max(vzWanted, 5.0);   // never toward the ground this low in normal flight
    const glm::dvec3 vWanted = toSite * vhWanted + up * vzWanted;
    const glm::dvec3 err = vWanted - v;
    const double errZ = glm::dot(err, up);
    // acceleration: horizontal tau 4 s, vertical 3 s, plus the weight less the orbital lift (v_h^2 / r)
    const glm::dvec3 aH = (err - up * errZ) / 4.0;
    const double aZ = errZ / 3.0 + g - vh * vh / rLen;
    glm::dvec3 a = aH + up * aZ;
    const double cap = 0.95 * mainAccel;
    if (glm::length(a) > cap)
    {
        // the vertical first (it keeps the ship off the ground), the horizontal gets the rest
        const double z = std::clamp(aZ, -cap, cap);
        const double room = std::sqrt(std::max(0.0, cap * cap - z * z));
        a = up * z + (glm::length(aH) > room ? aH * (room / glm::length(aH)) : aH);
    }
    if (glm::length(a) > 0.5)
        f.descentNose = glm::normalize(a);
    const glm::dvec3 noseHint = glm::length(f.descentNose) > 0.5 ? f.descentNose : (vh > 1.0 ? -vhVec / vh : glm::dvec3(tr.forward()));

    landing::SurfacePilotRequest rq;
    rq.actualVelocityMps = v;
    rq.desiredVelocityMps = vWanted;
    rq.gravityMps2 = -up * g;
    rq.upLocal = up;
    rq.headingHint = toSite;
    rq.forward = glm::dvec3(tr.forward());
    rq.right = glm::dvec3(tr.right());
    rq.up = glm::dvec3(tr.up());
    rq.pitchRateRadPerSec = tr.pitchRate;
    rq.yawRateRadPerSec = tr.yawRate;
    rq.rollRateRadPerSec = tr.rollRate;
    rq.bellyDown = true;
    rq.dt = dt;
    landing::SurfacePilotResult pilot = landing::flyNormalFlightThrust(rq, params, a, noseHint);
    const auto native = game::ship::plant::nativeFlightLaw(ship.core().desc().physicalControlModel);
    if (motion.localControlLaw != native)
    {
        pilot.control.localControlLawCommandValid = true;
        pilot.control.requestedLocalControlLaw = native;
    }
    if (!f.pilotActive)
        std::cerr << "[Autopilot] ship " << shipId.value << ": de-orbit pilot ON (normal flight, main engines along the nose, law "
                  << (native == game::navigation::LocalFlightControlLaw::Assisted ? "Assisted" : "Newtonian") << ")" << std::endl;
    f.pilotActive = true;
    f.pilotControl = pilot.control;

    motion.freeFlightAutopilotPhase = 11;
    motion.freeFlightAutopilotTargetId = body.id;
    motion.freeFlightAutopilotRangeM = arc;
    motion.freeFlightAutopilotRelSpeedMps = glm::length(v);
    motion.freeFlightAutopilotAccelMps2 = static_cast<float>(glm::length(motion.mainEngineAccelerationMps2));
    if (f.autopilotSeconds - f.pilotLastLogSeconds >= 2.0)
    {
        f.pilotLastLogSeconds = f.autopilotSeconds;
        const double noseOff = glm::degrees(std::acos(std::clamp(glm::dot(pilot.noseWanted, glm::normalize(rq.forward)), -1.0, 1.0)));
        std::cerr << "[Autopilot] ship " << shipId.value << " DE-ORBIT t+" << f.autopilotSeconds << " s: height " << h
                  << " m (ground " << (terrainKnown ? "sampled" : "site") << "), to the site " << arc / 1000.0 << " km, horizontal "
                  << vh << " m/s (wanted " << vhWanted << "), vertical " << vz << " m/s (wanted " << vzWanted << "), accel wanted "
                  << glm::length(a) << " m/s2, main " << pilot.mainCommand * 100.0 << " % -> " << glm::length(motion.mainEngineAccelerationMps2)
                  << " m/s2, nose off " << noseOff << " deg" << std::endl;
    }

    // hand-over: low and slow near the site -> stabilization against the ground (the 500 m/s
    // envelope over the surface), where the landing module flies the touchdown
    if ((arc < 2.0 * HandoverArcM && glm::length(v) < 80.0 && h < 1.7 * HandoverHeightM) ||
        (h < 600.0 && glm::length(v) < 450.0))
    {
        std::cerr << "[Autopilot] ship " << shipId.value << ": HAND-OVER to the ground stabilization at height " << h << " m, "
                  << arc << " m from the site, " << glm::length(v) << " m/s over the ground after " << f.autopilotSeconds << " s"
                  << std::endl;
        enterBodyFrame(shipId, ship, f, body, bodyVelocityMps);
        f.autopilotPhase = 10;
        updateSurfaceLanding(shipId, ship, f, dt);
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
    f.bodyFrame = true;
    f.bodyFrameId = body.id;
    f.bodyPreviousCentre = body.centerMeters;
    f.bodyHavePrevious = true;
    f.bodyPreviousBasisValid = m_bodyOrientation && m_bodyOrientation(body.id, f.bodyPreviousBasis);
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
    motion.freeFlightCaptureRefusal = 0;
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

void GameSimulation::updateSurfaceLanding(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt)
{
    namespace landing = game::navigation::landing;
    auto& tr = ship.core().transform();
    auto& motion = tr.motion;
    auto& frame = motion.travelFrame;
    const game::navigation::GravityBody* target = nullptr;
    for (const auto& b : m_gravityBodies)
        if (b.id == f.autopilotTargetId)
            target = &b;
    if (!target || dt <= 0.0)
        return;
    f.autopilotSeconds += dt;
    // the shared physics left the frame on the ship (local zero): world state
    const glm::dvec3 shipPos = frame.originMeters;
    const glm::dvec3 shipVel = frame.linearVelocityMps + motion.localVelocityMps;
    const glm::dvec3 bodyVel = motion.freeFlightReferenceVelocityMps;
    const glm::dvec3 rel = shipPos - target->centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    const double ground = motion.freeFlightOrbitBodyId == target->id ? motion.freeFlightGroundHeightM : 0.0;

    glm::dmat3 worldFromBody(1.0);
    if (m_bodyOrientation)
        (void)m_bodyOrientation(target->id, worldFromBody);
    const auto probe = [&](const glm::dvec3& bodyDirection, landing::TerrainPoint& out) {
        TerrainSample s;
        if (!m_terrainHeight || !m_terrainHeight(target->id, worldFromBody * bodyDirection, s))
            return false;
        out.heightM = s.heightM;
        out.slopeDeg = s.slopeDeg;
        out.material = s.material;
        return true;
    };
    glm::dvec3 hull = ship.core().effectivePhysics().physicalActuatorHalfExtentsMeters;
    if (hull.x <= 0.0 || hull.y <= 0.0 || hull.z <= 0.0)
        hull = glm::dvec3(8.0, 3.0, 15.0);
    if (!f.landingStarted)
    {
        f.landing.start(f.landingRequest, glm::transpose(worldFromBody) * up);
        f.landingStarted = true;
    }
    const ShipParams params = game::ship::telemetry::applyAuthorityToControlParams(ship.core().effectivePhysics(),
                                                                                   ship.core().controlAuthorityTelemetry());
    const bool hadSite = f.landing.site().valid;
    landing::LandingInput in;
    in.bodyCentreM = target->centerMeters;
    in.bodyVelocityMps = bodyVel;
    in.worldFromBody = worldFromBody;
    in.bodyRadiusM = target->radiusMeters;
    in.bodyMuM3s2 = target->gravitationalParameterM3s2;
    in.shipPositionM = shipPos;
    in.shipVelocityMps = shipVel;
    in.shipForward = glm::dvec3(tr.forward());
    in.hullHalfExtentsM = hull;
    in.groundUnderShipM = ground;
    in.dt = dt;
    in.authorityMps2 = landing::surfaceMainEngineAccelMps2(params);
    in.touchdownHalfHeightM = halfHeightAlong(ship, up);
    const landing::LandingCommand cmd = f.landing.step(in, probe);
    if (!hadSite && f.landing.site().valid)
    {
        const auto& site = f.landing.site();
        std::cerr << "[Autopilot] ship " << shipId.value << ": landing site ("
                  << (f.landingRequest.mode == landing::LandingMode::Pad ? "pad" : "wild search") << ") " << site.offsetM
                  << " m from the start, slope " << site.slopeDeg << " deg, roughness " << site.roughnessM << " m, ground "
                  << site.heightM << " m, material " << site.material << " - " << f.landing.status() << std::endl;
    }

    // the virtual pilot: ordinary Newtonian controls (over the body, its own pull)
    landing::SurfacePilotRequest rq;
    rq.actualVelocityMps = shipVel - bodyVel;
    rq.desiredVelocityMps = rq.actualVelocityMps + cmd.velocityErrorMps;
    rq.gravityMps2 = -up * (target->gravitationalParameterM3s2 / (rLen * rLen));
    rq.upLocal = up;
    rq.headingHint = cmd.noseWorld;
    rq.forward = glm::dvec3(tr.forward());
    rq.right = glm::dvec3(tr.right());
    rq.up = glm::dvec3(tr.up());
    rq.pitchRateRadPerSec = tr.pitchRate;
    rq.yawRateRadPerSec = tr.yawRate;
    rq.rollRateRadPerSec = tr.rollRate;
    rq.bellyDown = f.bellyLanding;
    rq.dt = dt;
    // the ship's native law (its physical control model): the plant executes only that law
    const auto native = game::ship::plant::nativeFlightLaw(ship.core().desc().physicalControlModel);
    const bool assisted = native == game::navigation::LocalFlightControlLaw::Assisted;
    // Assisted flies the terminal part in its Backspace mode (no speed servo: the keypad axes are
    // the vectoring main's accelerations) - the speed servo along the nose turned the hull's
    // levelling into a 170 m/s horizontal run (live, 2026-11-06)
    if (assisted && motion.localControlLaw == native && !motion.assistedFreeRotation)
    {
        motion.assistedFreeRotation = true;
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
        f.pilotFreeRotation = true;
    }
    landing::SurfacePilotResult pilot = assisted ? landing::flySurfaceLandingVectoring(rq, params)
                                                 : landing::flySurfaceLanding(rq, params);
    if (motion.localControlLaw != native)
    {
        pilot.control.localControlLawCommandValid = true;
        pilot.control.requestedLocalControlLaw = native;
    }
    if (!f.pilotActive)
    {
        f.lawBeforeLanding = motion.localControlLaw;
        std::cerr << "[Autopilot] ship " << shipId.value << ": landing pilot ON (normal flight, the ship's native law "
                  << (assisted ? "Assisted" : "Newtonian") << (motion.localControlLaw != native ? " - switching to it" : "")
                  << "; " << (assisted || f.bellyLanding ? "belly down" : "tail down") << ")" << std::endl;
    }
    f.pilotActive = true;
    f.pilotControl = pilot.control;

    const auto lp = f.landing.phase();
    motion.freeFlightAutopilotPhase = static_cast<std::uint8_t>(lp == landing::LandingPhase::Planning ? 7
                                                                : lp == landing::LandingPhase::Orbit  ? 8
                                                                : lp == landing::LandingPhase::Transit ? 6
                                                                                                       : 4);
    motion.freeFlightAutopilotTargetId = target->id;
    motion.freeFlightAutopilotRangeM = lp == landing::LandingPhase::Transit ? cmd.distanceToSiteM : cmd.heightAboveGroundM;
    motion.freeFlightAutopilotRelSpeedMps = glm::length(rq.actualVelocityMps);
    motion.freeFlightAutopilotAccelMps2 = static_cast<float>(glm::length(motion.engineAccelerationMps2));
    if (f.autopilotSeconds - f.pilotLastLogSeconds >= 2.0)
    {
        f.pilotLastLogSeconds = f.autopilotSeconds;
        const glm::dvec3 v = rq.actualVelocityMps;
        const double noseOff = glm::degrees(std::acos(std::clamp(glm::dot(pilot.noseWanted, rq.forward), -1.0, 1.0)));
        std::cerr << "[Autopilot] ship " << shipId.value << " NORMAL t+" << f.autopilotSeconds << " s " << f.landing.status()
                  << ": height " << cmd.heightAboveGroundM << " m, to the site " << cmd.distanceToSiteM / 1000.0
                  << " km, vertical " << glm::dot(v, up) << " m/s, horizontal "
                  << glm::length(v - up * glm::dot(v, up)) << " m/s, main " << pilot.mainCommand * 100.0 << " %, RCS ("
                  << pilot.control.strafeInput << ", " << pilot.control.liftInput << ", " << pilot.control.forwardInput
                  << "), nose off " << noseOff << " deg, law " << int(motion.localControlLaw) << ", engines "
                  << glm::length(motion.engineAccelerationMps2) << " m/s2" << std::endl;
    }
}

void GameSimulation::applySurfaceLandingPilots()
{
    // before the flight step: the landing pilot's ordinary controls replace the input
    for (auto& [id, f] : m_freeFlights)
    {
        Ship* ship = getShip(id);
        if (!ship)
            continue;
        if (f.pilotActive)
            ship->setControlState(f.pilotControl);
        else if (f.pilotRestoreLaw)
        {
            f.pilotRestoreLaw = false;
            if (f.pilotFreeRotation)
            {
                ship->core().transform().motion.assistedFreeRotation = false;
                f.pilotFreeRotation = false;
            }
            if (ship->core().transform().motion.localControlLaw == f.lawBeforeLanding)
                continue;
            ShipControlState restore {};
            restore.localControlLawCommandValid = true;
            restore.requestedLocalControlLaw = f.lawBeforeLanding;
            ship->setControlState(restore);
            std::cerr << "[Autopilot] ship " << id.value << ": landing pilot OFF, flight law restored ("
                      << int(f.lawBeforeLanding) << ")" << std::endl;
        }
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
            // below a body's J floor and within the manoeuvre envelope over it: stabilization
            // against the ground (the body frame); faster (an orbit) - the own ballistic frame
            const double overBody = glm::length(shipVel - motion.freeFlightReferenceVelocityMps);
            for (const auto& b : m_gravityBodies)
                if (b.id == active->second.primaryBodyId && b.radiusMeters > 0.0 && overBody <= safeSpeed &&
                    active->second.primaryAltitudeMeters <
                        game::simulation::jdrive::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters))
                {
                    auto& f = active->second;
                    f.autopilotTargetId.clear();
                    f.autopilotPhase = 0;
                    motion.freeFlightAutopilotPhase = 0;
                    enterBodyFrame(shipId, *ship, f, b, motion.freeFlightReferenceVelocityMps);
                    return true;
                }
            // No hub can take the ship: it keeps its global motion in its own
            // ballistic frame (non-rotating, falling freely under gravity) and the
            // pilot flies locally from rest inside it - global and local speed apart.
            auto& f = active->second;
            f.localFrame = true;
            f.autopilotTargetId.clear();
            f.autopilotPhase = 0;
            motion.jEngine = false;
            motion.velocityReferenceKind = 3;   // stabilization on the own trajectory (orbit)
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
    namespace jd = game::simulation::jdrive;
    JKeepOutHit hit;
    if (jKeepOut(shipId, systemId, shipWorld, hit))
    {
        why = hit.objectId + " at " + std::to_string(static_cast<int>(hit.distanceMeters / 1000.0)) + " km (J needs " +
              std::to_string(static_cast<int>((hit.ship ? jd::KeepOutShipM : jd::KeepOutObjectM) / 1000.0)) + " km)";
        return 7;
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
            return 8;
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
        if (!active->second.autopilotTargetId.empty() || active->second.legActive)
        {
            std::cerr << "[Flight] ship " << shipId.value << ": stabilization refused - the autopilot flies" << std::endl;
            return false;
        }
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
    if (active != m_freeFlights.end() && !active->second.autopilotTargetId.empty())
    {
        std::cerr << "[Flight] ship " << shipId.value << ": J engine refused - the autopilot flies its own J drive" << std::endl;
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
    motion.freeFlightCaptureRefusal = 0;
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
    // route legs that arrived last tick leave J now (a hub capture or the local
    // ballistic frame) - outside the pass below, which iterates m_freeFlights
    for (const EntityId id : std::exchange(m_pendingJExit, {}))
        if (m_freeFlights.count(id))
            (void)toggleFreeFlight(id);
    // automatic stabilization decided last pass (a coast on a safe closed orbit)
    for (const EntityId id : std::exchange(m_pendingStabilize, {}))
        if (const auto f = m_freeFlights.find(id); f != m_freeFlights.end() && !f->second.localFrame)
        {
            std::cerr << "[Flight] ship " << id.value << ": STABILIZATION ON (automatic: a closed orbit, coasting)" << std::endl;
            (void)toggleFreeFlight(id);
        }
    updateRoutes(dt);
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
        f.bodyLiftOff = false;
        if (f.bodyFrame)
        {
            // body frame: the frame moved with the ground (its velocity), the laws moved the
            // ship inside it; the world motion is both plus gravity - the frame is re-based on
            // the ship (local zero) for the shared physics below and split again at the end
            const glm::dvec3 local = motion.localPositionMeters;
            const glm::dvec3 localVel = motion.localVelocityMps;
            const glm::dvec3 upHere = glm::normalize(frame.originMeters - f.bodyPreviousCentre);
            f.bodyLiftOff = glm::dot(localVel, upHere) > 0.3;
            if (std::getenv("ELITE_DEV_BODY_TRACE") && std::fmod(f.elapsedSeconds, 2.0) < dt)
                std::cerr << "[BodyTrace] ship " << it->first.value << " dt " << dt << " local dv " << glm::length(localVel - f.bodyTraceLocalStart)
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
        if (!f.bodyFrame)
            frame.linearVelocityMps += 0.5 * (g0.accelerationMps2 + g1.accelerationMps2) * dt;
        frame.linearAccelerationMps2 = f.bodyFrame ? glm::dvec3(0.0) : g1.accelerationMps2;
        frame.valid = true;
        if (f.localFrame && !f.bodyFrame)
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
        motion.velocityReferenceKind = f.bodyFrame ? 4 : (referenceIsRoot ? 1 : 2);
        motion.mainEngineMaxMps2 = mainEngineAccelMps2(*ship);

        // ---- the J engine: the J field along the main-engine thrust, in place of it
        if (motion.jEngine && !f.bodyFrame)
        {
            namespace jd = game::simulation::jdrive;
            std::string why;
            if (const int blocked = jEngineBlocked(it->first, motion.systemId, frame.originMeters, why))
            {
                motion.jEngine = false;
                motion.jStopping = false;
                motion.jAccelMps2 = motion.jAccelCommandMps2 = 0.0;
                f.autoStabilizeArmed = true;
                motion.freeFlightCaptureRefusal = blocked == 7 ? 3 : 4;
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
                        cmd = (up ? 1.0 : -1.0) * std::min(jd::MaxAccelMps2, std::max(g, std::abs(cmd) * grow));
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
                if (motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted && f.autopilotTargetId.empty() &&
                    !f.legActive && !f.bodyFrame)
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
                    const bool coasting = !f.bodyFrame && !motion.jEngine && f.autopilotTargetId.empty() && !f.legActive &&
                                          !input && glm::length(motion.engineAccelerationMps2) < 0.05;
                    f.coastSeconds = coasting ? f.coastSeconds + dt : 0.0;
                    const bool safeOrbit = motion.freeFlightOrbitBound && !referenceIsRoot &&
                                           motion.freeFlightOrbitPeriapsisAltM > std::max(safeAlt, motion.freeFlightGroundHeightM + 1000.0);
                    if (f.autoStabilizeArmed && coasting && safeOrbit && f.coastSeconds >= 5.0)
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
                if (motion.freeFlightSurfaceState == 1 && (motion.freeFlightThrust == 1 || f.bodyLiftOff))
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
            // ---- route leg (hub / orbit / point)
            if (f.legActive && motion.freeFlightSurfaceState == 0 && !f.bodyFrame)
                updateFreeFlightLeg(it->first, *ship, f, dt);
            // ---- J autopilot
            if (!f.autopilotTargetId.empty())
            {
                if (motion.freeFlightSurfaceState == 0)
                {
                    if (f.bodyFrame)
                        updateSurfaceLanding(it->first, *ship, f, dt);
                    else
                        updateFreeFlightAutopilot(it->first, *ship, f, dt);
                }
                else
                {
                    if (f.pilotActive)
                    {
                        f.pilotActive = false;
                        f.pilotRestoreLaw = true;
                    }
                    std::cerr << "[Autopilot] ship " << it->first.value << ": "
                              << (motion.freeFlightSurfaceState == 1 ? "LANDED on " : "CRASHED on ") << f.autopilotTargetId << " after "
                              << f.autopilotSeconds << " s; ground material " << motion.freeFlightGroundMaterial << ", slope "
                              << motion.freeFlightGroundSlopeDeg << " deg";
                    if (f.landing.site().valid && f.landedBodyKnown && primary)
                        std::cerr << ", " << std::acos(std::clamp(glm::dot(glm::normalize(f.landedBodyDirection),
                                                                           glm::normalize(f.landing.site().bodyDirection)), -1.0, 1.0)) *
                                                 primary->radiusMeters
                                  << " m from the chosen site";
                    std::cerr << std::endl;
                    f.autopilotTargetId.clear();
                    f.autopilotPhase = 0;
                    motion.freeFlightAutopilotPhase = 5;
                }
            }
            // the J speed cap: the field does not carry the ship above 0.9 c against the reference body
            if (primary && f.lastPrimaryId == primary->id && dt > 0.0 && !f.bodyFrame)
            {
                const glm::dvec3 refVel = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 rel = frame.linearVelocityMps - refVel;
                if (glm::length(rel) > game::simulation::jdrive::SpeedCapMps)
                    frame.linearVelocityMps = refVel + rel * (game::simulation::jdrive::SpeedCapMps / glm::length(rel));
            }
            f.lastPrimaryId = primary ? primary->id : std::string();
            f.lastPrimaryCenterMeters = primary ? primary->centerMeters : glm::dvec3(0.0);
        }

        if (f.bodyFrame)
        {
            // split again: the frame moves with the ground under the ship (the body's velocity
            // and its turn), the local velocity is the velocity over the ground
            const game::navigation::GravityBody* body = nullptr;
            for (const auto& b : m_gravityBodies)
                if (b.id == f.bodyFrameId)
                    body = &b;
            if (body)
            {
                const glm::dvec3 shipVel = frame.linearVelocityMps + motion.localVelocityMps;
                glm::dvec3 ground = f.bodyHavePrevious && dt > 0.0 ? (body->centerMeters - f.bodyPreviousCentre) / dt
                                                                  : motion.freeFlightReferenceVelocityMps;
                glm::dmat3 basis(1.0);
                const bool basisValid = m_bodyOrientation && m_bodyOrientation(body->id, basis);
                const glm::dvec3 r = frame.originMeters - body->centerMeters;
                if (basisValid && f.bodyPreviousBasisValid && dt > 0.0)
                    ground += (basis * glm::transpose(f.bodyPreviousBasis) * r - r) / dt;
                f.bodyPreviousBasis = basis;
                f.bodyPreviousBasisValid = basisValid;
                f.bodyPreviousCentre = body->centerMeters;
                f.bodyHavePrevious = true;
                frame.linearVelocityMps = ground;
                motion.localVelocityMps = motion.freeFlightSurfaceState != 0 ? glm::dvec3(0.0) : shipVel - ground;
                f.bodyTraceLocalStart = motion.localVelocityMps;
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
