// The route-leg pilot (see RouteLegPilot.h). Moved from the server's GameSimulation::updateFreeFlightLeg
// and its take-off (2026-11-06): the same laws and logs; the velocity changes are a command for the
// plant (a J velocity target with its limit, a main-engine acceleration, an attitude target).
#include "src/game/navigation/globalflight/RouteLegPilot.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/JDrive.h"

namespace game::navigation::globalflight
{
namespace
{
namespace jd = game::navigation::jdrive;
constexpr double HubArrivalM = jd::KeepOutObjectM + 5000.0;   // the aim just outside the keep-out, on the approach side
constexpr double ShipArrivalM = jd::KeepOutShipM + 1000.0;    // a rendezvous: just outside the target ship's J keep-out
using Kind = route::RoutePoint::Kind;

const GravityBody* findBody(const BodyAutopilotEnvironment& env, const std::string& id)
{
    if (env.bodies)
        for (const auto& b : *env.bodies)
            if (b.id == id)
                return &b;
    return nullptr;
}
} // namespace

void RouteLegPilot::start(const route::RoutePoint& leg, double remainingAfterM, bool alreadyInJ, bool onGround,
                          const std::string& departureHubId, std::uint32_t shipId)
{
    *this = RouteLegPilot {};
    m_leg = leg;
    m_remainingAfterM = remainingAfterM;
    m_active = true;
    m_takeOff = onGround;
    m_shipId = shipId;
    // continuing in J (past a guide point): the drive stays at full strength
    m_driveStartSeconds = alreadyInJ ? -1000.0 : -1.0;
    m_toldTargetInHub = false;
    m_departureHubId = departureHubId;
    m_assistedAligned = alreadyInJ;   // past a guide point the hull is already on the motion
    if (m_takeOff)
        std::cerr << "[Route] ship " << shipId << ": TAKE-OFF - main-engine climb to the J floor first" << std::endl;
    std::cerr << "[Route] ship " << shipId << ": leg -> "
              << (leg.kind == Kind::Hub     ? "hub " + leg.hubId
                  : leg.kind == Kind::Orbit ? "orbit of " + leg.bodyId + " at " + std::to_string(static_cast<int>(leg.orbitAltitudeM / 1000.0)) + " km"
                  : leg.kind == Kind::Ship  ? "ship " + std::to_string(leg.shipEntityId)
                                            : std::string("point"))
              << " (departure " << m_departureHubId << ")" << std::endl;
}

BodyAutopilotOutput RouteLegPilot::stepTakeOff(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, double dt)
{
    BodyAutopilotOutput out;
    out.refusal = ship.refusal;
    out.control.globalFlightCommandValid = true;
    out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::NormalFlight);
    const GravityBody* primary = findBody(env, ship.primaryBodyId);
    if (!primary)
    {
        m_takeOff = false;
        return out;
    }
    const double floorAlt = jd::floorAltitudeM(primary->radiusMeters, primary->atmosphereRadiusMeters);
    const glm::dvec3 up = glm::normalize(ship.positionM - primary->centerMeters);
    const double alt = glm::length(ship.positionM - primary->centerMeters) - primary->radiusMeters;
    const bool haveLast = m_lastPrimaryId == primary->id;
    const glm::dvec3 bodyVel = haveLast ? (primary->centerMeters - m_lastPrimaryCentre) / dt : ship.velocityMps;
    m_lastPrimaryId = primary->id;
    m_lastPrimaryCentre = primary->centerMeters;
    if (alt >= floorAlt)
    {
        m_takeOff = false;
        std::cerr << "[Route] ship " << m_shipId << ": TAKE-OFF complete at " << alt / 1000.0 << " km (J floor " << floorAlt / 1000.0
                  << " km) - the leg continues" << std::endl;
        return out;
    }
    if (!haveLast || ship.surfaceState != 0 && m_seconds < 0.5)
        return out;   // the body's velocity needs two samples; the plant lifts the ship off first
    const double aboveGround = std::max(0.0, alt - ship.groundHeightM);
    const double climb = std::min(jd::FloorClimbSpeedMps, 5.0 + 0.5 * aboveGround);
    const glm::dvec3 dv = bodyVel + up * climb - ship.velocityMps;
    const glm::dvec3 gravity = env.bodies ? GravityFieldSystem::sample(ship.positionM, *env.bodies).accelerationMps2 : glm::dvec3(0.0);
    glm::dvec3 a = dv / 1.0 - gravity;
    const double aMax = 0.9 * mainEngineAccelMps2(ship.physics);
    if (glm::length(a) > aMax)
        a *= aMax / glm::length(a);
    out.control.globalMainAccelWorldMps2 = a;
    if (std::fmod(m_seconds, 2.0) < dt)
        std::cerr << "[Route] ship " << m_shipId << " take-off: " << aboveGround << " m above the ground, climb "
                  << glm::dot(ship.velocityMps - bodyVel, up) << " m/s (wanted " << climb << "), " << glm::length(a) / 9.81 << " g"
                  << std::endl;
    return out;
}

