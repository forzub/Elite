#include "GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>

#include "src/world/coordinates/WorldPosition.h"
#include <cmath>
#include <functional>
#include <unordered_map>

#include "src/world/celestial/SystemMapTypes.h"

GameServer::GameServer(){

        m_universeClock.reset();

        bool atlasLoaded =
        m_starAtlas.load(
            "assets/data/star_atlas/star_systems.json",
            "assets/data/star_atlas/system_details.json"
        );

        if (!atlasLoaded)
        {
            atlasLoaded =
                m_starAtlas.load(
                    "../assets/data/star_atlas/star_systems.json",
                    "../assets/data/star_atlas/system_details.json"
                );
        }

        m_playerNavigation.currentSystemId = 0;

        const auto* sol =
            m_starAtlas.findSystem(m_playerNavigation.currentSystemId);

        m_celestialRuntime.setSystem(sol);
        m_celestialRuntime.update(0.0);


    m_simulation.update(0.0);
    m_simulation.setTick(0);

    m_lastSnapshot = m_simulation.snapshot();
}






void GameServer::update(double dt)
{
    m_universeClock.update(dt);
    m_serverTick++;

    // 1. Применяем команды
    for (auto& [id, shipPtr] : m_simulation.ships())
    {
        Ship& ship = *shipPtr;
        auto it = m_pendingCommands.find(id.value);
        if (it == m_pendingCommands.end())
            continue;

        auto& queue = it->second;

        while (!queue.empty())
        {
            const auto& cmd = queue.front();

            ship.setControlState(cmd);
            queue.pop_front();
        }


        // --- Обработка событийных ShipCommand ---
        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt != m_pendingClientShipCommands.end())
        {
            auto& cmdQueue = cmdIt->second;

            while (!cmdQueue.empty())
            {
                const auto& shipCmd = cmdQueue.front();

                std::cout << "GameServer::update  - ClientShipCommand received: "
                        << shipCmd.type << "\n";

                ship.applyCommand(shipCmd);

                cmdQueue.pop_front();
            }
        }
    }


    m_simulation.update(dt);
    m_simulation.setTick(m_serverTick);




    const double universeTime =
        m_universeClock.timeSeconds();

    m_celestialRuntime.update(universeTime);

    if (const Ship* player = m_simulation.playerShip())
    {
        const auto& tr = player->core().transform();

        m_playerNavigation.currentSystemId = 0;
        m_playerNavigation.worldPosition = tr.worldPosition;
        m_playerNavigation.orientation = tr.orientation;

        m_playerNavigation.systemLocalMeters =
            world::coordinates::fullMeters(tr.worldPosition);

        m_playerNavigation.systemLocalAu =
            m_playerNavigation.systemLocalMeters /
            world::celestial::MetersPerAu;

        m_playerNavigation.forward = tr.forward();
        m_playerNavigation.up = tr.up();
    }











    if (m_serverTick % m_snapshotInterval == 0)
    {
        m_lastSnapshot = m_simulation.snapshot();
    }
}





void GameServer::submitCommand(EntityId id,const ShipControlState& control)
{
    m_pendingCommands[id.value].push_back(control);
}





void GameServer::receiveClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    
    switch (msg.type)
    {
        case game::network::ClientMessageType::ControlInput:
        {
            const auto& control =
                std::get<ShipControlState>(msg.payload);

            submitCommand(playerId, control);
            break;
        }

        case game::network::ClientMessageType::ClientShipCommand:
{
            
            const auto& cmd =
                std::get<ClientShipCommand>(msg.payload);

            m_pendingClientShipCommands[playerId.value].push_back(cmd);
            break;
        }
    }
}







void GameServer::debugRefreshSnapshot()
{
    // Debug panels are allowed to request heavy structural data.
    // This keeps regular published snapshots lightweight, while
    // structure_debug.html still receives modules/links on demand.
    m_simulation.debugForceFullShipGraphPayload();
    m_simulation.update(0.0);
    m_simulation.setTick(m_serverTick);
    m_lastSnapshot = m_simulation.snapshot();
}


const SimulationSnapshot& GameServer::snapshot() const
{
    // return m_simulation.snapshot();
    return m_lastSnapshot;
}

EntityId GameServer::playerId() const
{
    return m_simulation.playerId();
}

WorldParams& GameServer::world()
{
    return m_simulation.world();
}

bool GameServer::debugDestroyShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugDestroyShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugDestroyShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}


bool GameServer::debugSetShipStructuralLinkHealth(
    EntityId id,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    return m_simulation.debugSetShipStructuralLinkHealth(
        id,
        linkId,
        health,
        destroyed
    );
}


bool GameServer::debugDetachShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugDetachShipModule(id, moduleId);
}


