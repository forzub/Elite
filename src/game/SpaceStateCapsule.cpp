// The cockpit-capsule's RESCUE autopilot on the client (author 2026-11-24, memory cockpit-capsule-rescue): the hull is
// lost, the pilot alive in the capsule (CapsuleRules.h capsuleFlying) - the capsule's computer flies it to safety by
// ITS OWN knowledge (the client: «всё, что можно - на клиенте»):
//   1. the target: the hubs it hears (within a hub's comms reach, RescueRules) or knows from the onboard journal, within
//      the capsule's reach (half the reaction mass as the cruise speed, the life support's 4 days); a hub with a МЧС
//      rescue dock first (the pad on the rescue dock's roof), else the nearest hub (its first pad). A hub whose landing
//      failed is not chosen again.
//   2. far (> 20 km): a rendezvous on the main motor - burn toward the hub, coast, turn and brake, matching the hub's
//      velocity (the global main-engine command; the server executes it within the capsule's motor: along the nose only,
//      paid from the reaction mass - GameSimulationFreeFlight).
//   3. near: the ordinary pad landing (the planner + the autopilot + the pad service), as the map card's button.
// Beacons and a known settlement in another place come next (memory onboard-computer-journal).
#include "src/game/SpaceStateInternal.h"

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/rules/CapsuleRules.h"
#include "src/game/navigation/rules/RescueRules.h"
#include "src/game/navigation/globalflight/BodyAutopilot.h"
#include "src/game/scene/LayoutCityHitVolumes.h"

#include <iostream>
#include <limits>

namespace cap = game::navigation::capsule;

namespace
{
constexpr double CapsuleEnduranceS = 4.0 * 86400.0;   // EC-2 life support (catalog endurance_days 4)
constexpr double StagingAboveHubM = 8000.0;           // the staging point above the hub
constexpr double StagingReachedM = 1000.0;            // stopped within it: the pad landing takes over
constexpr double CruiseAccelShare = 0.5;              // the profile plans with half the motor (the turn, margins)

struct PadChoice
{
    std::string frameModuleId;
    std::string address;
    bool rescueDock = false;
};

// the pad of the hub's МЧС rescue dock (its roof), else the hub's first upper pad
PadChoice padOf(const std::string& hubId)
{
    PadChoice out;
    const auto* plan = game::navigation::HubPlanCatalog::forHub(hubId);
    if (!plan)
        return out;
    for (const auto& m : plan->modules())
        if (m.pad && m.module == "hubs_rescue_dock")
            return PadChoice {plan->frameModuleId(), m.address, true};
    for (const auto& m : plan->modules())
        if (m.pad && !m.under)
            return PadChoice {plan->frameModuleId(), m.address, false};
    return out;
}
} // namespace

