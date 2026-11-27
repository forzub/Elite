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
#include "src/game/localization/LocalizationService.h"

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/rules/CapsuleRules.h"
#include "src/game/navigation/rules/CapsuleBeacons.h"
#include "src/game/navigation/rules/RescueRules.h"
#include "src/game/navigation/globalflight/BodyAutopilot.h"
#include "src/game/scene/LayoutCityHitVolumes.h"

#include <iostream>
#include <limits>

namespace l10n = game::localization;

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

    // the capsule beacons it HEARS now (CapsuleBeacons.h): each tells its position and its rescue service (a hub)
    struct Heard { const game::navigation::beacons::CapsuleBeacon* beacon; glm::dvec3 worldM; };
    std::vector<Heard> heard;
    {
        const auto resolve = routeAnchorResolver();
        for (const auto& b : game::navigation::beacons::capsuleBeacons())
        {
            glm::dvec3 p(0.0);
            if (b.systemId == motion.systemId && resolve(b.anchor, p) && glm::length(p - me) <= b.hearRadiusM)
                heard.push_back({&b, p});
        }
    }

    // ---- 1. the target (re-chosen when none, or when its landing ended without the capsule landed)
    if (r.hubId.empty() && r.beaconId.empty() && now >= r.nextChoiceAt)
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
            for (const auto& h : heard)   // a beacon heard names its rescue service
                known = known || h.beacon->rescueHubId == hubId;
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
        else
        {
            // no hub within reach: the nearest beacon heard - fly to it and wait there for the pick-up
            double bestBeacon = std::numeric_limits<double>::infinity();
            for (const auto& h : heard)
            {
                const double d = glm::length(h.worldM - me);
                if (d <= reach && d < bestBeacon)
                {
                    bestBeacon = d;
                    r.beaconId = h.beacon->id;
                    r.hubName = h.beacon->name;
                }
            }
            if (!r.beaconId.empty())
            {
                r.phase = CapsuleRescue::Phase::Cruise;
                std::cerr << "[CapsuleRescue] no hub within reach - to the beacon " << r.hubName << ", " << bestBeacon / 1000.0
                          << " km, to wait for the rescue" << std::endl;
            }
        }
        if (r.hubId.empty() && r.beaconId.empty() && !r.reportedNoTarget)
        {
            r.reportedNoTarget = true;
            std::cerr << "[CapsuleRescue] no known hub within reach (reaction mass " << motion.capsuleDeltaVLeftMps
                      << " m/s): drifting, SOS" << std::endl;
        }
    }
    if (r.hubId.empty() && r.beaconId.empty())
    {
        m_pilotNotice = l10n::tr("cockpit.capsule.nothing_in_reach", "CAPSULE: no hubs or beacons in reach - drifting, SOS signal on");
        return;
    }
    glm::dvec3 staging(0.0), hubV(0.0), hubPos(0.0);
    if (!r.beaconId.empty())
    {
        // a beacon: its own point (it moves with its anchor; the velocity comes from its motion below)
        bool found = false;
        const auto resolve = routeAnchorResolver();
        for (const auto& b : game::navigation::beacons::capsuleBeacons())
            if (b.id == r.beaconId && resolve(b.anchor, staging))
                found = true;
        if (!found)
        {
            r.beaconId.clear();
            return;
        }
        hubPos = staging;
        hubV = r.stagingValid ? r.stagingVelocity : myV;
    }
    else
    {
        const auto hubIt = m_client->world().hubs().find(r.hubId);
        if (hubIt == m_client->world().hubs().end())
        {
            r.hubId.clear();
            return;
        }
        hubPos = world::coordinates::fullMeters(hubIt->second.worldPosition);
        hubV = hubIt->second.worldVelocityMps;
        // the STAGING point: 8 km above the hub (away from its planet) - where every approach to a city pad starts; from
        // a point in the hub's shadow (2.5 km off the station, the arrays between) the planner found no path at all
        // (live: every candidate through an obstacle, violation 1.0)
        glm::dvec3 up(0.0, 1.0, 0.0);
        if (const auto* celestial = m_client->celestialSnapshot(); celestial && celestial->systemId == hubIt->second.systemId)
            for (const auto& body : celestial->bodies)
                if (body.id == hubIt->second.parentBodyId && glm::length(hubPos - body.worldMeters) > 1.0)
                    up = glm::normalize(hubPos - body.worldMeters);
        staging = hubPos + up * StagingAboveHubM;
    }
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
        // the pad service flying / holding the capsule (ShipSnapshot::padService "phase:<n>:<frame>/<address>"; 3 = on the pad)
        if (ship.padService.rfind("phase:3:", 0) == 0)
        {
            if (!r.rescued)
            {
                r.rescued = true;
                std::cerr << "[CapsuleRescue] RESCUED: the capsule is on pad " << r.pad << " of " << r.hubName
                          << (r.rescueDock ? " (МЧС rescue dock)" : "") << ", reaction mass left "
                          << motion.capsuleDeltaVLeftMps << " m/s" << std::endl;
            }
            m_pilotNotice = r.rescueDock ? l10n::tr("cockpit.capsule.rescued_rescue_dock", {{"pad", r.pad}, {"hub", r.hubName}},
                                                     "CAPSULE RESCUED: pad {pad} - {hub} (rescue dock)")
                                         : l10n::tr("cockpit.capsule.rescued", {{"pad", r.pad}, {"hub", r.hubName}}, "CAPSULE RESCUED: pad {pad} - {hub}");
            return;
        }
        const bool landingAlive = pending.valid() || !m_padLandingObjectId.empty() || !ship.padService.empty();
        // the planner needs its time (the city's geometry, the route): the landing counts as ended only when nothing
        // is pending any more AND a minute and a half passed
        // a hand-over between the route and the pad service leaves a moment with neither: only 15 s of NOTHING is an end
        if (landingAlive)
            r.notAliveSince = -1.0;
        else if (r.notAliveSince < 0.0)
            r.notAliveSince = now;
        if (!landingAlive && now - r.landingAskedAt > 90.0 && now - r.notAliveSince > 15.0)
        {
            std::cerr << "[CapsuleRescue] the landing at " << r.hubName << " ended without the capsule down - next hub" << std::endl;
            r.refused.insert(r.hubId);
            r.hubId.clear();
            r.phase = CapsuleRescue::Phase::None;
            r.nextChoiceAt = now;
        }
        m_pilotNotice = r.rescueDock ? l10n::tr("cockpit.capsule.landing_rescue_dock", {{"pad", r.pad}, {"hub", r.hubName}},
                                                 "CAPSULE: landing on pad {pad} - {hub} (rescue dock)")
                                     : l10n::tr("cockpit.capsule.landing", {{"pad", r.pad}, {"hub", r.hubName}}, "CAPSULE: landing on pad {pad} - {hub}");
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
    if (!r.beaconId.empty() && d < StagingReachedM && glm::length(relV) < 5.0)
    {
        // at the beacon: keep station there and wait for the rescuers (the SOS is heard here)
        m_pilotNotice = l10n::tr("cockpit.capsule.at_beacon", {{"beacon", r.hubName}}, "CAPSULE: at beacon '{beacon}' - waiting for the rescuers, SOS signal on");
        if (!r.reportedAtBeacon)
        {
            r.reportedAtBeacon = true;
            std::cerr << "[CapsuleRescue] at the beacon " << r.hubName << ": holding, waiting for the rescue" << std::endl;
        }
    }
    else if (r.beaconId.empty() && d < StagingReachedM && glm::length(relV) < 5.0 && nearest > 500.0)
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
    const std::vector<l10n::MessageParam> course {{"target", r.hubName},
                                                  {"km", l10n::formatNumber(d / 1000.0, 0)},
                                                  {"closing", l10n::formatNumber(glm::dot(relV, dir), 0)},
                                                  {"reserve", l10n::formatNumber(motion.capsuleDeltaVLeftMps, 0)}};
    if (!r.reportedAtBeacon)   // waiting at a beacon keeps its own line
        m_pilotNotice = r.rescueDock
            ? l10n::tr("cockpit.capsule.course_rescue_dock", course,
                       "CAPSULE: heading for '{target}' (rescue dock) - {km} km, closing {closing} m/s, reserve {reserve} m/s")
            : l10n::tr("cockpit.capsule.course", course, "CAPSULE: heading for '{target}' - {km} km, closing {closing} m/s, reserve {reserve} m/s");
}
