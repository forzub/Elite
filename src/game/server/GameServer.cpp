#include "GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>


GameServer::GameServer(){
    m_simulation.update(0.0);
    m_simulation.setTick(0);

    m_lastSnapshot = m_simulation.snapshot();
}






void GameServer::update(double dt)
{
   
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
