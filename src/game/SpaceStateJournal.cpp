// The onboard computer's journal on the CLIENT (navigation/rules/OnboardJournal.h; author 2026-11-24: «надо максимально
// все, что можно хранить на клиенте - хранить и делать на клиенте»). The journal is the player's knowledge: the client
// records what its ship meets from what it already receives (the hubs of its system, the celestial state) and keeps it
// in a file beside the game's other per-user data. The server never sees it.
//
// Every 2 s: each hub of the system within its comms reach (RescueRules: 30 light-seconds - the hub's identification is
// heard) is recorded with its inhabited zone and its docks (a МЧС rescue dock from the city plan), relative to the hub's
// parent body (an orbital hub moves with it). A failed onboard computer (motion.navComputerState 2) records nothing.
// One journal per ship (its instance id): a cockpit-capsule keeps its ship's computer.
#include "src/core/BuildFlavor.h"
#include "src/game/localization/LocalizationService.h"
#include "src/game/SpaceStateInternal.h"

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/rules/OnboardJournal.h"
#include "src/game/navigation/rules/RescueRules.h"
#include "src/game/navigation/rules/CapsuleBeacons.h"
#include "src/game/navigation/HubAttachedPose.h"

#include <algorithm>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace l10n = game::localization;

namespace journal = game::navigation::journal;

namespace
{
std::filesystem::path journalDir()
{
    namespace fs = std::filesystem;
    // ELITE_JOURNAL_DIR: the test runs keep their journals apart from the player's
    if (const char* dir = std::getenv("ELITE_JOURNAL_DIR"); dir && *dir)
        return fs::path(dir);
    if (const char* base = std::getenv("LOCALAPPDATA"); base && *base)
        return fs::path(base) / core::appDataFolderName() / "journal";
    return fs::path("journal");
}
} // namespace