void SpaceState::updateCapsuleRescue(double dt)
{
    (void)dt;
    if (!m_client)
        return;
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
        return;
    const auto& ship = it->second;
    const auto& motion = ship.transform.motion;
    auto& r = m_capsuleRescue;
    if (!cap::capsuleFlying(motion.destroyedCause, motion.capsuleState))
    {
        if (r.ownsControl)
        {
            m_clientAutopilotControlActive = false;
            m_clientAutopilotControl = {};
        }
        r = CapsuleRescue {};
        return;
    }
    const double now = glfwGetTime();
    const glm::dvec3 me = world::coordinates::fullMeters(ship.transform.worldPosition);
    const glm::dvec3 myV = motion.worldVelocityMps;

    // ---- 1. the target (re-chosen when none, or when its landing ended without the capsule landed)
    if (r.hubId.empty() && now >= r.nextChoiceAt)
    {
        r.nextChoiceAt = now + 5.0;
        const double reach = 0.5 * motion.capsuleDeltaVLeftMps * CapsuleEnduranceS;
        double best = std::numeric_limits<double>::infinity();
        bool bestRescue = false;
        for (const auto& [hubId, hub] : m_client->world().hubs())
        {
            if (hub.systemId != motion.systemId || r.refused.count(hubId))
                continue;
            const double d = glm::length(world::coordinates::fullMeters(hub.worldPosition) - me);
            // the capsule's own knowledge: heard now (its comms reach) or in the onboard journal
            bool known = d <= game::navigation::rescue::ZoneRadiusM;
            for (const auto& e : m_onboardJournal)
                known = known || (e.kind == game::navigation::journal::Kind::Hub && e.key == hubId);
            if (!known || d > reach)
                continue;
            const auto pad = padOf(hubId);
            if (pad.address.empty())
                continue;
            // a rescue dock wins over a nearer hub without one (unless it is twice as far)
            const double score = pad.rescueDock ? d : d * 2.0;
            if (score < best || (pad.rescueDock && !bestRescue && d < 2.0 * best))
            {
                best = score;
                bestRescue = pad.rescueDock;
                r.hubId = hubId;
                r.pad = pad.address;
                r.frameModuleId = pad.frameModuleId;
                r.rescueDock = pad.rescueDock;
            }
        }
        if (!r.hubId.empty())
        {
            const auto& hub = m_client->world().hubs().at(r.hubId);
            r.hubName = hub.name.empty() ? r.hubId : hub.name;
            r.phase = CapsuleRescue::Phase::Cruise;
            std::cerr << "[CapsuleRescue] target " << r.hubName << " pad " << r.pad << (r.rescueDock ? " (МЧС rescue dock)" : "")
                      << ", " << glm::length(world::coordinates::fullMeters(hub.worldPosition) - me) / 1000.0
                      << " km, reaction mass " << motion.capsuleDeltaVLeftMps << " m/s" << std::endl;
        }
        else if (!r.reportedNoTarget)
        {
            r.reportedNoTarget = true;
            std::cerr << "[CapsuleRescue] no known hub within reach (reaction mass " << motion.capsuleDeltaVLeftMps
                      << " m/s): drifting, SOS" << std::endl;
        }
    }
    if (r.hubId.empty())
    {
        m_pilotNotice = "КАПСУЛА: известных хабов в досягаемости нет - дрейф, сигнал SOS";
        return;
    }
    const auto hubIt = m_client->world().hubs().find(r.hubId);
    if (hubIt == m_client->world().hubs().end())
    {
        r.hubId.clear();
        return;
    }
    const glm::dvec3 hubPos = world::coordinates::fullMeters(hubIt->second.worldPosition);
    const glm::dvec3 hubV = hubIt->second.worldVelocityMps;
    // the STAGING point: 8 km above the hub (away from its planet) - where every approach to a city pad starts; from a
    // point in the hub's shadow (2.5 km off the station, the arrays between) the planner found no path at all (live:
    // every candidate through an obstacle, violation 1.0)
    glm::dvec3 up(0.0, 1.0, 0.0);
    if (const auto* celestial = m_client->celestialSnapshot(); celestial && celestial->systemId == hubIt->second.systemId)
        for (const auto& body : celestial->bodies)
            if (body.id == hubIt->second.parentBodyId && glm::length(hubPos - body.worldMeters) > 1.0)
                up = glm::normalize(hubPos - body.worldMeters);
    const glm::dvec3 staging = hubPos + up * StagingAboveHubM;
    // the staging point turns with the orbit (8 km x the orbit's rate ~ 9 m/s against the hub): its own velocity, from
    // its motion between the samples (the hub's until there is one)
    const double simNow = m_client->lastSimulationMetadata().universeTimeSeconds;
    glm::dvec3 stagingV = hubV;
    if (r.stagingValid && simNow - r.stagingAt > 0.05 && simNow - r.stagingAt < 5.0)
        stagingV = (staging - r.stagingPrev) / (simNow - r.stagingAt);
    else if (r.stagingValid && simNow - r.stagingAt <= 0.05)
        stagingV = r.stagingVelocity;
    if (!r.stagingValid || simNow - r.stagingAt > 0.05)
    {
        r.stagingPrev = staging;
        r.stagingAt = simNow;
        r.stagingVelocity = stagingV;
        r.stagingValid = true;
    }
    const glm::dvec3 rel = staging - me;
    const double d = glm::length(rel);
    const glm::dvec3 relV = myV - stagingV;

    // ---- 3. near: the ordinary pad landing (once); its failure -> the next hub
    if (r.phase == CapsuleRescue::Phase::Landing)
    {
        const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
        const bool landingAlive = pending.valid() || !m_padLandingObjectId.empty();
        // the planner needs its time (the city's geometry, the route): the landing counts as ended only when nothing
        // is pending any more AND a minute and a half passed
        if (!landingAlive && now - r.landingAskedAt > 90.0)
        {
            std::cerr << "[CapsuleRescue] the landing at " << r.hubName << " ended without the capsule down - next hub" << std::endl;
            r.refused.insert(r.hubId);
            r.hubId.clear();
            r.phase = CapsuleRescue::Phase::None;
            r.nextChoiceAt = now;
        }
        m_pilotNotice = "КАПСУЛА: посадка на площадку " + r.pad + " - " + r.hubName + (r.rescueDock ? " (док МЧС)" : "");
        return;
    }
    // the nearest structure (the capsule's own clearance - the global main-engine command does not go through the
    // evasion reflex): a capsule is born where its ship broke up - by a station; the straight line to the target ran
    // through it (live: an 83 m/s hit on the habitat ring right after the take-over)
    double nearest = 1.0e30;
    glm::dvec3 away(0.0), structV(0.0);
    {
        for (const auto& [oid, object] : m_client->world().objects())
        {
            if (object.systemId != motion.systemId)
                continue;
            const glm::dvec3 origin = world::coordinates::fullMeters(object.worldPosition);
            if (glm::length(origin - me) > 25000.0)
                continue;
            const auto* volumes = &object.debugHitVolumes;
            if (volumes->empty() && game::scene::layoutCityHitVolumesOnClient(object.type, object.hubAttachment.hubId))
                volumes = game::scene::clientLayoutCityHitVolumes(object.type, object.hubAttachment.hubId);
            if (!volumes)
                continue;
            const glm::dmat3 R(glm::mat3(object.orientation));
            for (const auto& v : *volumes)
            {
                if (v.destroyed)
                    continue;
                const glm::dvec3 c = origin + R * glm::dvec3(v.center);
                const double surface = glm::length(c - me) - glm::length(glm::dvec3(v.halfSize));
                if (surface < nearest)
                {
                    nearest = surface;
                    away = glm::length(me - c) > 1.0e-6 ? glm::normalize(me - c) : glm::dvec3(0.0, 1.0, 0.0);
                    structV = object.linearVelocityMps + glm::cross(object.angularVelocityWorldRadPerSecond, c - origin);
                }
            }
        }
    }

    // ---- 3. near: stopped against the hub, clear of its structures -> the ordinary pad landing (once)
    if (d < StagingReachedM && glm::length(relV) < 5.0 && nearest > 500.0)
    {
        if (r.ownsControl)
        {
            r.ownsControl = false;
            m_clientAutopilotControlActive = false;
            m_clientAutopilotControl = {};
        }
        m_systemMapRenderer.requestPadLanding(r.frameModuleId, r.pad);
        r.phase = CapsuleRescue::Phase::Landing;
        r.landingAskedAt = now;
        std::cerr << "[CapsuleRescue] " << d / 1000.0 << " km from " << r.hubName << ", " << glm::length(relV)
                  << " m/s: the pad landing on " << r.pad << std::endl;
        return;
    }

    // ---- 2. far: the rendezvous on the main motor
    const auto params = cap::capsuleParams(motion.capsuleDeltaVLeftMps);
    const double aPlan = CruiseAccelShare * params.forwardMainEngineAccelerationMps2;
    const glm::dvec3 dir = d > 1.0 ? rel / d : glm::dvec3(0.0);
    // the closing speed that still stops at the near boundary, capped by what the reaction mass can pay (half to speed
    // up, half to stop) - the coast is between
    const double toGo = std::max(0.0, d - 0.5 * StagingReachedM);
    const double cruiseCap = std::max(5.0, 0.4 * motion.capsuleDeltaVLeftMps);
    // the economy profile (the reaction mass is all it has): no faster than the distance over 10 minutes
    const double vClose = std::min({cruiseCap, std::sqrt(2.0 * std::max(0.1, aPlan) * toGo), std::max(10.0, d / 600.0)});
    glm::dvec3 wantRelV = dir * vClose;   // to the staging point, stopping there
    if (nearest < 1000.0)
    {
        wantRelV = away * 15.0 + (structV - stagingV);   // within 1 km of a structure: back away from it first
        if (now - r.lastLogAt > 10.0)
            std::cerr << "[CapsuleRescue] clearing a structure " << nearest << " m away first" << std::endl;
    }
    const glm::dvec3 dv = wantRelV - relV;
    // burn only for a real difference (a coast otherwise); the motor pushes along the nose: aim it first
    ShipControlState c {};
    c.globalFlightCommandValid = true;
    // the rendezvous flies in normal flight (the global main-engine command is executed there): leave a hub frame
    if (!motion.freeFlight)
        c.globalFrameRequest = static_cast<std::uint8_t>(game::navigation::globalflight::FrameRequest::NormalFlight);
    const double dvLen = glm::length(dv);
    if (dvLen > 0.3 && dvLen <= 20.0 && params.forwardMainEngineAvailable)
    {
        // a small correction: the jets, in any direction, no turn of the hull (the turn and a main-motor pulse cost more)
        c.globalMainAccelWorldMps2 = dv / dvLen * std::min<double>(params.manoeuvreThrusterAccel, dvLen / 2.0);
    }
    else if (dvLen > 20.0 && params.forwardMainEngineAvailable)
    {
        const glm::dvec3 burnDir = dv / dvLen;
        c.globalAttitudeTargetValid = true;
        c.globalAttitudeForward = burnDir;
        c.globalAttitudeUp = std::abs(burnDir.y) < 0.9 ? glm::dvec3(0.0, 1.0, 0.0) : glm::dvec3(1.0, 0.0, 0.0);
        const double aligned = glm::dot(glm::normalize(glm::dvec3(ship.transform.forward())), burnDir);
        if (aligned > 0.98)
            c.globalMainAccelWorldMps2 = burnDir * std::min<double>(params.forwardMainEngineAccelerationMps2, dvLen / 2.0);
    }
    r.ownsControl = true;
    m_clientAutopilotControlActive = true;
    m_clientAutopilotControl = c;
    // through the evasion reflex like every virtual pilot (it sees the structures: no cruise through a station)
    submitVirtualPilotControl(c, 0x43u);
    if (now - r.lastLogAt > 10.0)
    {
        r.lastLogAt = now;
        std::cerr << "[CapsuleRescue] cruise to " << r.hubName << ": " << d / 1000.0 << " km, closing "
                  << glm::dot(relV, dir) << " m/s (want " << vClose << "), reaction mass " << motion.capsuleDeltaVLeftMps
                  << " m/s" << std::endl;
    }
    char b[256];
    std::snprintf(b, sizeof b, "КАПСУЛА: курс на «%s»%s - %.0f км, сближение %.0f м/с, запас %.0f м/с", r.hubName.c_str(),
                  r.rescueDock ? " (док МЧС)" : "", d / 1000.0, glm::dot(relV, dir), motion.capsuleDeltaVLeftMps);
    m_pilotNotice = b;
}
