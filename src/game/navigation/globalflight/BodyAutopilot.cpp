// The body autopilot (see BodyAutopilot.h). Extracted from the server's J autopilot
// (GameSimulation::updateFreeFlightAutopilot / updateNormalDescent / updateSurfaceLanding,
// 2026-11-06) behaviour-preserving: the same laws, the same logs; the velocity changes the server
// applied directly are now a command its plant executes (a J velocity target with its limit, a
// main-engine thrust along the nose, an attitude target).
#include "src/game/navigation/globalflight/BodyAutopilot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/landing/SurfaceLandingPilot.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"

namespace game::navigation::globalflight
{
namespace jd = game::navigation::jdrive;

double mainEngineAccelMps2(const ShipParams& p)
{
    if (p.forwardMainEngineAccelerationMps2 > 0.0f)
        return p.forwardMainEngineAccelerationMps2;
    const double gs = p.maxLinearGs > 0.0f ? p.maxLinearGs : p.maxGs;
    return std::max(1.0, gs * jd::StandardGravityMps2);
}

double halfHeightAlong(const ShipParams& p, const glm::dvec3& right, const glm::dvec3& up, const glm::dvec3& forward,
                       const glm::dvec3& vertical)
{
    glm::dvec3 half = p.physicalActuatorHalfExtentsMeters;
    if (half.x <= 0.0 || half.y <= 0.0 || half.z <= 0.0)
        half = glm::dvec3(8.0, 3.0, 15.0);
    return std::abs(glm::dot(right, vertical)) * half.x + std::abs(glm::dot(up, vertical)) * half.y +
           std::abs(glm::dot(forward, vertical)) * half.z;
}

namespace
{
glm::dvec3 hullOf(const ShipParams& p)
{
    glm::dvec3 hull = p.physicalActuatorHalfExtentsMeters;
    if (hull.x <= 0.0 || hull.y <= 0.0 || hull.z <= 0.0)
        hull = glm::dvec3(8.0, 3.0, 15.0);
    return hull;
}

landing::TerrainProbe probeFor(const BodyAutopilotEnvironment& env, const std::string& bodyId, const glm::dmat3& worldFromBody)
{
    return [&env, bodyId, worldFromBody](const glm::dvec3& bodyDirection, landing::TerrainPoint& out) {
        return env.terrain && env.terrain(bodyId, worldFromBody * bodyDirection, out);
    };
}

// an attitude target: the nose and an up kept as close as possible to the current one
void attitude(ShipControlState& c, const glm::dvec3& nose, glm::dvec3 up)
{
    up = up - nose * glm::dot(up, nose);
    if (glm::length(up) < 1.0e-3)
        up = glm::cross(nose, std::abs(nose.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
    c.globalAttitudeTargetValid = true;
    c.globalAttitudeForward = nose;
    c.globalAttitudeUp = glm::normalize(up);
}
} // namespace

const GravityBody* BodyAutopilot::find(const BodyAutopilotEnvironment& env, const std::string& id) const
{
    if (!env.bodies)
        return nullptr;
    for (const auto& b : *env.bodies)
        if (b.id == id)
            return &b;
    return nullptr;
}

void BodyAutopilot::cancel()
{
    if (!m_targetId.empty())
        std::cerr << "[Autopilot] cancelled (" << m_targetId << ")" << std::endl;
    m_targetId.clear();
    m_stage = AutopilotStage::None;
    m_pilotActive = false;
}

bool BodyAutopilot::engage(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const std::string& bodyId,
                           const std::string& departureBodyId, const std::string& landingSpec, std::uint8_t& refusal)
{
    const std::uint32_t shipId = ship.shipId;
    // a settlement ("site:<catalog id>"): its body from the catalog
    const bool toSettlement = landingSpec.rfind("site:", 0) == 0;
    const std::string settlementBody = toSettlement ? world::celestial::settlements::surfaceSettlementBody(landingSpec.substr(5))
                                                    : std::string();
    if (toSettlement && settlementBody.empty())
    {
        std::cerr << "[Autopilot] ship " << shipId << ": no surface settlement '" << landingSpec.substr(5) << "'" << std::endl;
        return false;
    }
    const std::string target = toSettlement ? settlementBody : bodyId.empty() ? ship.primaryBodyId : bodyId;
    const GravityBody* body = find(env, target);
    if (target.empty() || !body)
    {
        std::cerr << "[Autopilot] ship " << shipId << ": no gravity body '" << target << "'" << std::endl;
        return false;
    }
    // a landing must be survivable and reversible: main engines vs surface gravity; an atmosphere takes shuttles only
    bool belly = false;
    if (body->radiusMeters > 0.0)
    {
        const GravityBody& b = *body;
        const double gSurface = b.gravitationalParameterM3s2 / (b.radiusMeters * b.radiusMeters);
        const double main = mainEngineAccelMps2(ship.physics);
        if (!game::navigation::rules::isAirless(b.radiusMeters, b.atmosphereRadiusMeters) && !ship.shuttle)
        {
            refusal = code(FlightRefusal::LandingAtmosphere);
            std::cerr << "[Autopilot] ship " << shipId << ": REFUSED " << target << " - an atmosphere: only shuttles land" << std::endl;
            return false;
        }
        if (main < jd::LandingThrustToWeightMin * gSurface)
        {
            refusal = code(FlightRefusal::LandingThrustTooLow);
            std::cerr << "[Autopilot] ship " << shipId << ": REFUSED " << target << " - main engines " << main / 9.81
                      << " g vs surface gravity " << gSurface / 9.81 << " g (need x" << jd::LandingThrustToWeightMin << ")" << std::endl;
            return false;
        }
        // belly down when the ventral RCS the Newtonian law really commands holds the weight
        const bool assistedPlant = ship.nativeLaw == LocalFlightControlLaw::Assisted;
        const double ventral = assistedPlant ? game::ship::assistedLateralStabilizationAccelerationLimitMps2(ship.physics)
                                             : landing::surfaceRcsMps2(ship.physics, 1, 1.0);
        belly = ventral >= jd::LandingThrustToWeightMin * gSurface;
        std::cerr << "[Autopilot] ship " << shipId << ": landing check " << target << " - main engines " << main / 9.81 << " g, "
                  << (assistedPlant ? "Assisted vectoring lift " : "ventral RCS ") << ventral / 9.81 << " g vs surface "
                  << gSurface / 9.81 << " g -> "
                  << (belly ? "BELLY-DOWN landing" : "tail-down landing (the ventral thrusters cannot hold the weight)") << ", J floor "
                  << jd::floorAltitudeM(b.radiusMeters, b.atmosphereRadiusMeters) / 1000.0 << " km" << std::endl;
    }
    *this = BodyAutopilot {};
    m_bellyLanding = belly;
    m_targetId = target;
    m_departureId = departureBodyId;
    // landing: "pad:<lat>,<lon>" (degrees, body-fixed) or a wild-terrain site search
    double lat = 0.0, lon = 0.0;
    if (toSettlement)
    {
        for (const auto& layout : world::celestial::settlements::surfaceSettlementsOn(target, body->radiusMeters))
            if (layout.siteId == landingSpec.substr(5))
                if (const auto* pad = layout.pad(1))
                {
                    m_landingRequest.mode = landing::LandingMode::Pad;
                    m_landingRequest.padBodyDirection = pad->bodyDirection;
                    std::cerr << "[Autopilot] ship " << shipId << ": landing at the settlement " << layout.name << " (" << layout.siteId
                              << "), pad 1" << std::endl;
                }
    }
    else if (landingSpec.rfind("pad:", 0) == 0 && std::sscanf(landingSpec.c_str() + 4, "%lf,%lf", &lat, &lon) == 2)
    {
        const double la = glm::radians(lat), lo = glm::radians(lon);
        m_landingRequest.mode = landing::LandingMode::Pad;
        // body basis (prime, north, east)
        m_landingRequest.padBodyDirection = glm::dvec3(std::cos(la) * std::cos(lo), std::sin(la), std::cos(la) * std::sin(lo));
    }
    m_stage = AutopilotStage::Cruise;
    m_lawBefore = ship.currentLaw;
    std::cerr << "[Autopilot] ship " << shipId << ": ENGAGED -> " << target << " (departure " << m_departureId << "), landing "
              << (m_landingRequest.mode == landing::LandingMode::Pad ? "on a pad " + landingSpec : std::string("wild: flat-site search"))
              << std::endl;
    return true;
}

BodyAutopilotOutput BodyAutopilot::step(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, double dt)
{
    BodyAutopilotOutput out;
    out.refusal = m_refusal;
    out.control.globalFlightCommandValid = true;
    const GravityBody* target = find(env, m_targetId);
    if (!target)
    {
        m_targetId.clear();
        m_stage = AutopilotStage::None;
        m_pilotActive = false;
        out.control.globalFlightCommandValid = false;
        out.finished = true;
        return out;
    }
    if (dt <= 0.0)
    {
        out.control.globalFlightCommandValid = false;
        return out;
    }
    if (m_stage == AutopilotStage::GroundStabilized)
    {
        // the terminal: the ground stabilization (asked again every step until the plant is in it)
        stepSurface(env, ship, *target, dt, out);
        out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::GroundStabilization);
        return out;
    }
    // the transfer and the de-orbit fly in normal flight
    out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::NormalFlight);
    const bool haveVelocity = m_haveLastCenter;
    const glm::dvec3 targetVelocity = haveVelocity ? (target->centerMeters - m_lastTargetCenter) / dt : glm::dvec3(0.0);
    m_lastTargetCenter = target->centerMeters;
    m_haveLastCenter = true;
    if (!haveVelocity)
    {
        out.control.globalFlightCommandValid = false;
        return out;
    }
    if (m_stage == AutopilotStage::Deorbit)
        stepDescent(env, ship, *target, targetVelocity, dt, out);
    else
        stepTransfer(env, ship, *target, targetVelocity, dt, out);
    m_refusal = out.refusal;
    // the ground's turn under the ship: the target's basis now (the next sample's "last")
    glm::dmat3 basis(1.0);
    m_haveLastBasis = env.bodyOrientation && env.bodyOrientation(target->id, basis);
    m_lastBasis = basis;
    return out;
}

void BodyAutopilot::stepTransfer(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& tgt,
                                 const glm::dvec3& targetVelocity, double dt, BodyAutopilotOutput& out)
{
    const GravityBody* target = &tgt;
    const std::uint32_t shipId = ship.shipId;
    ShipControlState& c = out.control;
    m_seconds += dt;

    static const double maxAccel = [] {
        const char* v = std::getenv("ELITE_J_AUTOPILOT_MAX_ACCEL");
        const double a = v && *v ? std::atof(v) : 0.0;
        return a > 0.0 ? a : jd::MaxAccelMps2;
    }();
    // J ends above the target's J floor; the descent below it is on the main engines
    const double AimAltitudeM = jd::floorAltitudeM(target->radiusMeters, target->atmosphereRadiusMeters);
    constexpr double ApproachTauS = jd::ApproachTauS;
    const double mainAccel = mainEngineAccelMps2(ship.physics);

    const glm::dvec3 position = ship.positionM;
    const glm::dvec3 velocity = ship.velocityMps;
    const glm::dvec3 rel = position - target->centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    const glm::dvec3 w = velocity - targetVelocity;
    const double ground = ship.groundBodyId == target->id ? ship.groundHeightM : 0.0;
    const double height = rLen - target->radiusMeters - ground;
    // the drive: 1 g doubling every 2 s up to the cap
    const double ramp = std::min(maxAccel, 9.81 * std::exp2(m_rampSeconds / jd::DriveRampDoublingS));

    glm::dvec3 wanted(0.0);
    double limit = ramp;
    AutopilotStage phase = m_stage;
    const int shownPhase = static_cast<int>(phase);
    const double shownRange = height;
    glm::dvec3 orbitTangent(0.0);
    double orbitSpeed = 0.0;
    if (phase == AutopilotStage::OrbitHold)
    {
        // parking orbit: J off, no thrust - the orbit holds by itself; then the de-orbit
        m_orbitCoastSeconds += dt;
        const double vCirc = std::sqrt(target->gravitationalParameterM3s2 / rLen);
        if (m_seconds - m_lastLogSeconds >= 2.0)
        {
            m_lastLogSeconds = m_seconds;
            std::cerr << "[Autopilot] ship " << shipId << " ORBIT " << target->id << ": height " << height / 1000.0 << " km, speed "
                      << glm::length(w) << " m/s (circular " << vCirc << "), vertical " << glm::dot(w, up) << " m/s, coast "
                      << m_orbitCoastSeconds << " s" << std::endl;
        }
        if (m_orbitCoastSeconds >= 15.0 && m_landing.site().valid)
        {
            // the orbit held (stabilization), now the de-orbit: NORMAL flight on the main engines
            // (no envelope - the orbital speed is braked by the pilot's ordinary controls)
            std::cerr << "[Autopilot] ship " << shipId << ": orbit held " << m_orbitCoastSeconds << " s at " << height / 1000.0
                      << " km - DE-ORBIT in NORMAL flight (main engines)" << std::endl;
            m_stage = AutopilotStage::Deorbit;
            m_descentNose = glm::dvec3(0.0);
            m_glideInit = false;
            m_lawBefore = ship.currentLaw;
            m_seconds -= dt;   // the descent counts this tick
            stepDescent(env, ship, *target, targetVelocity, dt, out);
            return;
        }
        // a coast: a valid command with nothing in it (no J, no thrust, the hull left alone)
        out.displayPhase = code(AutopilotDisplayPhase::OrbitHold);
        out.displayTargetId = target->id;
        out.displayRangeM = height;
        out.displayRelSpeedMps = glm::length(w);
        out.displayAccelMps2 = 0.0f;
        if (out.refusal == code(FlightRefusal::BelowJFloorMainEngines))
            out.refusal = code(FlightRefusal::None);
        return;
    }
    {
        // choose the landing site AHEAD (from 200 000 km: the terrain loads in the background) and
        // aim the approach above it - the lander lands where it chose
        glm::dvec3 aimDirection = up;
        {
            glm::dmat3 worldFromBody(1.0);
            if (env.bodyOrientation)
                (void)env.bodyOrientation(target->id, worldFromBody);
            if (rLen < 2.0e8)
            {
                m_landingPlanSeconds -= dt;
                if (!m_landing.site().valid && m_landingPlanSeconds <= 0.0)
                {
                    m_landingPlanSeconds = 2.0;
                    const auto probe = probeFor(env, target->id, worldFromBody);
                    const glm::dvec3 hull = hullOf(ship.physics);
                    if (!m_landingStarted)
                    {
                        m_landing.start(m_landingRequest, glm::transpose(worldFromBody) * up);
                        m_landingStarted = true;
                    }
                    if (m_landing.planAhead(glm::transpose(worldFromBody) * up, target->radiusMeters, hull, probe))
                    {
                        const auto& site = m_landing.site();
                        std::cerr << "[Autopilot] ship " << shipId << ": landing site chosen AHEAD at " << height / 1000.0 << " km ("
                                  << (m_landingRequest.mode == landing::LandingMode::Pad ? "pad" : "wild search") << "): " << site.offsetM
                                  << " m from the approach point, slope " << site.slopeDeg << " deg, roughness " << site.roughnessM
                                  << " m, ground " << site.heightM << " m" << std::endl;
                    }
                }
            }
            if (m_landing.site().valid)
                aimDirection = glm::normalize(worldFromBody * m_landing.site().bodyDirection);
        }
        // the parking orbit: circular at the J floor, in the plane of the ship's approach and the
        // site; the insertion point lies the main-engine braking distance before the site, the
        // velocity there is the circular one toward the site
        {
            const double rOrb = target->radiusMeters + AimAltitudeM;
            orbitSpeed = std::sqrt(target->gravitationalParameterM3s2 / rOrb);
            glm::dvec3 n = glm::length(m_orbitNormal) > 0.5 ? m_orbitNormal : glm::cross(up, aimDirection);
            if (glm::length(n) < 1.0e-6)
                n = glm::cross(up, std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
            n = glm::normalize(n);
            if (m_landing.site().valid && glm::length(m_orbitNormal) < 0.5 && rLen < rOrb * 4.0)
                m_orbitNormal = n;   // the plane is fixed once the site is known and the body near
            const double gSurface = target->gravitationalParameterM3s2 / (target->radiusMeters * target->radiusMeters);
            // an along-path lander (a fore braking bank) glides (stepGlide): the de-orbit starts ~340 km
            // out (the offline model: the nose +8 deg sink needs ~330-400 km)
            const double brake = std::min(alongPathLander(ship.controlParams) ? 4.3 : 10.0, 0.5 * std::max(0.3, mainAccel - gSurface));
            const double theta = std::min(1.5, orbitSpeed * orbitSpeed / (2.0 * brake) / target->radiusMeters + 0.03);
            const glm::dvec3 insertion = glm::normalize(aimDirection * std::cos(theta) - glm::cross(n, aimDirection) * std::sin(theta));
            orbitTangent = glm::normalize(glm::cross(n, insertion));
            aimDirection = insertion;
            // close to the orbit: the nearest point of the orbit circle (the ship slides onto the
            // orbit instead of chasing a fixed point it overshoots)
            const glm::dvec3 relNow = position - target->centerMeters;
            if (glm::length(target->centerMeters + insertion * rOrb - position) < 50000.0)
                m_orbitAcquire = true;
            if (m_orbitAcquire)
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
        const glm::dvec3 toAim = aim - position;
        const double d = glm::length(toAim);
        glm::dvec3 dir = d > 1e-3 ? toAim / d : glm::dvec3(0.0);
        phase = AutopilotStage::Cruise;
        // departure: never cut through the body left behind - first move off the line
        if (const GravityBody* departure = find(env, m_departureId); departure && departure != target && d > 1.0)
        {
            const glm::dvec3 s = position - departure->centerMeters;
            const double t = std::clamp(-glm::dot(s, toAim) / (d * d), 0.0, 1.0);
            if (glm::length(s + toAim * t) < departure->radiusMeters * 1.3)
            {
                glm::dvec3 side = s - dir * glm::dot(s, dir);
                if (glm::length(side) < 1.0)
                    side = glm::cross(dir, std::abs(dir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
                dir = glm::normalize(side);
                phase = AutopilotStage::Departure;
            }
        }
        // never through the target either (an aim point on its far side): go round
        if (phase == AutopilotStage::Cruise && d > 1.0)
        {
            const glm::dvec3 s = position - target->centerMeters;
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
        if (phase == AutopilotStage::Cruise && tail < braking)
            phase = AutopilotStage::Approach;
        // tangential: the closing velocity on top of the orbit velocity at the insertion point
        wanted = dir * std::min(braking, tail) + orbitTangent * orbitSpeed;
        if ((phase == AutopilotStage::Approach || m_orbitAcquire) && d < 1000.0 && glm::length(w - orbitTangent * orbitSpeed) < 5.0)
        {
            phase = AutopilotStage::OrbitHold;
            m_orbitCoastSeconds = 0.0;
            std::cerr << "[Autopilot] ship " << shipId << ": ON ORBIT of " << target->id << " at " << height / 1000.0 << " km, "
                      << orbitSpeed << " m/s (J off, coast) after " << m_seconds << " s" << std::endl;
        }
    }
    m_stage = phase;

    // the J speed cap (0.9 c against the target)
    if (glm::length(wanted) > jd::SpeedCapMps)
        wanted *= jd::SpeedCapMps / glm::length(wanted);
    // deadbeat velocity servo within the drive's authority (gravity was integrated this tick): the
    // plant's J field tracks the world velocity target within the limit
    glm::dvec3 targetWorld = targetVelocity + wanted;
    // no J field near objects: a gentle push out of the nearest keep-out (not in the powered
    // descent, which flies on the main engines)
    if (KeepOutHit hit; env.keepOut && env.keepOut(position, hit))
    {
        const glm::dvec3 off = position - hit.positionM;
        const glm::dvec3 away = glm::length(off) > 1.0 ? glm::normalize(off) : ship.forward;
        targetWorld = hit.velocityMps + away * jd::GentleSpeedMps;
        limit = std::min(limit, jd::GentleAccelMps2);
        if (out.refusal != code(FlightRefusal::JDroppedNearObject) || m_seconds - m_lastLogSeconds >= 1.9)
            std::cerr << "[Autopilot] ship " << shipId << ": J keep-out of " << hit.objectId << " at " << hit.distanceM / 1000.0 << " km ("
                      << glm::length(hit.velocityMps - velocity) << " m/s relative)" << std::endl;
        out.refusal = code(FlightRefusal::JDroppedNearObject);
        m_rampSeconds = 0.0;   // the full drive ramps from 1 g again outside the keep-out
    }
    else
    {
        if (out.refusal == code(FlightRefusal::JDroppedNearObject))
            out.refusal = code(FlightRefusal::None);
        m_rampSeconds += dt;
    }
    glm::dvec3 dv = targetWorld - velocity;
    // below the J floor of the reference body: no J field - the ship's own main engines (thrust along
    // the hull, the hull turns toward the thrust it needs). Leaving a body: a straight climb against
    // it to its floor first. (A landing never gets here: it leaves J on the parking orbit and flies
    // in normal flight.)
    bool mainEngine = false;
    const GravityBody* floorBody = find(env, ship.primaryBodyId);
    if (floorBody && ship.primaryAltitudeM < jd::floorAltitudeM(floorBody->radiusMeters, floorBody->atmosphereRadiusMeters))
    {
        mainEngine = true;
        if (floorBody != target)
        {
            const glm::dvec3 refVel = m_lastPrimaryId == floorBody->id ? (floorBody->centerMeters - m_lastPrimaryCenter) / dt : glm::dvec3(0.0);
            const glm::dvec3 upRef = glm::normalize(position - floorBody->centerMeters);
            dv = refVel + upRef * jd::FloorClimbSpeedMps - velocity;
        }
    }
    m_lastPrimaryId = floorBody ? floorBody->id : std::string();
    m_lastPrimaryCenter = floorBody ? floorBody->centerMeters : glm::dvec3(0.0);
    glm::dvec3 thrustWanted(0.0);
    glm::dvec3 dvApplied(0.0);
    if (mainEngine)
    {
        // a thrust-axis lander: the acceleration it wants = velocity error / tau + gravity
        // compensation (gravity is integrated with the frame); the nose turns toward it and the
        // engine gives its projection on the nose, within the real main-engine thrust
        constexpr double TauS = 1.0;
        limit = std::min(limit, mainAccel);
        m_rampSeconds = 0.0;
        if (out.refusal != code(FlightRefusal::JDroppedNearObject))
            out.refusal = code(FlightRefusal::BelowJFloorMainEngines);
        const glm::dvec3 gravity = env.bodies ? GravityFieldSystem::sample(position, *env.bodies).accelerationMps2 : glm::dvec3(0.0);
        thrustWanted = dv / TauS - gravity;
        const glm::dvec3 noseNow = glm::normalize(ship.forward);
        const double reverse = ship.physics.reverseMainEngineAvailable ? -limit : 0.0;
        const double thrust = std::clamp(glm::dot(thrustWanted, noseNow), reverse, limit);
        c.globalMainAccelWorldMps2 = noseNow * thrust;
        dvApplied = noseNow * (thrust * dt);
    }
    else
    {
        if (out.refusal == code(FlightRefusal::BelowJFloorMainEngines))
            out.refusal = code(FlightRefusal::None);
        c.globalJVelocityTargetValid = true;
        c.globalJVelocityTargetWorldMps = targetWorld;
        c.globalJAccelLimitMps2 = limit;
        dvApplied = dv;
        const double maxDv = limit * dt;
        if (glm::length(dvApplied) > maxDv)
            dvApplied *= maxDv / glm::length(dvApplied);
    }
    const glm::dvec3 accel = dvApplied / dt;

    // the hull: in the transfer the nose points the drive; on the main engines the nose points the
    // thrust it needs (tail down while hovering)
    glm::dvec3 noseWanted(0.0);
    if (mainEngine)
    {
        const glm::dvec3 upLocal = glm::normalize(position - (floorBody ? floorBody->centerMeters : target->centerMeters));
        noseWanted = glm::length(thrustWanted) > 0.05 ? glm::normalize(thrustWanted) : upLocal;
    }
    else
    {
        // the J drive pushes both ways: the nose follows the flight - along the velocity against
        // the target (prograde), the target ahead when nearly at rest
        const glm::dvec3 toTarget = target->centerMeters + up * target->radiusMeters - position;
        const glm::dvec3 wNow = w + dvApplied;
        if (glm::length(wNow) > 20.0)
            noseWanted = glm::normalize(wNow);
        else if (glm::length(toTarget) > 1.0)
            noseWanted = glm::normalize(toTarget);
    }
    if (glm::length(noseWanted) > 0.5)
        attitude(c, noseWanted, ship.up);

    const glm::dvec3 wAfter = w + dvApplied;
    out.displayPhase = static_cast<std::uint8_t>(shownPhase);
    out.displayTargetId = target->id;
    out.displayRangeM = shownRange;
    out.displayRelSpeedMps = glm::length(wAfter);
    // signed along the velocity: braking is a negative acceleration (the speed still grows while it
    // is positive, however small)
    out.displayAccelMps2 = static_cast<float>(glm::length(wAfter) > 1.0e-3 ? glm::dot(accel, glm::normalize(wAfter)) : glm::length(accel));
    if (m_seconds - m_lastLogSeconds >= 2.0)
    {
        m_lastLogSeconds = m_seconds;
        std::cerr << "[Autopilot] ship " << shipId << " t+" << m_seconds << " s phase " << static_cast<int>(phase) << " -> " << target->id
                  << ": height " << height / 1000.0 << " km, speed " << glm::length(wAfter) / 1000.0 << " km/s, accel "
                  << glm::length(accel) << " m/s2 (drive " << ramp << ")" << std::endl;
    }
}

void BodyAutopilot::stepDescent(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& body,
                                const glm::dvec3& bodyVelocityMps, double dt, BodyAutopilotOutput& out)
{
    const std::uint32_t shipId = ship.shipId;
    if (dt <= 0.0 || !m_landing.site().valid)
        return;
    m_seconds += dt;
    const ShipParams& params = ship.controlParams;
    const glm::dvec3 rel = ship.positionM - body.centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    glm::dmat3 worldFromBody(1.0);
    const bool basis = env.bodyOrientation && env.bodyOrientation(body.id, worldFromBody);
    const auto& site = m_landing.site();
    const glm::dvec3 siteDir = glm::normalize(worldFromBody * site.bodyDirection);
    // over the ground: the body's velocity and its turn under the ship
    glm::dvec3 groundVel = bodyVelocityMps;
    if (basis && m_haveLastBasis)
        groundVel += (worldFromBody * glm::transpose(m_lastBasis) * rel - rel) / dt;
    const glm::dvec3 v = ship.velocityMps - groundVel;
    const double vz = glm::dot(v, up);
    const glm::dvec3 vhVec = v - up * vz;
    const double vh = glm::length(vhVec);
    // the ground: the terrain under the ship when it is sampled (low), else the site's
    const bool terrainKnown = ship.groundBodyId == body.id && ship.groundMaterial >= 0;
    const double groundM = terrainKnown ? ship.groundHeightM : site.heightM + halfHeightAlong(ship.physics, ship.right, ship.up, ship.forward, up);
    const double h = rLen - body.radiusMeters - groundM;
    // to the site along the surface
    const double arc = std::acos(std::clamp(glm::dot(up, siteDir), -1.0, 1.0)) * body.radiusMeters;
    glm::dvec3 toSite = siteDir - up * glm::dot(siteDir, up);
    toSite = glm::length(toSite) > 1.0e-12 ? glm::normalize(toSite) : (vh > 1.0 ? vhVec / vh : ship.forward);
    const double mainAccel = std::max(1.0, landing::surfaceMainEngineAccelMps2(params));
    const double g = body.gravitationalParameterM3s2 / (rLen * rLen);
    if (alongPathLander(params))
    {
        // the aircraft-like de-orbit (author 2026-11-07) - see stepGlide
        GlideSample gs;
        gs.arcM = arc;
        gs.heightM = h;
        gs.horizontalMps = glm::dot(vhVec, toSite);
        gs.verticalMps = vz;
        gs.radiusM = rLen - h;
        gs.toSite = toSite;
        gs.up = up;
        gs.velocityMps = v;
        gs.terrainKnown = terrainKnown;
        stepGlide(env, ship, body, bodyVelocityMps, gs, dt, out);
        return;
    }

    // the wanted velocity over the ground: toward the site, braking at 35 % of the main engine (the
    // rest holds the weight and the corrections), an approach tail arc / 15 s; down along a glide
    // that reaches the hand-over height as the arc closes
    constexpr double HandoverHeightM = 1500.0;
    constexpr double HandoverArcM = 1500.0;
    // never faster than the circular speed here: the descent only brakes the orbit
    const double vhWanted = std::min({std::sqrt(2.0 * 0.35 * mainAccel * arc), arc / 15.0, std::sqrt(body.gravitationalParameterM3s2 / rLen)});
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
        m_descentNose = glm::normalize(a);
    const glm::dvec3 noseHint = glm::length(m_descentNose) > 0.5 ? m_descentNose : (vh > 1.0 ? -vhVec / vh : ship.forward);

    landing::SurfacePilotRequest rq;
    rq.actualVelocityMps = v;
    rq.desiredVelocityMps = vWanted;
    rq.gravityMps2 = -up * g;
    rq.upLocal = up;
    rq.headingHint = toSite;
    rq.forward = ship.forward;
    rq.right = ship.right;
    rq.up = ship.up;
    rq.pitchRateRadPerSec = ship.pitchRate;
    rq.yawRateRadPerSec = ship.yawRate;
    rq.rollRateRadPerSec = ship.rollRate;
    rq.bellyDown = true;
    rq.dt = dt;
    landing::SurfacePilotResult pilot = landing::flyNormalFlightThrust(rq, params, a, noseHint);
    if (ship.currentLaw != ship.nativeLaw)
    {
        pilot.control.localControlLawCommandValid = true;
        pilot.control.requestedLocalControlLaw = ship.nativeLaw;
    }
    if (!m_pilotActive)
        std::cerr << "[Autopilot] ship " << shipId << ": de-orbit pilot ON (normal flight, main engines along the nose, law "
                  << (ship.nativeLaw == LocalFlightControlLaw::Assisted ? "Assisted" : "Newtonian") << ")" << std::endl;
    m_pilotActive = true;
    out.control = pilot.control;             // the ordinary controls; the global part says "the autopilot flies"
    out.control.globalFlightCommandValid = true;
    out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::NormalFlight);
    out.pilotActive = true;

    out.displayPhase = code(AutopilotDisplayPhase::Deorbit);
    out.displayTargetId = body.id;
    out.displayRangeM = arc;
    out.displayRelSpeedMps = glm::length(v);
    out.displayAccelMps2 = static_cast<float>(glm::length(v) > 1.0 ? glm::dot(ship.mainEngineAccelerationMps2, glm::normalize(v))
                                                                    : glm::length(ship.mainEngineAccelerationMps2));
    if (m_seconds - m_pilotLastLogSeconds >= 2.0)
    {
        m_pilotLastLogSeconds = m_seconds;
        const double noseOff = glm::degrees(std::acos(std::clamp(glm::dot(pilot.noseWanted, glm::normalize(rq.forward)), -1.0, 1.0)));
        std::cerr << "[Autopilot] ship " << shipId << " DE-ORBIT t+" << m_seconds << " s: height " << h << " m (ground "
                  << (terrainKnown ? "sampled" : "site") << "), to the site " << arc / 1000.0 << " km, horizontal " << vh
                  << " m/s (wanted " << vhWanted << "), vertical " << vz << " m/s (wanted " << vzWanted << "), accel wanted "
                  << glm::length(a) << " m/s2, main " << pilot.mainCommand * 100.0 << " % -> "
                  << glm::length(ship.mainEngineAccelerationMps2) << " m/s2, nose off " << noseOff << " deg" << std::endl;
    }

    // hand-over: low and slow near the site -> stabilization against the ground (the 500 m/s
    // envelope over the surface), where the landing module flies the touchdown
    if ((arc < 2.0 * HandoverArcM && glm::length(v) < 80.0 && h < 1.7 * HandoverHeightM) || (h < 600.0 && glm::length(v) < 450.0))
    {
        std::cerr << "[Autopilot] ship " << shipId << ": HAND-OVER to the ground stabilization at height " << h << " m, " << arc
                  << " m from the site, " << glm::length(v) << " m/s over the ground after " << m_seconds << " s" << std::endl;
        m_stage = AutopilotStage::GroundStabilized;
        BodyAutopilotShip inFrame = ship;
        inFrame.referenceVelocityMps = bodyVelocityMps;
        stepSurface(env, inFrame, body, dt, out);
        out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::GroundStabilization);
    }
}

// ---- the aircraft-like de-orbit (author 2026-11-07: "посадку как у самолёта: снижение по дуге, в
// самом финале - зависание над площадкой и вертикальная посадка"). A ship with a fore (braking)
// main bank. In vacuum a hull with an axial engine has no lift: the vertical comes only from the
// thrust's tilt. An offline study (2D, the Moon; the live q3 run reproduced within a few %) showed
// that a pitch within ~ +-25 deg cannot both bring the ship down ~80 km and stop the sink before the
// ground; so the glide ends HIGH and inside the ground stabilization's envelope, whose vectoring
// thrust stops the sink with the hull level and flies the level approach, the hover and the vertical
// touchdown (LandingAutopilot). The glide itself flies two attitudes:
//   1 - the sink: the nose PitchUp above the horizon toward the site, the fore bank pushing back and
//       down (an aircraft at a high angle of attack, the surface flowing below);
//   2 - the braking: the nose PitchDown below the horizon, the fore bank braking and holding the sink.
// Every second a planner (2D: the body's pull less the orbital lift) re-solves the thrust: in phase 2
// the one that stops at EndArc from the site; in phase 1 the one that, with the switch made when the
// nominal braking from there would end at EndArc, ends at EndHeight.
namespace glideplan
{
constexpr double PitchUpRad = 0.13962634015954636;    // 8 deg: the horizon (~18 deg below from 80 km) stays in view (live q4: +15 deg showed only stars)
constexpr double PitchDownRad = -0.3490658503988659;  // -20 deg
constexpr double EndSpeedMps = 450.0;                 // inside the ground stabilization's envelope
constexpr double EndArcM = 15000.0;
constexpr double EndHeightM = 8000.0;
constexpr double NominalBrakeMps2 = 10.0;

struct S
{
    double arc, h, vh, vz;
};

inline void advance(S& s, double mu, double r0, double pitch, double thrust, double dt)
{
    const double r = r0 + s.h;
    const double deficit = mu / (r * r) - s.vh * s.vh / r;   // the pull less the orbital lift
    s.vh += -thrust * std::cos(pitch) * dt;                  // the fore bank: back along the nose
    s.vz += (-thrust * std::sin(pitch) - deficit) * dt;
    s.arc -= s.vh * dt;
    s.h += s.vz * dt;
}

// a constant attitude and thrust until the speed falls to EndSpeed (or the ground)
inline S coast(S s, double mu, double r0, double pitch, double thrust, double dt)
{
    for (double time = 0.0; time < 1500.0 && std::hypot(s.vh, s.vz) > EndSpeedMps && s.h >= 0.0; time += dt)
        advance(s, mu, r0, pitch, thrust, dt);
    return s;
}

inline bool brakeFitsNow(const S& s, double mu, double r0)
{
    return coast(s, mu, r0, PitchDownRad, NominalBrakeMps2, 4.0).arc <= EndArcM;
}

// phase 2's thrust that stops the ship at EndArc (more thrust = shorter)
inline double brakeFor(const S& s, double mu, double r0, double maxThrust)
{
    double lo = 0.5, hi = std::max(1.0, maxThrust);
    for (int i = 0; i < 18; ++i)
    {
        const double m = 0.5 * (lo + hi);
        if (coast(s, mu, r0, PitchDownRad, m, 2.0).arc < EndArcM)
            lo = m;
        else
            hi = m;
    }
    return 0.5 * (lo + hi);
}

// the end height when phase 1 flies with `thrust` until the nominal braking fits, then brakes
inline double endHeight(S s, double mu, double r0, double thrust)
{
    for (double time = 0.0; time < 1500.0 && !brakeFitsNow(s, mu, r0); time += 2.0)
    {
        advance(s, mu, r0, PitchUpRad, thrust, 2.0);
        if (s.h < 0.0)
            return -1.0e9;
    }
    return coast(s, mu, r0, PitchDownRad, NominalBrakeMps2, 2.0).h;
}

// phase 1's thrust: more pushes down (a lower end)
inline double sinkFor(const S& s, double mu, double r0, double maxThrust)
{
    double lo = 0.0, hi = std::min(15.0, maxThrust);
    for (int i = 0; i < 14; ++i)
    {
        const double m = 0.5 * (lo + hi);
        if (endHeight(s, mu, r0, m) > EndHeightM)
            lo = m;
        else
            hi = m;
    }
    return 0.5 * (lo + hi);
}
} // namespace glideplan

void BodyAutopilot::stepGlide(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& body,
                              const glm::dvec3& bodyVelocityMps, const GlideSample& gs, double dt, BodyAutopilotOutput& out)
{
    namespace gp = glideplan;
    const std::uint32_t shipId = ship.shipId;
    const ShipParams& params = ship.controlParams;
    const double mu = body.gravitationalParameterM3s2;
    const double reverseMax = std::max(1.0e-3, static_cast<double>(params.reverseMainEngineAccelerationMps2));
    const gp::S now {gs.arcM, gs.heightM, gs.horizontalMps, gs.verticalMps};
    if (!m_glideInit)
    {
        m_glideInit = true;
        m_glidePhase = 1;
        m_glideReplanSeconds = -1.0e30;
        std::cerr << "[Autopilot] ship " << shipId << ": GLIDE (like an aircraft) from " << gs.arcM / 1000.0 << " km to the site, "
                  << gs.heightM / 1000.0 << " km high, " << gs.horizontalMps << " m/s - the nose " << glm::degrees(gp::PitchUpRad)
                  << " deg up while sinking, " << glm::degrees(gp::PitchDownRad) << " deg while braking; the ground stabilization from "
                  << gp::EndSpeedMps << " m/s" << std::endl;
    }
    if (m_seconds - m_glideReplanSeconds >= 1.0)
    {
        m_glideReplanSeconds = m_seconds;
        if (m_glidePhase == 1 && gp::brakeFitsNow(now, mu, gs.radiusM))
        {
            m_glidePhase = 2;
            std::cerr << "[Autopilot] ship " << shipId << ": GLIDE braking (nose down) at " << gs.arcM / 1000.0 << " km, "
                      << gs.heightM / 1000.0 << " km high, " << gs.horizontalMps << " / " << gs.verticalMps << " m/s" << std::endl;
        }
        m_glideThrustMps2 = m_glidePhase == 1 ? gp::sinkFor(now, mu, gs.radiusM, reverseMax) : gp::brakeFor(now, mu, gs.radiusM, reverseMax);
    }
    const double pitch = m_glidePhase == 1 ? gp::PitchUpRad : gp::PitchDownRad;
    const glm::dvec3 nose = glm::normalize(gs.toSite * std::cos(pitch) + gs.up * std::sin(pitch));

    landing::SurfacePilotRequest rq;
    rq.forward = ship.forward;
    rq.right = ship.right;
    rq.up = ship.up;
    rq.upLocal = gs.up;
    rq.pitchRateRadPerSec = ship.pitchRate;
    rq.yawRateRadPerSec = ship.yawRate;
    rq.rollRateRadPerSec = ship.rollRate;
    rq.dt = dt;
    landing::SurfacePilotResult pilot;
    pilot.noseWanted = nose;
    landing::surfaceAttitudeInputs(rq, params, nose, gs.up, pilot.control);   // belly toward the ground
    // the fore bank fires once the hull holds the attitude (full within 4 deg, none beyond 20 deg)
    const double align = glm::dot(glm::normalize(ship.forward), nose);
    const double lo = std::cos(glm::radians(20.0)), hi = std::cos(glm::radians(4.0));
    const double t = std::clamp((align - lo) / (hi - lo), 0.0, 1.0);
    const double command = -std::clamp(m_glideThrustMps2 / reverseMax, 0.0, 1.0) * t * t * (3.0 - 2.0 * t);
    pilot.control.targetSpeedRate = static_cast<float>(command);
    pilot.mainCommand = command;
    if (ship.currentLaw != ship.nativeLaw)
    {
        pilot.control.localControlLawCommandValid = true;
        pilot.control.requestedLocalControlLaw = ship.nativeLaw;
    }
    if (!m_pilotActive)
        std::cerr << "[Autopilot] ship " << shipId << ": de-orbit pilot ON (normal flight, the aircraft-like glide on the fore bank, law "
                  << (ship.nativeLaw == LocalFlightControlLaw::Assisted ? "Assisted" : "Newtonian") << ")" << std::endl;
    m_pilotActive = true;
    out.control = pilot.control;
    out.control.globalFlightCommandValid = true;
    out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::NormalFlight);
    out.pilotActive = true;
    out.displayPhase = code(AutopilotDisplayPhase::Deorbit);
    out.displayTargetId = body.id;
    out.displayRangeM = gs.arcM;
    out.displayRelSpeedMps = glm::length(gs.velocityMps);
    // signed along the velocity (the HUD: "-" braking): the fore bank brakes - not "разгон"
    out.displayAccelMps2 = static_cast<float>(glm::length(gs.velocityMps) > 1.0 ? glm::dot(ship.mainEngineAccelerationMps2, glm::normalize(gs.velocityMps))
                                                                                 : glm::length(ship.mainEngineAccelerationMps2));
    if (m_seconds - m_pilotLastLogSeconds >= 2.0)
    {
        m_pilotLastLogSeconds = m_seconds;
        const double hullPitch = glm::degrees(std::asin(std::clamp(glm::dot(glm::normalize(ship.forward), gs.up), -1.0, 1.0)));
        const double pathPitch = glm::degrees(std::atan2(gs.verticalMps, std::max(1.0e-3, gs.horizontalMps)));
        std::cerr << "[Autopilot] ship " << shipId << " GLIDE " << m_glidePhase << " t+" << m_seconds << " s: height " << gs.heightM
                  << " m (ground " << (gs.terrainKnown ? "sampled" : "site") << "), to the site " << gs.arcM / 1000.0 << " km, horizontal "
                  << gs.horizontalMps << " m/s, vertical " << gs.verticalMps << " m/s, path " << pathPitch << " deg, hull pitch "
                  << hullPitch << " deg (wanted " << glm::degrees(pitch) << "), fore bank " << m_glideThrustMps2 << " m/s2, cmd "
                  << command * 100.0 << " % -> " << glm::length(ship.mainEngineAccelerationMps2) << " m/s2" << std::endl;
    }
    // hand-over: inside the ground stabilization's envelope -> its vectoring stops the sink with the
    // hull level and flies the level approach, the hover and the vertical touchdown
    if (glm::length(gs.velocityMps) <= gp::EndSpeedMps || (gs.heightM < 2000.0 && glm::length(gs.velocityMps) < 490.0))
    {
        std::cerr << "[Autopilot] ship " << shipId << ": HAND-OVER to the ground stabilization at height " << gs.heightM << " m, "
                  << gs.arcM << " m from the site, " << glm::length(gs.velocityMps) << " m/s over the ground (vertical " << gs.verticalMps
                  << ") after " << m_seconds << " s" << std::endl;
        m_stage = AutopilotStage::GroundStabilized;
        BodyAutopilotShip inFrame = ship;
        inFrame.referenceVelocityMps = bodyVelocityMps;
        stepSurface(env, inFrame, body, dt, out);
        out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::GroundStabilization);
    }
}

void BodyAutopilot::stepSurface(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, const GravityBody& tgt, double dt,
                                BodyAutopilotOutput& out)
{
    const GravityBody* target = &tgt;
    const std::uint32_t shipId = ship.shipId;
    m_seconds += dt;
    const glm::dvec3 shipPos = ship.positionM;
    const glm::dvec3 shipVel = ship.velocityMps;
    const glm::dvec3 bodyVel = ship.referenceVelocityMps;
    const glm::dvec3 rel = shipPos - target->centerMeters;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    const double ground = ship.groundBodyId == target->id ? ship.groundHeightM : 0.0;

    glm::dmat3 worldFromBody(1.0);
    if (env.bodyOrientation)
        (void)env.bodyOrientation(target->id, worldFromBody);
    const auto probe = probeFor(env, target->id, worldFromBody);
    const glm::dvec3 hull = hullOf(ship.physics);
    if (!m_landingStarted)
    {
        m_landing.start(m_landingRequest, glm::transpose(worldFromBody) * up);
        m_landingStarted = true;
    }
    const ShipParams& params = ship.controlParams;
    const bool hadSite = m_landing.site().valid;
    landing::LandingInput in;
    in.bodyCentreM = target->centerMeters;
    in.bodyVelocityMps = bodyVel;
    in.worldFromBody = worldFromBody;
    in.bodyRadiusM = target->radiusMeters;
    in.bodyMuM3s2 = target->gravitationalParameterM3s2;
    in.shipPositionM = shipPos;
    in.shipVelocityMps = shipVel;
    in.shipForward = ship.forward;
    in.hullHalfExtentsM = hull;
    in.groundUnderShipM = ground;
    in.dt = dt;
    in.authorityMps2 = landing::surfaceMainEngineAccelMps2(params);
    in.touchdownHalfHeightM = halfHeightAlong(ship.physics, ship.right, ship.up, ship.forward, up);
    const landing::LandingCommand cmd = m_landing.step(in, probe);
    if (!hadSite && m_landing.site().valid)
    {
        const auto& site = m_landing.site();
        std::cerr << "[Autopilot] ship " << shipId << ": landing site ("
                  << (m_landingRequest.mode == landing::LandingMode::Pad ? "pad" : "wild search") << ") " << site.offsetM
                  << " m from the start, slope " << site.slopeDeg << " deg, roughness " << site.roughnessM << " m, ground " << site.heightM
                  << " m, material " << site.material << " - " << m_landing.status() << std::endl;
    }

    // the virtual pilot: ordinary Newtonian controls (over the body, its own pull)
    landing::SurfacePilotRequest rq;
    rq.actualVelocityMps = shipVel - bodyVel;
    rq.desiredVelocityMps = rq.actualVelocityMps + cmd.velocityErrorMps;
    rq.gravityMps2 = -up * (target->gravitationalParameterM3s2 / (rLen * rLen));
    rq.upLocal = up;
    rq.headingHint = cmd.noseWorld;
    rq.forward = ship.forward;
    rq.right = ship.right;
    rq.up = ship.up;
    rq.pitchRateRadPerSec = ship.pitchRate;
    rq.yawRateRadPerSec = ship.yawRate;
    rq.rollRateRadPerSec = ship.rollRate;
    rq.bellyDown = m_bellyLanding;
    rq.dt = dt;
    // the ship's native law (its physical control model): the plant executes only that law
    const auto native = ship.nativeLaw;
    const bool assisted = native == LocalFlightControlLaw::Assisted;
    // Assisted flies the terminal part in its Backspace mode (no speed servo: the keypad axes are the
    // vectoring main's accelerations) - the speed servo along the nose turned the hull's levelling into
    // a 170 m/s horizontal run (live, 2026-11-06)
    if (assisted && ship.currentLaw == native && !ship.assistedFreeRotation)
    {
        out.setAssistedFreeRotation = true;
        m_freeRotation = true;
    }
    landing::SurfacePilotResult pilot = assisted ? landing::flySurfaceLandingVectoring(rq, params) : landing::flySurfaceLanding(rq, params);
    if (ship.currentLaw != native)
    {
        pilot.control.localControlLawCommandValid = true;
        pilot.control.requestedLocalControlLaw = native;
    }
    if (!m_pilotActive)
    {
        m_lawBefore = ship.currentLaw;
        std::cerr << "[Autopilot] ship " << shipId << ": landing pilot ON (normal flight, the ship's native law "
                  << (assisted ? "Assisted" : "Newtonian") << (ship.currentLaw != native ? " - switching to it" : "") << "; "
                  << (assisted || m_bellyLanding ? "belly down" : "tail down") << ")" << std::endl;
    }
    m_pilotActive = true;
    out.control = pilot.control;
    out.control.globalFlightCommandValid = true;
    out.pilotActive = true;

    const auto lp = m_landing.phase();
    out.displayPhase = code(lp == landing::LandingPhase::Planning  ? AutopilotDisplayPhase::SiteChoice
                            : lp == landing::LandingPhase::Orbit   ? AutopilotDisplayPhase::OrbitSiteSearch
                            : lp == landing::LandingPhase::Transit ? AutopilotDisplayPhase::ToPad
                                                                   : AutopilotDisplayPhase::PadDescent);
    out.displayTargetId = target->id;
    out.displayRangeM = lp == landing::LandingPhase::Transit ? cmd.distanceToSiteM : cmd.heightAboveGroundM;
    out.displayRelSpeedMps = glm::length(rq.actualVelocityMps);
    out.displayAccelMps2 = static_cast<float>(glm::length(ship.engineAccelerationMps2));
    if (m_seconds - m_pilotLastLogSeconds >= 2.0)
    {
        m_pilotLastLogSeconds = m_seconds;
        const glm::dvec3 v = rq.actualVelocityMps;
        const double noseOff = glm::degrees(std::acos(std::clamp(glm::dot(pilot.noseWanted, rq.forward), -1.0, 1.0)));
        std::cerr << "[Autopilot] ship " << shipId << " NORMAL t+" << m_seconds << " s " << m_landing.status() << ": height "
                  << cmd.heightAboveGroundM << " m, to the site " << cmd.distanceToSiteM / 1000.0 << " km, vertical " << glm::dot(v, up)
                  << " m/s, horizontal " << glm::length(v - up * glm::dot(v, up)) << " m/s, main " << pilot.mainCommand * 100.0
                  << " %, RCS (" << pilot.control.strafeInput << ", " << pilot.control.liftInput << ", " << pilot.control.forwardInput
                  << "), nose off " << noseOff << " deg, law " << int(ship.currentLaw) << ", engines "
                  << glm::length(ship.engineAccelerationMps2) << " m/s2" << std::endl;
    }
}

} // namespace game::navigation::globalflight