BodyAutopilotOutput RouteLegPilot::step(const BodyAutopilotEnvironment& env, const RouteLegResolver& resolve, const BodyAutopilotShip& ship,
                                        double dt)
{
    BodyAutopilotOutput out;
    out.refusal = ship.refusal;
    if (!m_active || dt <= 0.0)
        return out;
    m_seconds += dt;
    if (m_takeOff)
        return stepTakeOff(env, ship, dt);
    out.control.globalFlightCommandValid = true;
    out.control.globalFrameRequest = static_cast<std::uint8_t>(FrameRequest::NormalFlight);
    const route::RoutePoint& leg = m_leg;
    const glm::dvec3 position = ship.positionM;
    const glm::dvec3 velocity = ship.velocityMps;

    // ---- destination kinematics: position to reach, velocity to match there
    glm::dvec3 targetPos(0.0), targetVel(0.0);
    // the velocity reference the pilot flies against - the one the HUD shows and the Assisted nose
    // follows (author 2026-11-08: "the main velocity vector never matched the thrust vector"):
    // a hub for hub legs (the stabilization's hub reference kept), else the dominant body
    glm::dvec3 refVel = ship.referenceVelocityMps;
    std::string refHubId;
    const GravityBody* body = findBody(env, leg.bodyId);
    if (leg.kind == Kind::Hub)
    {
        glm::dvec3 hubPos, hubVel;
        if (!resolve.hub || !resolve.hub(leg.hubId, hubPos, hubVel))
        {
            std::cerr << "[Route] ship " << m_shipId << ": hub " << leg.hubId << " unknown - leg dropped" << std::endl;
            m_active = false;
            return out;
        }
        // the arrival point inside the J boundary, on the side the ship comes from
        const glm::dvec3 fromHub = position - hubPos;
        const glm::dvec3 side = glm::length(fromHub) > 1.0 ? glm::normalize(fromHub) : glm::dvec3(0, 1, 0);
        targetPos = hubPos + side * HubArrivalM;
        targetVel = hubVel;
        refVel = hubVel;
        refHubId = leg.hubId;
    }
    else if (leg.kind == Kind::Ship)
    {
        // a rendezvous (ROUTE_PROGRAMS_DESIGN.md §4): the target ship's position now, its velocity
        // matched, the arrival just outside its J keep-out on the approach side
        glm::dvec3 otherPos, otherVel;
        if (!resolve.ship || !resolve.ship(leg.shipEntityId, otherPos, otherVel))
        {
            std::cerr << "[Route] ship " << m_shipId << ": rendezvous target " << leg.shipEntityId << " unknown - leg dropped" << std::endl;
            m_active = false;
            return out;
        }
        const glm::dvec3 fromOther = position - otherPos;
        const glm::dvec3 side = glm::length(fromOther) > 1.0 ? glm::normalize(fromOther) : glm::dvec3(0, 1, 0);
        targetPos = otherPos + side * ShipArrivalM;
        targetVel = otherVel;
        refVel = otherVel;
        // a target inside a hub's keep-out (dock traffic): no J field there to brake in - the leg ends
        // matched at that hub's keep-out edge, the rest is a local approach (live q6 2026-11-07: aimed at
        // the ship itself, it entered the hub's keep-out at 1.4 km/s on the gentle 1 g and oscillated)
        std::string hubId;
        glm::dvec3 hubPos, hubVel;
        if (resolve.hubAround && resolve.hubAround(otherPos, hubId, hubPos, hubVel))
        {
            const glm::dvec3 fromHub = position - hubPos;
            const glm::dvec3 hubSide = glm::length(fromHub) > 1.0 ? glm::normalize(fromHub) : side;
            targetPos = hubPos + hubSide * HubArrivalM;
            targetVel = hubVel;
            refVel = hubVel;
            refHubId = hubId;
            if (!m_toldTargetInHub)
            {
                m_toldTargetInHub = true;
                std::cerr << "[Route] ship " << m_shipId << ": rendezvous target " << leg.shipEntityId << " is inside the keep-out of " << hubId
                          << " - the leg ends at its edge (the rest is a local approach)" << std::endl;
            }
        }
    }
    else if (leg.kind == Kind::Orbit)
    {
        if (!body)
        {
            m_active = false;
            return out;
        }
        const glm::dvec3 bodyVel = m_haveLastBody ? (body->centerMeters - m_lastBodyCentre) / dt : glm::dvec3(0.0);
        m_lastBodyCentre = body->centerMeters;
        if (!m_haveLastBody)
        {
            m_haveLastBody = true;
            out.control.globalFlightCommandValid = true;
            return out;
        }
        const glm::dvec3 rel = position - body->centerMeters;
        const double D = std::max(1.0, glm::length(rel));
        const glm::dvec3 up = glm::length(rel) > 1.0 ? glm::normalize(rel) : glm::dvec3(0, 1, 0);
        // J ends above the body's J floor: a lower orbit is raised to it
        const double r = body->radiusMeters + std::max(leg.orbitAltitudeM, jd::floorAltitudeM(body->radiusMeters, body->atmosphereRadiusMeters));
        // the orbit plane, fixed once: the ship's position and its motion across the radius (else the
        // ecliptic-like plane through the body) - the approach and the orbit share it
        if (glm::length(m_orbitNormal) < 0.5)
        {
            const glm::dvec3 w0 = velocity - bodyVel;
            glm::dvec3 n = glm::cross(rel, w0);
            if (glm::length(n) < 1.0e-3 * D * std::max(1.0, glm::length(w0)))
                n = glm::cross(rel, glm::cross(glm::dvec3(0, 1, 0), rel));
            if (glm::length(n) < 1.0e-9)
                n = glm::cross(rel, glm::dvec3(1, 0, 0));
            m_orbitNormal = glm::normalize(n);
        }
        const glm::dvec3 n = m_orbitNormal;
        glm::dvec3 u = up - n * glm::dot(up, n);
        u = glm::length(u) > 1.0e-9 ? glm::normalize(u) : glm::normalize(glm::cross(n, glm::dvec3(1, 0, 0)));
        const glm::dvec3 t = glm::normalize(glm::cross(n, u));
        // the TANGENT point of the orbit circle seen from the ship (author 2026-11-06: not into the
        // centre and a magic 90 deg turn - along the tangent, matching the circular speed)
        glm::dvec3 aim, tangent;
        if (D > r * 1.02)
        {
            const double th = std::acos(std::clamp(r / D, -1.0, 1.0));
            const glm::dvec3 radial = u * std::cos(th) + t * std::sin(th);
            aim = body->centerMeters + radial * r;
            tangent = glm::normalize(glm::cross(n, radial));
        }
        else
        {
            aim = body->centerMeters + u * r;
            tangent = t;
        }
        targetPos = aim;
        targetVel = bodyVel + tangent * std::sqrt(body->gravitationalParameterM3s2 / r);
        refVel = bodyVel;
    }
    else
    {
        targetPos = leg.worldM;
        targetVel = ship.referenceVelocityMps;
        glm::dvec3 hubPos, hubVel;
        if (!m_routeHubId.empty() && resolve.hub && resolve.hub(m_routeHubId, hubPos, hubVel))
        {
            targetVel = hubVel;
            refHubId = m_routeHubId;
        }
        refVel = targetVel;
    }

    const glm::dvec3 toAim = targetPos - position;
    const double d = glm::length(toAim);
    glm::dvec3 dir = d > 1.0e-3 ? toAim / d : glm::dvec3(0.0);
    const glm::dvec3 w = velocity - targetVel;

    // ---- the drive: gentle inside any object's keep-out (no J field there), then 1 g doubling every 2 s
    KeepOutHit keep;
    const bool inKeepOut = env.keepOut && env.keepOut(position, keep);
    const bool nearDeparture = inKeepOut &&
                               !(leg.kind == Kind::Hub && keep.objectId == leg.hubId && m_seconds > 1.0 && keep.objectId != m_departureHubId) &&
                               !(leg.kind == Kind::Ship && m_seconds > 1.0 &&
                                 keep.objectId.rfind("ship " + std::to_string(leg.shipEntityId) + " ", 0) == 0);
    if (!nearDeparture && m_driveStartSeconds == -1.0)
        m_driveStartSeconds = m_seconds;
    double driveSeconds = m_driveStartSeconds == -1.0 ? 0.0 : m_seconds - m_driveStartSeconds;
    double limit = nearDeparture ? jd::GentleAccelMps2 : std::min(jd::MaxAccelMps2, 9.81 * std::exp2(driveSeconds / jd::DriveRampDoublingS));
    // the plant's field restarts its ramp under a J floor / in a keep-out: the pilot follows its real
    // level (live 2026-11-08: the client braked on 1e5 g while the plant had 2e2 g - 8000 km overshoot)
    // (held 0.5 s: the replicated level lags a jump of the wanted velocity by a tick or two)
    const bool plantBehind = !nearDeparture && ship.jAccelKnown && driveSeconds > 1.0 && glm::length(m_lastWantedDv) > 2.0 * limit * dt &&
                             ship.jAccelMps2 < 0.25 * limit;
    m_plantBehindSeconds = plantBehind ? m_plantBehindSeconds + dt : 0.0;
    if (m_plantBehindSeconds > 0.5)
    {
        m_plantBehindSeconds = 0.0;
        const double level = std::max(9.81, ship.jAccelMps2);
        m_driveStartSeconds = m_seconds - jd::DriveRampDoublingS * std::log2(level / 9.81);
        driveSeconds = m_seconds - m_driveStartSeconds;
        limit = std::min(jd::MaxAccelMps2, level);
        if (!m_rampSyncLogged)
            std::cerr << "[Route] ship " << m_shipId << ": the plant J field is at " << level << " m/s2 (pilot expected more) - ramp follows the plant"
                      << std::endl;
        m_rampSyncLogged = true;
    }

    // ---- obstacles: never through a planet / moon / the star - go round the first one in the way
    int phase = 2;
    double starMu = 0.0;
    if (env.bodies)
    {
        for (const auto& b : *env.bodies)
            starMu = std::max(starMu, b.gravitationalParameterM3s2);
        for (const auto& b : *env.bodies)
        {
            if (b.radiusMeters < 50000.0 || d < 1.0)
                continue;
            // the body, its atmosphere and a margin; a destination below that (a hub in low orbit)
            // still keeps the path from dipping under the destination's own altitude. The star: 20 of
            // its radii (the hull's heat, not its surface)
            const bool star = b.gravitationalParameterM3s2 >= starMu && starMu > 0.0;
            double clear = game::navigation::rules::clearanceM(b.radiusMeters, b.atmosphereRadiusMeters, star);
            clear = std::min(clear, glm::length(targetPos - b.centerMeters) * 0.995);
            clear = std::min(clear, glm::length(position - b.centerMeters) * 0.995);
            const glm::dvec3 s = position - b.centerMeters;
            const double t = std::clamp(-glm::dot(s, toAim) / (d * d), 0.0, 1.0);
            if (t > 0.0 && t < 1.0 && glm::length(s + toAim * t) < clear)
            {
                glm::dvec3 sideStep = s - dir * glm::dot(s, dir);
                if (glm::length(sideStep) < 1.0)
                    sideStep = glm::cross(dir, std::abs(dir.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
                dir = glm::normalize(glm::normalize(sideStep) + dir * 0.3);
                phase = 1;
                break;
            }
        }
    }

    // a pass-through guide point: brake for the rest of the route, not for the point
    const bool passThrough = leg.kind == Kind::Point && !leg.stop;
    const double dEff = d + (passThrough ? m_remainingAfterM : 0.0);
    // the braking speed: the field still has to ramp up to full strength (rampLeft s, ~coasting),
    // then brakes at 0.4 of it: k v^2 + rampLeft v = dEff
    const double rampLeft = nearDeparture ? 0.0 : jd::DriveRampDoublingS * std::log2(std::max(1.0, jd::MaxAccelMps2 / std::max(9.81, limit)));
    const double kBrake = 1.0 / (2.0 * 0.4 * jd::MaxAccelMps2);
    const double braking = (-rampLeft + std::sqrt(rampLeft * rampLeft + 4.0 * kBrake * dEff)) / (2.0 * kBrake);
    const double tail = dEff / jd::ApproachTauS;
    double speed = std::min({braking, tail, jd::SpeedCapMps});
    if (nearDeparture)
        speed = std::min(speed, jd::GentleSpeedMps);
    if (phase == 2 && tail < braking)
        phase = 3;
    glm::dvec3 targetWorld = targetVel + dir * speed;
    if (nearDeparture)
    {
        // inside an object's keep-out: a slow climb out AGAINST THAT OBJECT (its traffic), not against
        // the destination - the destination's speed is matched only after the keep-out
        const glm::dvec3 away = glm::normalize(position - keep.positionM + dir * 1.0);
        targetWorld = keep.velocityMps + away * jd::GentleSpeedMps;
    }
    // ---- Assisted (author 2026-11-08: "Assisted always turns along the main velocity vector; leaving
    // the stabilization the ship lines its thrust up with the orbital velocity and flies to the other
    // hub from that attitude"): the main velocity is the one against the dominant body. The ship
    // first turns onto it (the velocity held), then the velocity is kept within AssistLeadRad of the
    // hull while the hull turns onto the wanted velocity - the motion follows the nose, never sideways
    const bool assisted = ship.currentLaw == LocalFlightControlLaw::Assisted;
    m_assistNoseValid = false;
    glm::dvec3 mainVel = ship.referenceVelocityMps;
    bool haveMain = false;
    if (const GravityBody* mb = findBody(env, ship.primaryBodyId))
    {
        if (m_mainBodyId == mb->id)
        {
            mainVel = (mb->centerMeters - m_mainBodyLastCentre) / dt;
            haveMain = true;
        }
        m_mainBodyId = mb->id;
        m_mainBodyLastCentre = mb->centerMeters;
    }
    if (assisted && haveMain)
    {
        constexpr double AssistLeadRad = 2.0 * 3.14159265358979 / 180.0;
        const glm::dvec3 mainRel = velocity - mainVel;
        const glm::dvec3 wantRel = targetWorld - mainVel;
        const double wantSpeed = glm::length(wantRel);
        const glm::dvec3 fwd = glm::normalize(ship.forward);
        const double offHull = glm::length(mainRel) > 30.0 ? std::acos(std::clamp(glm::dot(glm::normalize(mainRel), fwd), -1.0, 1.0)) : 0.0;
        if (!m_assistedAligned && offHull < 3.0 * AssistLeadRad / 2.0)
            m_assistedAligned = true;
        else if (m_assistedAligned && offHull > 15.0 * AssistLeadRad / 2.0)
            m_assistedAligned = false;   // lost (a new dominant body): line up again
        if (wantSpeed > 30.0)
        {
            if (!m_assistedAligned)
            {
                targetWorld = velocity;   // hold the motion while the hull turns onto it
                m_assistNose = glm::length(mainRel) > 1.0 ? glm::normalize(mainRel) : fwd;
            }
            else
            {
                // the nose leads the real main velocity by at most AssistLeadRad towards the wanted one
                // (a weak J field at the start turns the motion slowly - the hull waits for it), and the
                // commanded velocity stays within AssistLeadRad of the hull (a strong field turns the
                // motion as fast as the hull turns)
                const glm::dvec3 wantDir = wantRel / wantSpeed;
                const glm::dvec3 relDir = glm::length(mainRel) > 1.0 ? glm::normalize(mainRel) : fwd;
                const auto toward = [](const glm::dvec3& from, const glm::dvec3& to, double maxRad) {
                    const double a = std::acos(std::clamp(glm::dot(from, to), -1.0, 1.0));
                    if (a <= maxRad)
                        return to;
                    glm::dvec3 axis = to - from * glm::dot(to, from);
                    if (glm::length(axis) < 1.0e-9)
                        return from;
                    axis = glm::normalize(axis);
                    return glm::normalize(from * std::cos(maxRad) + axis * std::sin(maxRad));
                };
                const glm::dvec3 cmdDir = toward(fwd, toward(relDir, wantDir, AssistLeadRad), AssistLeadRad);
                // the speed while the hull is still off the wanted direction: none beyond 45 deg, full
                // when on it (live as7: kept along a slowly turning hull at speed, the ship circled the
                // target at 300 km - the turning radius v / omega); a motion nearly stopped leaves the
                // hull free to come round onto the wanted direction, then it speeds up along the hull
                const double offWant = std::acos(std::clamp(glm::dot(fwd, wantDir), -1.0, 1.0));
                m_assistWantOffDeg = offWant * 57.2957795;
                const double cos45 = 0.70710678;
                const double useful = std::clamp((std::cos(offWant) - cos45) / (1.0 - cos45), 0.0, 1.0);
                targetWorld = mainVel + cmdDir * (wantSpeed * useful);
                const bool stopped = glm::length(mainRel) < 50.0;
                m_assistNose = stopped ? wantDir : toward(relDir, wantDir, AssistLeadRad);
            }
            m_assistNoseValid = true;
        }
        refVel = mainVel;
        refHubId.clear();   // the HUD's reference: the dominant body
        if (m_seconds - m_assistLogSeconds >= 2.0)
        {
            m_assistLogSeconds = m_seconds;
            std::cerr << "[RouteAssist] ship " << m_shipId << " t+" << m_seconds << (m_assistedAligned ? " aligned" : " LINING UP")
                      << ": main velocity " << glm::length(mainRel) << " m/s vs " << m_mainBodyId << ", hull off it "
                      << offHull * 57.2957795 << " deg, wanted velocity " << wantSpeed << " m/s " << m_assistWantOffDeg << " deg off the hull" << std::endl;
        }
    }
    glm::dvec3 dv = targetWorld - velocity;
    m_lastWantedDv = dv;
    if (nearDeparture && !keep.ship && !(assisted && haveMain))
    {
        refVel = keep.velocityMps;   // climbing out against the hub just left (its frame on the HUD)
        refHubId = keep.objectId;
    }
    const double maxDv = limit * dt;
    if (glm::length(dv) > maxDv)
        dv *= maxDv / glm::length(dv);
    out.control.globalJVelocityTargetValid = true;
    out.control.globalJVelocityTargetWorldMps = targetWorld;
    out.control.globalJAccelLimitMps2 = limit;
    const glm::dvec3 accel = dv / dt;

    out.control.globalReferenceHubId = refHubId;

    // ---- attitude: the Assisted rule - the nose along the wanted velocity against the reference
    // (the J field bends the real one onto it, the thrust stays along the nose); at the very end
    // along the remaining motion
    {
        const glm::dvec3 wanted = targetWorld - refVel;
        glm::dvec3 nose = m_assistNoseValid             ? m_assistNose
                        : glm::length(wanted) > 1.0 ? glm::normalize(wanted)
                        : d > 50.0                  ? dir
                        : (glm::length(w + dv) > 1.0 ? glm::normalize(w + dv) : ship.forward);
        glm::dvec3 upW = ship.up - nose * glm::dot(ship.up, nose);
        if (glm::length(upW) < 1.0e-3)
            upW = glm::cross(nose, std::abs(nose.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
        out.control.globalAttitudeTargetValid = true;
        out.control.globalAttitudeForward = nose;
        out.control.globalAttitudeUp = glm::normalize(upW);
    }

    // ---- HUD (the J autopilot panel)
    const glm::dvec3 wAfter = w + dv;
    out.displayPhase = nearDeparture ? code(AutopilotDisplayPhase::HubExit) : static_cast<std::uint8_t>(phase);
    out.displayTargetId = leg.kind == Kind::Hub ? leg.hubId : leg.bodyId;
    out.displayRangeM = d;
    out.displayRelSpeedMps = glm::length(wAfter);
    out.displayAccelMps2 = static_cast<float>(glm::length(wAfter) > 1.0e-3 ? glm::dot(accel, glm::normalize(wAfter)) : glm::length(accel));
    if (m_seconds - m_lastLog >= 2.0)
    {
        m_lastLog = m_seconds;
        std::cerr << "[Route] ship " << m_shipId << " t+" << m_seconds << " s phase " << int(out.displayPhase) << ": to aim " << d / 1000.0
                  << " km, speed vs destination " << glm::length(wAfter) << " m/s, accel " << glm::length(accel) << " m/s2" << std::endl;
    }

    // ---- a guide point passed: the route goes on in J (no stop, no matching); passed within 1 % of
    // the rest of the route, 20 km .. 2000 km
    if (passThrough && d < std::clamp(0.01 * m_remainingAfterM, 20000.0, 2.0e6))
    {
        m_active = false;
        m_passed = true;
        std::cerr << "[Route] ship " << m_shipId << ": passed a guide point at " << d / 1000.0 << " km, " << glm::length(wAfter) / 1000.0
                  << " km/s" << std::endl;
        return out;
    }
    // ---- finish: matched to the destination -> the stabilization (an orbit precisely: 3 m/s)
    const bool there = leg.kind == Kind::Orbit ? d < 500.0 && glm::length(wAfter) < 3.0 : d < 1500.0 && glm::length(wAfter) < 20.0;
    if (there)
    {
        m_active = false;
        m_arrived = true;
        std::cerr << "[Route] ship " << m_shipId << ": ARRIVED after " << m_seconds << " s, " << d << " m from the aim, "
                  << glm::length(wAfter) << " m/s vs destination - leaving J" << std::endl;
    }
    return out;
}

} // namespace game::navigation::globalflight