bool GameServer::debugReattachShipModule(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok = m_simulation.debugReattachShipModule(id, moduleId);

    std::cout
        << "[GameServer] debugReattachShipModule entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}



bool GameServer::startShipRepairJob(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok =
        m_simulation.startShipRepairJob(id, moduleId);

    std::cout
        << "[GameServer] startShipRepairJob entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForMissingSlot(
    EntityId targetShipId,
    const std::string& targetModuleId
)
{
    const bool ok =
        m_simulation.startBestRepairJobForMissingSlot(
            targetShipId,
            targetModuleId
        );

    std::cout
        << "[GameServer] startBestRepairJobForMissingSlot shipId="
        << targetShipId.value
        << " targetModuleId="
        << targetModuleId
        << " ok="
        << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForFirstMissingSlot(EntityId targetShipId)
{
    const bool ok =
        m_simulation.startBestRepairJobForFirstMissingSlot(targetShipId);

    std::cout
        << "[GameServer] startBestRepairJobForFirstMissingSlot shipId="
        << targetShipId.value
        << " ok=" << ok
        << "\n";

    return ok;
}





bool GameServer::ejectShipCockpitCapsule(EntityId id)
{
    const bool ok = m_simulation.ejectShipCockpitCapsule(id);

    std::cout
        << "[GameServer] ejectShipCockpitCapsule entityId="
        << id.value
        << " ok=" << ok
        << "\n";

    return ok;
}




bool GameServer::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugHangShipModule(id, moduleId);
}

bool GameServer::debugReevaluateShipStructure(EntityId id)
{
    return m_simulation.debugReevaluateShipStructure(id);
}



bool GameServer::debugRestoreShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugRestoreShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugRestoreShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

bool GameServer::debugResetShipStructure(EntityId shipId)
{
    const bool ok = m_simulation.debugResetShipStructure(shipId);

    std::cout
        << "[GameServer] debugResetShipStructure entityId="
        << shipId.value
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

void GameServer::debugResetAllShipStructures()
{
    m_simulation.debugResetAllShipStructures();

    std::cout << "[GameServer] debugResetAllShipStructures\n";
}




namespace
{
    constexpr double AU_KM_LOCAL = 149597870.7;

    glm::vec4 fallbackBodyColor(
        world::celestial::BodyType type
    )
    {
        using world::celestial::BodyType;

        switch (type)
        {
            case BodyType::Star:
                return {1.00f, 0.93f, 0.62f, 1.00f};

            case BodyType::Planet:
                return {0.36f, 0.68f, 1.00f, 1.00f};

            case BodyType::Moon:
                return {0.70f, 0.72f, 0.78f, 1.00f};

            case BodyType::AsteroidBelt:
                return {0.55f, 0.55f, 0.55f, 0.70f};

            default:
                return {0.60f, 0.82f, 1.00f, 1.00f};
        }
    }

    std::string jurisdictionForSystemId(int systemId)
    {
        if (systemId == 0)
            return "Sol Authority";

        if (systemId >= 1 && systemId <= 9)
            return "Core Jurisdiction";

        if (systemId >= 10 && systemId <= 29)
            return "Colonial Administration";

        if (systemId >= 30 && systemId <= 44)
            return "Frontier / Independent";

        return "Unregistered";
    }

    double stablePhaseRadians(const std::string& id)
    {
        uint32_t h = 2166136261u;

        for (unsigned char c : id)
        {
            h ^= c;
            h *= 16777619u;
        }

        const double t =
            static_cast<double>(h % 10000u) / 10000.0;

        return t * glm::two_pi<double>();
    }
}



world::celestial::GalaxyMapSnapshot GameServer::buildGalaxyMapSnapshot() const
{
    world::celestial::GalaxyMapSnapshot out;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeDate =
        m_universeClock.dateTimeString();

    for (const auto& s : m_starAtlas.systems())
    {
        world::celestial::GalaxyMapSystem item;

        item.id = s.id;
        item.name = s.name;
        item.starType = s.starType;
        item.starsCount = s.starsCount;
        item.positionLy = s.positionLy;
        item.jurisdiction = jurisdictionForSystemId(s.id);

        out.systems.push_back(std::move(item));
    }

    return out;
}



world::celestial::SystemMapSnapshot
GameServer::buildSystemMapSnapshot(
    int systemId
) const
{
    world::celestial::SystemMapSnapshot out;

    const auto* system =
        m_starAtlas.findSystem(systemId);

    if (!system)
        return out;

    out.systemId = system->systemId;
    out.systemName = system->name;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeDate =
        m_universeClock.dateTimeString();

    for (const auto& body : system->bodies)
    {
        world::celestial::SystemMapBody item;

        item.id = body.id;
        item.name = body.name;
        item.parentId = body.parentId;
        item.type = body.type;

        item.radiusKm = body.radiusKm;

        item.positionAu =
            body.staticPositionAu;

        item.orbitCenterAu =
            glm::dvec3(0.0);

        item.orbitRadiusAu =
            body.distanceAu;

        item.drawOrbit =
            body.distanceAu > 0.0;

        for (const auto& ring : body.rings)
        {
            world::celestial::SystemMapRing r;

            r.innerRadiusKm =
                ring.innerRadiusKm;

            r.outerRadiusKm =
                ring.outerRadiusKm;

            item.rings.push_back(r);
        }

        out.bodies.push_back(
            std::move(item)
        );
    }

    return out;
}