// DEV: ELITE_DEV_HUB_POSE_CHECK=1 - every 5 s the client computes every hub-attached object's pose from its hub row
// (HubAttachedPose.h, the server's law) and logs the worst difference from the pose the server sent: the proof the
// objects can leave the wire (the client poses them itself)
void SpaceState::devCheckHubAttachedPoses(double dt)
{
    static const bool on = [] { const char* v = std::getenv("ELITE_DEV_HUB_POSE_CHECK"); return v && *v == '1'; }();
    if (!on || !m_client)
        return;
    m_hubPoseCheckClockS += dt;
    if (m_hubPoseCheckClockS < 5.0)
        return;
    m_hubPoseCheckClockS = 0.0;
    const double t = m_client->lastSimulationMetadata().universeTimeSeconds;
    int ownSystem = -1;
    if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
        ownSystem = it->second.transform.motion.systemId;
    double worstM = 0.0, worstDeg = 0.0;
    int n = 0;
    std::string worstId;
    std::vector<std::pair<double, std::string>> offenders;
    for (const auto& [oid, object] : m_client->world().objects())
    {
        if (!object.hubAttachment.valid || object.systemId != ownSystem)
            continue;
        const auto hubIt = m_client->world().hubs().find(object.hubAttachment.hubId);
        if (hubIt == m_client->world().hubs().end())
            continue;
        const auto& hub = hubIt->second;
        const auto frame = game::navigation::makeReplicatedHubKinematicFrame(
            hub.systemId, hub.id, world::coordinates::fullMeters(hub.worldPosition), hub.worldVelocityMps,
            hub.angularVelocityWorldRadPerSecond, hub.orientation);
        if (!frame.valid)
            continue;
        const auto pose = game::navigation::poseHubAttached(frame.originMeters, frame.linearVelocityMps, frame.localToWorldBasis[0],
                                                            frame.localToWorldBasis[1], frame.localToWorldBasis[2],
                                                            frame.angularVelocityWorldRadPerSecond, object.hubAttachment, t);
        if (!pose.valid)
            continue;
        ++n;
        const double dm = glm::length(pose.worldMeters - world::coordinates::fullMeters(object.worldPosition));
        const glm::dmat3 a(glm::mat3(pose.orientation)), b(glm::mat3(object.orientation));
        const double c = std::clamp((glm::dot(a[0], b[0]) + glm::dot(a[1], b[1]) + glm::dot(a[2], b[2]) - 1.0) * 0.5, -1.0, 1.0);
        const double deg = glm::degrees(std::acos(c));
        offenders.emplace_back(dm, object.hubAttachment.moduleId + (object.hubAttachment.inheritHubOrientation ? "" : " (own orientation)") +
                                       " " + std::to_string(static_cast<int>(deg)) + " deg");
        if (dm > worstM)
        {
            worstM = dm;
            worstId = object.hubAttachment.moduleId;
        }
        worstDeg = std::max(worstDeg, deg);
    }
    std::sort(offenders.begin(), offenders.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::cerr << "[HubPoseCheck] " << n << " attached objects of the system: worst position " << worstM << " m (" << worstId
              << "), worst orientation " << worstDeg << " deg; top:";
    for (std::size_t i = 0; i < offenders.size() && i < 3; ++i)
        std::cerr << " " << offenders[i].second << " " << offenders[i].first << " m;";
    std::cerr << std::endl;
}

void SpaceState::updateOnboardJournal(double dt)
{
    m_journalClockS += dt;
    if (m_journalClockS < 2.0 || !m_client)
        return;
    m_journalClockS = 0.0;
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
        return;
    const auto& ship = it->second;
    const auto& motion = ship.transform.motion;
    if (motion.navComputerState == 2)
        return;   // the computer failed: no records

    // the ship changed (bought, a session restored): load its computer's journal
    const std::uint64_t instance = static_cast<std::uint64_t>(ship.instanceId);
    if (instance != m_journalShipInstance)
    {
        m_journalShipInstance = instance;
        m_onboardJournal.clear();
        std::ifstream in(journalDir() / ("ship_" + std::to_string(instance) + ".txt"), std::ios::binary);
        if (in)
        {
            std::stringstream ss;
            ss << in.rdbuf();
            m_onboardJournal = journal::parse(ss.str());
        }
        std::cerr << "[Journal] ship instance " << instance << ": " << m_onboardJournal.size() << " records" << std::endl;
    }

    const glm::dvec3 me = world::coordinates::fullMeters(ship.transform.worldPosition);
    const auto* celestial = m_client->celestialSnapshot();
    bool changed = false;
    for (const auto& [hubId, hub] : m_client->world().hubs())
    {
        if (hub.systemId != motion.systemId)
            continue;
        const glm::dvec3 hubM = world::coordinates::fullMeters(hub.worldPosition);
        if (glm::length(hubM - me) > game::navigation::rescue::ZoneRadiusM)
            continue;
        glm::dvec3 anchor(0.0);
        std::string anchorId;
        if (celestial && celestial->systemId == hub.systemId)
            for (const auto& body : celestial->bodies)
                if (body.id == hub.parentBodyId)
                {
                    anchor = body.worldMeters;
                    anchorId = body.id;
                    break;
                }
        bool rescueDock = false;
        int pads = 0;
        if (const auto* plan = game::navigation::HubPlanCatalog::forHub(hubId))
            for (const auto& m : plan->modules())
            {
                rescueDock |= m.module == "hubs_rescue_dock";
                pads += m.pad ? 1 : 0;
            }
        journal::Entry e;
        e.kind = journal::Kind::Hub;
        e.key = hubId;
        e.systemId = hub.systemId;
        e.anchorBodyId = anchorId;
        e.positionM = hubM - anchor;
        e.radiusM = game::navigation::rescue::ZoneRadiusM;   // its comms reach = its inhabited zone
        e.label = hub.name.empty() ? hubId : hub.name;
        // the record's text in the UI language (a client-local journal; a language switch re-records it on the next pass)
        e.detail = pads > 0 ? (rescueDock ? l10n::tr("cockpit.journal.hub_rescue_dock_pads", {{"pads", std::to_string(pads)}}, "rescue dock; pads {pads}")
                                          : l10n::tr("cockpit.journal.hub_no_rescue_dock_pads", {{"pads", std::to_string(pads)}}, "no rescue dock; pads {pads}"))
                            : (rescueDock ? l10n::tr("cockpit.journal.hub_rescue_dock", "rescue dock")
                                          : l10n::tr("cockpit.journal.hub_no_rescue_dock", "no rescue dock"));
        e.recordedAtS = m_client->lastSimulationMetadata().universeTimeSeconds;
        e.hasRescueDock = rescueDock;
        if (journal::store(m_onboardJournal, e))
        {
            changed = true;
            std::cerr << "[Journal] recorded hub " << e.label << " (" << e.detail << ")" << std::endl;
        }
    }
    // the capsule beacons heard (CapsuleBeacons.h): their position (relative to their anchor's primary body) and the
    // rescue service they name
    {
        const auto resolve = routeAnchorResolver();
        for (const auto& b : game::navigation::beacons::capsuleBeacons())
        {
            glm::dvec3 p(0.0);
            if (b.systemId != motion.systemId || !resolve(b.anchor, p) || glm::length(p - me) > b.hearRadiusM)
                continue;
            glm::dvec3 anchor(0.0);
            std::string anchorId;
            if (celestial && celestial->systemId == b.systemId)
                for (const auto& body : celestial->bodies)
                    if (body.id == (b.anchor.kind == "lagrange" ? b.anchor.primary : b.anchor.id))
                    {
                        anchor = body.worldMeters;
                        anchorId = body.id;
                        break;
                    }
            journal::Entry e;
            e.kind = journal::Kind::Beacon;
            e.key = b.id;
            e.systemId = b.systemId;
            e.anchorBodyId = anchorId;
            e.positionM = p - anchor;
            e.radiusM = b.hearRadiusM;
            e.label = b.name;
            e.detail = l10n::tr("cockpit.journal.beacon_rescue_service", {{"service", b.operatorName.empty() ? b.rescueHubId : b.operatorName}},
                                "rescue service: {service}");
            e.recordedAtS = m_client->lastSimulationMetadata().universeTimeSeconds;
            if (journal::store(m_onboardJournal, e))
            {
                changed = true;
                std::cerr << "[Journal] recorded beacon " << e.label << std::endl;
            }
        }
    }
    if (changed)
    {
        std::error_code ec;
        std::filesystem::create_directories(journalDir(), ec);
        std::ofstream out(journalDir() / ("ship_" + std::to_string(instance) + ".txt"), std::ios::binary | std::ios::trunc);
        out << journal::format(m_onboardJournal);
    }
}
