// The onboard computer's journal on the CLIENT (navigation/rules/OnboardJournal.h; author 2026-11-24: «надо максимально
// все, что можно хранить на клиенте - хранить и делать на клиенте»). The journal is the player's knowledge: the client
// records what its ship meets from what it already receives (the hubs of its system, the celestial state) and keeps it
// in a file beside the game's other per-user data. The server never sees it.
//
// Every 2 s: each hub of the system within its comms reach (RescueRules: 30 light-seconds - the hub's identification is
// heard) is recorded with its inhabited zone and its docks (a МЧС rescue dock from the city plan), relative to the hub's
// parent body (an orbital hub moves with it). A failed onboard computer (motion.navComputerState 2) records nothing.
// One journal per ship (its instance id): a cockpit-capsule keeps its ship's computer.
#include "src/game/SpaceStateInternal.h"

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/rules/OnboardJournal.h"
#include "src/game/navigation/rules/RescueRules.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace journal = game::navigation::journal;

namespace
{
std::filesystem::path journalDir()
{
    namespace fs = std::filesystem;
    if (const char* base = std::getenv("LOCALAPPDATA"); base && *base)
        return fs::path(base) / "EliteGame" / "journal";
    return fs::path("journal");
}
} // namespace

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
        e.detail = std::string(rescueDock ? "док МЧС" : "без дока МЧС") +
                   (pads > 0 ? "; площадок " + std::to_string(pads) : std::string());
        e.recordedAtS = m_client->lastSimulationMetadata().universeTimeSeconds;
        e.hasRescueDock = rescueDock;
        if (journal::store(m_onboardJournal, e))
        {
            changed = true;
            std::cerr << "[Journal] recorded hub " << e.label << " (" << e.detail << ")" << std::endl;
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
